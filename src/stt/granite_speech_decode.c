/**
 * @file granite_speech_decode.c
 * @brief Shared Granite Speech ASR/SAA decode routine implementation.
 *
 * Extracted verbatim (no behavior change) from
 * src/stt/granite_speech_backend.c's ethervox_stt_granite_speech_finalize() -
 * see granite_speech_decode.h for the shared-helper rationale.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 */

#include "ethervox/granite_speech_decode.h"

#if defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE && defined(MTMD_AVAILABLE) && MTMD_AVAILABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "ggml.h"
#include "mtmd-helper.h"
#include "ethervox/logging.h"

#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

// TASK-N7.7: temporary encode/prefill/decode split timing - measurement only, no
// behaviour change, removed by N7.8. Off unless ETHERVOX_LATENCY_TRACE is defined.
#ifdef ETHERVOX_LATENCY_TRACE
static double n7_7_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}
#define LOG_LATENCY(...) ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
static inline double n7_7_now_ms(void) { return 0.0; }
#define LOG_LATENCY(...) ((void)0)
#endif

// Fixed prompts, verbatim from IBM's granite-speech-4.1-2b-plus model card -
// see granite_speech_backend.c's file header for the "why fixed prompts"
// rationale (mode switching is entirely prompt-driven).
#define GRANITE_SPEECH_ASR_PROMPT \
  "<|audio|> can you transcribe the speech into a written format?"
#define GRANITE_SPEECH_SAA_PROMPT \
  "<|audio|> Speaker attribution: Transcribe and denote who is speaking by adding " \
  "[Speaker 1]: and [Speaker 2]: tags before speaker turns."
#define GRANITE_SPEECH_SYSTEM_PROMPT \
  "You are Granite, developed by IBM. You are a helpful AI assistant"

#define GRANITE_SPEECH_ASR_MAX_TOKENS 256
#define GRANITE_SPEECH_SAA_MAX_TOKENS 1024
#define GRANITE_SPEECH_PROMPT_BUF_SIZE 2048

