/**
 * @file granite_speech_backend.c
 * @brief Granite Speech (4.1-2b / 4.1-2b-plus) ASR backend via llama.cpp mtmd
 *
 * Replaces the former whisper.cpp and Vosk backends entirely (no
 * backward-compat path). Loads a Granite Speech GGUF (LLM decoder + Conformer
 * audio encoder weights) together with its companion mmproj GGUF (QFormer
 * audio projector) via llama.cpp's mtmd (multimodal) library, and transcribes
 * accumulated utterance audio to text using one of two fixed prompts,
 * selected by which variant is configured:
 *
 *   - ETHERVOX_STT_BACKEND_GRANITE_SPEECH       (BASE) - punctuated ASR/AST,
 *     used for Mode 1 (voice conversation) and Mode 4 (voice-to-text).
 *   - ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS  (PLUS) - Speaker-Attributed
 *     ASR (SAA), natively emitting "[Speaker N]: ..." tags. Used exclusively
 *     for Mode 2 (transcription) - replaces Whisper AND any separate
 *     diarization heuristic in one step.
 *
 * IBM's model card is explicit that an unfamiliar or tool-calling prompt is
 * simply ignored, falling back to plain transcription - so this backend only
 * ever sends the fixed ASR/SAA prompt strings below. The Governor (a
 * separate llama_model instance - see src/governor/governor.c) is the only
 * component that performs reasoning/tool-calling, always on text produced
 * here, never directly on audio.
 *
 * Each finalize() call is one-shot: the KV cache is reset in start() and the
 * whole accumulated utterance/chunk is tokenized (audio + prompt) and decoded
 * in a single pass. Mode 2's cross-chunk continuity comes from
 * config.prefix_text (Granite Speech's own incremental-decoding mechanism),
 * not from carrying the KV cache across calls.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ethervox/stt.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include "ethervox/chat_template.h"
#include "ethervox/granite_speech_decode.h"
#include "ethervox/model_pool.h"  // N6.3: Pool integration

#if defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE && defined(MTMD_AVAILABLE) && MTMD_AVAILABLE

#include "llama.h"
#include "ggml.h"
#include "mtmd.h"

#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...)  ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

static bool g_granite_llama_backend_initialized = false;

typedef struct {
  struct llama_model* model;
  struct llama_context* ctx;
  mtmd_context* mctx;
  const chat_template_t* tmpl;  // CHAT_TEMPLATE_GRANITE - shared with the Governor's format
  int audio_sample_rate;        // From mtmd_get_audio_sample_rate(), expected 16000
  // N6.3: Pool integration
  ethervox_model_pool_t* pool;          // weak ref, NULL = legacy direct loading
  ethervox_model_handle_t* model_handle; // owned, unload via pool when non-NULL
} granite_speech_context_t;

static void granite_speech_log_callback(enum ggml_log_level level, const char* text, void* user_data) {
  (void)user_data;
  if (!text) return;
  // Only surface warnings/errors - llama.cpp's INFO/DEBUG stream is very
  // verbose during GGUF loading and would otherwise flood the app log.
  if (level == GGML_LOG_LEVEL_ERROR) {
    LOG_ERROR("[llama.cpp] %s", text);
  } else if (level == GGML_LOG_LEVEL_WARN) {
    LOG_WARN("[llama.cpp] %s", text);
  }
}

// GGML_ASSERT failures (e.g. a shape mismatch building the clip/mtmd graph) call
// ggml_abort(), which by default only fprintf()s to stderr - invisible on Android unless
// the process happens to redirect stderr to logcat. Without this, a SIGABRT tombstone names
// only the function it happened in, never the failed assertion or tensor shapes. Routing it
// through our own logger makes the actual "GGML_ASSERT(...) failed" text (with file:line)
// appear in logcat before the abort(), same as any other native crash investigation.
static void granite_speech_abort_callback(const char* error_message) {
  LOG_ERROR("[FATAL] ggml_abort: %s", error_message ? error_message : "(no message)");
}

ethervox_result_t ethervox_stt_granite_speech_init(ethervox_stt_runtime_t* runtime) {
  if (!runtime->config.model_path || runtime->config.model_path[0] == '\0') {
    LOG_ERROR("Granite Speech: model_path is required");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  if (!runtime->config.mmproj_path || runtime->config.mmproj_path[0] == '\0') {
    LOG_ERROR("Granite Speech: mmproj_path is required (audio projector GGUF - "
              "Granite Speech always ships as a (model, mmproj) pair, never one file)");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  granite_speech_context_t* gs = (granite_speech_context_t*)calloc(1, sizeof(granite_speech_context_t));
  if (!gs) {
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }

  // N6.3: Store pool reference if provided
  gs->pool = runtime->config.pool;

  if (!g_granite_llama_backend_initialized) {
    ggml_log_set(granite_speech_log_callback, NULL);
    llama_log_set(granite_speech_log_callback, NULL);
    ggml_set_abort_callback(granite_speech_abort_callback);
    llama_backend_init();
    ggml_backend_load_all();
    if (ggml_backend_reg_count() == 0) {
      LOG_ERROR("Granite Speech: no ggml backends loaded, cannot load model");
      free(gs);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    g_granite_llama_backend_initialized = true;
  }

  // N6.3: Load through pool if available (Android with N6.2 complete),
  // otherwise fall back to direct loading (iOS, other platforms)
  if (gs->pool) {
    LOG_INFO("Granite Speech: loading via model pool for budget enforcement");
    
    ethervox_model_config_t pool_config = {
      .model_path = runtime->config.model_path,
      .mmproj_path = runtime->config.mmproj_path,  // Pool handles mmproj loading
      // mtmd_tokenize() splits prompts on this marker (see granite_speech_decode.c's
      // fixed prompts) - mtmd's own default ("<__media__>") never matches "<|audio|>",
      // which silently made every decode fail with "number of bitmaps does not
      // match number of markers" (rc=1).
      .media_marker = "<|audio|>",
      .context_size = 4096,
      .n_threads = 4,
      .use_gpu = (runtime->config.n_gpu_layers > 0),
      .role = "speech",  // Role for pool tracking
      .n_seq_max = 3,    // Typical for multimodal models
    };
    
    // shared_context so a handle already resident under a different role
    // (e.g. the governor's "main" role, same GGUF) is reused instead of
    // loading a second copy of the weights (C3.6a).
    ethervox_result_t pool_result = ethervox_model_pool_load_shared_context(
      gs->pool, &pool_config, NULL, NULL, &gs->model_handle
    );
    
    if (ethervox_is_error(pool_result)) {
      LOG_ERROR("Granite Speech: pool load failed with error %d", pool_result);
      free(gs);
      return pool_result;
    }
    
    // Extract model, context, and mtmd from pool handle
    gs->model = ethervox_model_handle_get_model(gs->model_handle);
    gs->ctx = ethervox_model_handle_get_context(gs->model_handle);
    gs->mctx = ethervox_model_handle_get_mtmd(gs->model_handle);
    
    if (!gs->model || !gs->ctx || !gs->mctx) {
      LOG_ERROR("Granite Speech: pool returned NULL components");
      ethervox_model_pool_unload(gs->pool, gs->model_handle);
      free(gs);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    LOG_INFO("Granite Speech: loaded via pool successfully");
  } else {
    // Legacy direct loading path (iOS, other platforms without pool)
    LOG_INFO("Granite Speech: loading directly (no pool configured)");
    
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = runtime->config.n_gpu_layers;

    gs->model = llama_model_load_from_file(runtime->config.model_path, model_params);
    if (!gs->model) {
      LOG_ERROR("Granite Speech: failed to load model from %s", runtime->config.model_path);
      free(gs);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    struct llama_context_params ctx_params = llama_context_default_params();
    // Granite Speech's own model card documents evaluation on segments up to
    // ~9 minutes. 4096 tokens comfortably covers one utterance's audio tokens
    // + prompt + generated transcript for a single one-shot call.
    ctx_params.n_ctx = 4096;
    ctx_params.n_batch = 512;
    ctx_params.n_ubatch = 512;
    ctx_params.no_perf = true;

    gs->ctx = llama_init_from_model(gs->model, ctx_params);
    if (!gs->ctx) {
      LOG_ERROR("Granite Speech: failed to create llama context");
      llama_model_free(gs->model);
      free(gs);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    struct mtmd_context_params mtmd_params = mtmd_context_params_default();
    mtmd_params.use_gpu = (runtime->config.n_gpu_layers != 0);
    mtmd_params.print_timings = false;
    mtmd_params.n_threads = ctx_params.n_threads;
    // mtmd_tokenize() splits prompts on ctx->media_marker, which is a plain field
    // assignment from this param (mtmd_context_params_default() leaves it at
    // "<__media__>", NOT the GGUF's own marker - there is no metadata-driven
    // override in mtmd_context's constructor). granite_speech_decode.c's prompts
    // use the literal "<|audio|>" token, so it must match here or mtmd_tokenize()
    // fails with "number of bitmaps does not match number of markers" (rc=1).
    mtmd_params.media_marker = "<|audio|>";

    gs->mctx = mtmd_init_from_file(runtime->config.mmproj_path, gs->model, mtmd_params);
    if (!gs->mctx) {
      LOG_ERROR("Granite Speech: failed to load mmproj from %s", runtime->config.mmproj_path);
      llama_free(gs->ctx);
      llama_model_free(gs->model);
      free(gs);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
  }

  if (!mtmd_support_audio(gs->mctx)) {
    LOG_ERROR("Granite Speech: loaded mmproj does not report audio support - wrong file?");
    mtmd_free(gs->mctx);
    llama_free(gs->ctx);
    llama_model_free(gs->model);
    free(gs);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  gs->audio_sample_rate = mtmd_get_audio_sample_rate(gs->mctx);
  if (gs->audio_sample_rate > 0 && (uint32_t)gs->audio_sample_rate != runtime->config.sample_rate) {
    LOG_WARN("Granite Speech: model expects %d Hz audio, config requests %u Hz - "
             "audio capture must resample to match", gs->audio_sample_rate, runtime->config.sample_rate);
  }

  // Granite Speech shares the same Granite 4.1 chat template family as the
  // Governor (see src/governor/chat_template.c) - reuse it directly instead
  // of re-declaring the "<|start_of_role|>...<|end_of_role|>" wrapper here.
  gs->tmpl = chat_template_get(CHAT_TEMPLATE_GRANITE, NULL);

  runtime->backend_context = gs;
  LOG_INFO("Granite Speech backend initialized (%s variant)",
           runtime->config.backend == ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS ? "PLUS" : "BASE");
  return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_stt_granite_speech_start(ethervox_stt_runtime_t* runtime) {
  granite_speech_context_t* gs = (granite_speech_context_t*)runtime->backend_context;
  if (!gs) return ETHERVOX_ERROR_NOT_INITIALIZED;

  // Reset the KV cache so each utterance/chunk starts from a clean context -
  // Granite Speech is invoked one-shot per finalize() call, not as a
  // persistent multi-turn chat session (see file header).
  llama_memory_t mem = llama_get_memory(gs->ctx);
  if (mem) {
    llama_memory_clear(mem, true);
  }
  return ETHERVOX_SUCCESS;
}

// Granite Speech has no incremental/partial-result path (unlike Whisper's
// streaming design) - audio is simply accumulated here and transcribed in
// one pass when ethervox_stt_granite_speech_finalize() is called.
ethervox_result_t ethervox_stt_granite_speech_process(ethervox_stt_runtime_t* runtime,
                                                       const ethervox_audio_buffer_t* audio_buffer,
                                                       ethervox_stt_result_t* result) {
  if (!runtime->backend_context) return ETHERVOX_ERROR_NOT_INITIALIZED;

  uint32_t space_left = runtime->accumulator_size - runtime->accumulator_write_pos;
  uint32_t to_copy = audio_buffer->size < space_left ? audio_buffer->size : space_left;
  if (to_copy > 0) {
    memcpy(runtime->audio_accumulator + runtime->accumulator_write_pos,
           audio_buffer->data, to_copy * sizeof(float));
    runtime->accumulator_write_pos += to_copy;
  }
  if (to_copy < audio_buffer->size) {
    LOG_WARN("Granite Speech: audio accumulator full (%u/%u samples), dropping %u samples - "
             "caller should finalize() more often for this mode",
             runtime->accumulator_write_pos, runtime->accumulator_size,
             audio_buffer->size - to_copy);
  }

  // No partial result to report - Mode 1/4 UI live captions come from the
  // Governor's own streaming, not from STT partials.
  result->is_partial = false;
  result->is_final = false;
  return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_stt_granite_speech_finalize(ethervox_stt_runtime_t* runtime,
                                                        ethervox_stt_result_t* result) {
  granite_speech_context_t* gs = (granite_speech_context_t*)runtime->backend_context;
  if (!gs) return ETHERVOX_ERROR_NOT_INITIALIZED;

  if (runtime->accumulator_write_pos == 0) {
    LOG_WARN("Granite Speech: finalize() called with no accumulated audio");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  bool is_plus = (runtime->config.backend == ETHERVOX_STT_BACKEND_GRANITE_SPEECH_PLUS);

  char* transcript = NULL;
  ethervox_result_t decode_result = granite_speech_decode(
      gs->model, gs->ctx, gs->mctx, gs->tmpl, /*seq_id=*/0,
      runtime->audio_accumulator, runtime->accumulator_write_pos,
      /*is_saa=*/is_plus, /*prefix_text=*/runtime->config.prefix_text,
      /*max_tokens=*/runtime->config.max_transcript_tokens, &transcript);
  if (!ethervox_is_success(decode_result)) {
    // Ready for the next utterance/chunk even on failure - otherwise the next process() call
    // accumulates on top of this attempt's still-unreset write position, so a session that fails
    // once and is reused (BUG: found via a_session_can_transcribe_more_than_once, real crash on
    // the second call) mixes stale audio into the next attempt instead of starting clean.
    runtime->accumulator_write_pos = 0;
    return decode_result;
  }

  result->text = transcript;  // ownership transferred to caller (ethervox_stt_result_free)
  result->confidence = 1.0f;   // Granite Speech does not expose a per-utterance confidence score
  result->is_partial = false;
  result->is_final = true;
  result->language = runtime->config.language;

  runtime->accumulator_write_pos = 0;  // Ready for the next utterance/chunk
  return ETHERVOX_SUCCESS;
}