ethervox_result_t granite_speech_decode(
    struct llama_model* model,
    struct llama_context* ctx,
    mtmd_context* mctx,
    const chat_template_t* tmpl,
    llama_seq_id seq_id,
    const float* samples,
    uint32_t n_samples,
    bool is_saa,
    const char* prefix_text,
    uint32_t max_tokens,
    char** out_text) {
  if (!model || !ctx || !mctx || !tmpl || !samples || n_samples == 0 || !out_text) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  *out_text = NULL;

  const char* task_prompt = is_saa ? GRANITE_SPEECH_SAA_PROMPT : GRANITE_SPEECH_ASR_PROMPT;
  if (max_tokens == 0) {
    max_tokens = is_saa ? GRANITE_SPEECH_SAA_MAX_TOKENS : GRANITE_SPEECH_ASR_MAX_TOKENS;
  }

  // Build the full prompt: system + user (containing the "<|audio|>" marker)
  // + assistant generation-prompt tag (+ prefix_text, if carrying forward a
  // Mode 2 incremental-decoding session - matches the model's own Jinja
  // template, which appends prefix_text immediately after the assistant tag).
  char prompt[GRANITE_SPEECH_PROMPT_BUF_SIZE];
  int written = snprintf(prompt, sizeof(prompt), "%s%s%s%s%s%s%s%s",
                          tmpl->system_start, GRANITE_SPEECH_SYSTEM_PROMPT, tmpl->system_end,
                          tmpl->user_start, task_prompt, tmpl->user_end,
                          tmpl->assistant_start,
                          prefix_text ? prefix_text : "");
  if (written < 0 || (size_t)written >= sizeof(prompt)) {
    LOG_ERROR("Granite Speech: prompt buffer too small (needed %d bytes)", written);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  mtmd_bitmap* bitmap = mtmd_bitmap_init_from_audio(n_samples, samples);
  if (!bitmap) {
    LOG_ERROR("Granite Speech: failed to construct audio bitmap from %u samples", n_samples);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  const double n7_7_encode_start_ms = n7_7_now_ms();
  mtmd_input_text input_text = {.text = prompt, .add_special = true, .parse_special = true};
  mtmd_input_chunks* chunks = mtmd_input_chunks_init();
  const mtmd_bitmap* bitmaps[1] = {bitmap};
  int32_t tok_rc = mtmd_tokenize(mctx, chunks, &input_text, bitmaps, 1);
  LOG_LATENCY("N7.7 encode(tokenize): wall=%.1fms n_samples=%u",
              n7_7_now_ms() - n7_7_encode_start_ms, n_samples);
  if (tok_rc != 0) {
    LOG_ERROR("Granite Speech: mtmd_tokenize failed (rc=%d)", tok_rc);
    mtmd_input_chunks_free(chunks);
    mtmd_bitmap_free(bitmap);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // mtmd_helper_eval_chunks() runs the audio encoder and prefills the text prompt in
  // one call - llama.cpp/mtmd does not expose a finer split than this.
  const double n7_7_prefill_start_ms = n7_7_now_ms();
  llama_pos new_n_past = 0;
  int32_t eval_rc = mtmd_helper_eval_chunks(mctx, ctx, chunks, /*n_past=*/0, seq_id,
                                            /*n_batch=*/512, /*logits_last=*/true, &new_n_past);
  LOG_LATENCY("N7.7 prefill(audio_encode+eval): wall=%.1fms new_n_past=%d",
              n7_7_now_ms() - n7_7_prefill_start_ms, new_n_past);
  mtmd_input_chunks_free(chunks);
  mtmd_bitmap_free(bitmap);
  if (eval_rc != 0) {
    LOG_ERROR("Granite Speech: mtmd_helper_eval_chunks failed (rc=%d)", eval_rc);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Autoregressive greedy decoding of the transcript, one token at a time -
  // mirrors the pattern used for the Governor's own generation loop in
  // src/governor/governor.c. Greedy (temp=0) matches the decoding IBM/
  // llama.cpp validated their token-for-token reference match against.
  const struct llama_vocab* vocab = llama_model_get_vocab(model);
  struct llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
  llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.0f));
  llama_sampler_chain_add(sampler, llama_sampler_init_dist(0));

  char* transcript = (char*)calloc(1, 8192);
  size_t transcript_len = 0;
  size_t transcript_cap = 8192;
  llama_pos pos = new_n_past;
  const double n7_7_decode_start_ms = n7_7_now_ms();
  uint32_t n7_7_tokens_generated = 0;

  for (uint32_t i = 0; i < max_tokens; i++) {
    n7_7_tokens_generated = i + 1;
    llama_token next_token = llama_sampler_sample(sampler, ctx, -1);
    if (llama_vocab_is_eog(vocab, next_token)) {
      break;
    }

    char piece[128];
    int n_chars = llama_token_to_piece(vocab, next_token, piece, sizeof(piece), 0, false);
    if (n_chars > 0) {
      if (transcript_len + (size_t)n_chars + 1 > transcript_cap) {
        transcript_cap *= 2;
        char* grown = (char*)realloc(transcript, transcript_cap);
        if (!grown) {
          LOG_ERROR("Granite Speech: out of memory growing transcript buffer");
          break;
        }
        transcript = grown;
      }
      memcpy(transcript + transcript_len, piece, (size_t)n_chars);
      transcript_len += (size_t)n_chars;
      transcript[transcript_len] = '\0';

      if (chat_template_has_stop_sequence(tmpl, transcript)) {
        break;
      }
    }

    llama_batch batch = llama_batch_init(1, 0, 1);
    batch.n_tokens = 1;
    batch.token[0] = next_token;
    batch.pos[0] = pos;
    batch.n_seq_id[0] = 1;
    batch.seq_id[0][0] = seq_id;
    batch.logits[0] = true;
    int decode_rc = llama_decode(ctx, batch);
    llama_batch_free(batch);
    if (decode_rc != 0) {
      LOG_ERROR("Granite Speech: llama_decode failed during generation (rc=%d)", decode_rc);
      break;
    }
    pos++;
  }

  llama_sampler_free(sampler);
  LOG_LATENCY("N7.7 decode: wall=%.1fms tokens=%u",
              n7_7_now_ms() - n7_7_decode_start_ms, n7_7_tokens_generated);

  // Strip any trailing stop-sequence text that leaked into the buffer before
  // the check above caught it (e.g. "<|end_of_text|>" itself is 1-3 tokens).
  for (int i = 0; i < tmpl->stop_sequence_count; i++) {
    const char* stop = tmpl->stop_sequences[i];
    if (!stop) continue;
    char* found = strstr(transcript, stop);
    if (found) {
      *found = '\0';
      transcript_len = strlen(transcript);
    }
  }
  (void)transcript_len;

  *out_text = transcript;  // ownership transferred to caller
  return ETHERVOX_SUCCESS;
}

#endif  // LLAMA_CPP_AVAILABLE && MTMD_AVAILABLE