void ethervox_stt_granite_speech_stop(ethervox_stt_runtime_t* runtime) {
  (void)runtime;
  // No per-session teardown needed - the model/context stay resident across
  // start()/stop() cycles (reloading a ~1-2GB GGUF per utterance would be far
  // too slow); only ethervox_stt_granite_speech_cleanup() releases them.
}

void ethervox_stt_granite_speech_cleanup(ethervox_stt_runtime_t* runtime) {
  granite_speech_context_t* gs = (granite_speech_context_t*)runtime->backend_context;
  if (!gs) return;

  // N6.3: Route cleanup through pool if model was loaded that way
  if (gs->pool && gs->model_handle) {
    LOG_INFO("Granite Speech: unloading via model pool");
    ethervox_model_pool_unload(gs->pool, gs->model_handle);
    gs->model_handle = NULL;
    gs->model = NULL;
    gs->ctx = NULL;
    gs->mctx = NULL;
  } else {
    // Legacy direct cleanup (iOS, other platforms)
    if (gs->mctx) mtmd_free(gs->mctx);
    if (gs->ctx) llama_free(gs->ctx);
    if (gs->model) llama_model_free(gs->model);
  }
  
  free(gs);
  runtime->backend_context = NULL;
}

ethervox_result_t ethervox_stt_granite_speech_release_projector(ethervox_stt_runtime_t* runtime) {
  granite_speech_context_t* gs = (granite_speech_context_t*)runtime->backend_context;
  if (!gs) return ETHERVOX_ERROR_NOT_INITIALIZED;

  if (!gs->pool || !gs->model_handle) {
    LOG_ERROR("Granite Speech: release_projector requires pool-backed loading (no pool configured)");
    return ETHERVOX_ERROR_NOT_SUPPORTED;
  }

  ethervox_result_t result = ethervox_model_pool_detach_projector(gs->pool, gs->model_handle);
  if (ethervox_is_error(result)) {
    return result;
  }
  // The handle's own mtmd_ctx is now NULL (detach_projector's job) - refresh our cached copy so
  // finalize()/transcribe calls fail fast with NOT_INITIALIZED rather than use a freed pointer.
  gs->mctx = ethervox_model_handle_get_mtmd(gs->model_handle);
  LOG_INFO("Granite Speech: projector released, model stays resident");
  return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_stt_granite_speech_reattach_projector(ethervox_stt_runtime_t* runtime) {
  granite_speech_context_t* gs = (granite_speech_context_t*)runtime->backend_context;
  if (!gs) return ETHERVOX_ERROR_NOT_INITIALIZED;

  if (!gs->pool || !gs->model_handle) {
    LOG_ERROR("Granite Speech: reattach_projector requires pool-backed loading (no pool configured)");
    return ETHERVOX_ERROR_NOT_SUPPORTED;
  }
  if (gs->mctx) {
    return ETHERVOX_SUCCESS;  // Already attached - no-op, matching detach's own no-op contract.
  }

  // Same media_marker as the original load (see ethervox_stt_granite_speech_init) - mtmd's own
  // default never matches granite_speech_decode.c's literal "<|audio|>" token.
  ethervox_result_t result = ethervox_model_pool_attach_projector(
      gs->pool, gs->model_handle, runtime->config.mmproj_path, "<|audio|>");
  if (ethervox_is_error(result)) {
    return result;
  }
  gs->mctx = ethervox_model_handle_get_mtmd(gs->model_handle);
  if (!gs->mctx) {
    LOG_ERROR("Granite Speech: reattach_projector succeeded but handle has no mtmd context");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  LOG_INFO("Granite Speech: projector re-attached without reloading the model");
  return ETHERVOX_SUCCESS;
}

#else  // !(LLAMA_CPP_AVAILABLE && MTMD_AVAILABLE)

// Stub implementation for builds without llama.cpp/mtmd (e.g. mtmd not yet
// vendored). Fails loudly rather than silently no-opping, so a missing build
// dependency is caught immediately instead of masquerading as "no speech
// detected".
ethervox_result_t ethervox_stt_granite_speech_init(ethervox_stt_runtime_t* runtime) {
  (void)runtime;
  ETHERVOX_LOG_ERROR("Granite Speech backend requires LLAMA_CPP_AVAILABLE and MTMD_AVAILABLE - "
                     "rebuild with llama.cpp mtmd support (see cmake/FetchDependencies.cmake)");
  return ETHERVOX_ERROR_NOT_SUPPORTED;
}
ethervox_result_t ethervox_stt_granite_speech_start(ethervox_stt_runtime_t* runtime) {
  (void)runtime;
  return ETHERVOX_ERROR_NOT_SUPPORTED;
}
ethervox_result_t ethervox_stt_granite_speech_process(ethervox_stt_runtime_t* runtime,
                                                       const ethervox_audio_buffer_t* audio_buffer,
                                                       ethervox_stt_result_t* result) {
  (void)runtime; (void)audio_buffer; (void)result;
  return ETHERVOX_ERROR_NOT_SUPPORTED;
}
ethervox_result_t ethervox_stt_granite_speech_finalize(ethervox_stt_runtime_t* runtime,
                                                        ethervox_stt_result_t* result) {
  (void)runtime; (void)result;
  return ETHERVOX_ERROR_NOT_SUPPORTED;
}
void ethervox_stt_granite_speech_stop(ethervox_stt_runtime_t* runtime) { (void)runtime; }
void ethervox_stt_granite_speech_cleanup(ethervox_stt_runtime_t* runtime) { (void)runtime; }
ethervox_result_t ethervox_stt_granite_speech_release_projector(ethervox_stt_runtime_t* runtime) {
  (void)runtime;
  return ETHERVOX_ERROR_NOT_SUPPORTED;
}
ethervox_result_t ethervox_stt_granite_speech_reattach_projector(ethervox_stt_runtime_t* runtime) {
  (void)runtime;
  return ETHERVOX_ERROR_NOT_SUPPORTED;
}

#endif  // LLAMA_CPP_AVAILABLE && MTMD_AVAILABLE
