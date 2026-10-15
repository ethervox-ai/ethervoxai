/**
 * @file vision.c
 * @brief Image-to-text generation over a multimodal model (see vision.h).
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/vision.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/logging.h"

#if defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE && defined(MTMD_AVAILABLE) && MTMD_AVAILABLE

#include "llama.h"
#include "mtmd-helper.h"
#include "mtmd.h"

// IBM's reference examples cap generation at 8192 new tokens (max_new_tokens=8192).
#define VISION_DEFAULT_MAX_TOKENS 8192
#define VISION_BATCH 512

// Granite Docling's chat format (its chat_template.jinja): user turn = image + instruction, closed
// by <|end_of_text|>, then the assistant tag the model continues from.
#define VISION_PROMPT_PREFIX "<|start_of_role|>user<|end_of_role|>"
#define VISION_PROMPT_SUFFIX "<|end_of_text|>\n<|start_of_role|>assistant<|end_of_role|>"

static ethervox_result_t decode_locked(
    ethervox_model_handle_t* handle, const void* image, size_t image_size, const char* prompt,
    uint32_t max_tokens, const ethervox_cancel_token_t* cancel, char** out_text) {
  struct llama_model* model = ethervox_model_handle_get_model(handle);
  struct llama_context* ctx = ethervox_model_handle_get_context(handle);
  mtmd_context* mctx = ethervox_model_handle_get_mtmd(handle);
  if (!model || !ctx || !mctx || !mtmd_support_vision(mctx)) {
    ETHERVOX_LOG_ERROR("[Vision] model handle has no vision projector");
    return ETHERVOX_ERROR_NOT_SUPPORTED;
  }

  mtmd_bitmap* bitmap = mtmd_helper_bitmap_init_from_buf(mctx, (const unsigned char*)image, image_size);
  if (!bitmap) {
    ETHERVOX_LOG_ERROR("[Vision] could not decode the image (%zu bytes)", image_size);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  size_t prompt_cap = strlen(VISION_PROMPT_PREFIX) + strlen(mtmd_default_marker()) + strlen(prompt) +
                      strlen(VISION_PROMPT_SUFFIX) + 1;
  char* text = (char*)malloc(prompt_cap);
  if (!text) {
    mtmd_bitmap_free(bitmap);
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }
  snprintf(text, prompt_cap, "%s%s%s%s", VISION_PROMPT_PREFIX, mtmd_default_marker(), prompt,
           VISION_PROMPT_SUFFIX);

  mtmd_input_text input_text = {.text = text, .add_special = true, .parse_special = true};
  mtmd_input_chunks* chunks = mtmd_input_chunks_init();
  const mtmd_bitmap* bitmaps[1] = {bitmap};
  int32_t tok_rc = mtmd_tokenize(mctx, chunks, &input_text, bitmaps, 1);
  free(text);
  if (tok_rc != 0) {
    ETHERVOX_LOG_ERROR("[Vision] mtmd_tokenize failed (rc=%d)", tok_rc);
    mtmd_input_chunks_free(chunks);
    mtmd_bitmap_free(bitmap);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Nothing carries over between calls: each page/image starts from an empty KV cache.
  llama_memory_clear(llama_get_memory(ctx), true);

  llama_pos n_past = 0;
  int32_t eval_rc = mtmd_helper_eval_chunks(mctx, ctx, chunks, 0, 0, VISION_BATCH, true, &n_past);
  mtmd_input_chunks_free(chunks);
  mtmd_bitmap_free(bitmap);
  if (eval_rc != 0) {
    ETHERVOX_LOG_ERROR("[Vision] mtmd_helper_eval_chunks failed (rc=%d)", eval_rc);
    return ETHERVOX_ERROR_FAILED;
  }

  const struct llama_vocab* vocab = llama_model_get_vocab(model);
  struct llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
  llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

  size_t cap = 8192;
  size_t len = 0;
  char* out = (char*)calloc(1, cap);
  if (!out) {
    llama_sampler_free(sampler);
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }

  ethervox_result_t result = ETHERVOX_SUCCESS;
  if (max_tokens == 0) max_tokens = VISION_DEFAULT_MAX_TOKENS;
  llama_pos pos = n_past;
  for (uint32_t i = 0; i < max_tokens; i++) {
    if (cancel && ethervox_cancel_token_is_cancelled(cancel)) {
      result = ETHERVOX_ERROR_INTERRUPTED;
      break;
    }
    // The model's context (8192 positions for granite-docling) holds the prompt, the image tokens
    // and the answer; stop at its end and keep what was read rather than failing the page.
    if (pos >= (llama_pos)llama_n_ctx(ctx)) break;
    llama_token token = llama_sampler_sample(sampler, ctx, -1);
    if (llama_vocab_is_eog(vocab, token)) break;

    char piece[256];
    int n = llama_token_to_piece(vocab, token, piece, sizeof(piece), 0, true);
    if (n > 0) {
      if (len + (size_t)n + 1 > cap) {
        size_t new_cap = cap * 2;
        char* grown = (char*)realloc(out, new_cap);
        if (!grown) {
          result = ETHERVOX_ERROR_OUT_OF_MEMORY;
          break;
        }
        out = grown;
        cap = new_cap;
      }
      memcpy(out + len, piece, (size_t)n);
      len += (size_t)n;
      out[len] = '\0';
    }

    llama_batch batch = llama_batch_init(1, 0, 1);
    batch.n_tokens = 1;
    batch.token[0] = token;
    batch.pos[0] = pos;
    batch.n_seq_id[0] = 1;
    batch.seq_id[0][0] = 0;
    batch.logits[0] = true;
    int rc = llama_decode(ctx, batch);
    llama_batch_free(batch);
    if (rc != 0) {
      ETHERVOX_LOG_ERROR("[Vision] llama_decode failed (rc=%d)", rc);
      result = ETHERVOX_ERROR_FAILED;
      break;
    }
    pos++;
  }
  llama_sampler_free(sampler);

  if (result != ETHERVOX_SUCCESS) {
    free(out);
    return result;
  }
  *out_text = out;
  return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_vision_generate(
    ethervox_model_handle_t* handle, const void* image, size_t image_size, const char* prompt,
    uint32_t max_tokens, const ethervox_cancel_token_t* cancel, char** out_text) {
  if (!handle || !image || image_size == 0 || !prompt || !out_text) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  *out_text = NULL;
  ethervox_model_handle_lock(handle);
  ethervox_result_t result = decode_locked(handle, image, image_size, prompt, max_tokens, cancel, out_text);
  ethervox_model_handle_unlock(handle);
  return result;
}

#else  // no llama.cpp / mtmd in this build

ethervox_result_t ethervox_vision_generate(
    ethervox_model_handle_t* handle, const void* image, size_t image_size, const char* prompt,
    uint32_t max_tokens, const ethervox_cancel_token_t* cancel, char** out_text) {
  (void)handle; (void)image; (void)image_size; (void)prompt; (void)max_tokens; (void)cancel;
  if (out_text) *out_text = NULL;
  return ETHERVOX_ERROR_NOT_SUPPORTED;
}

#endif

void ethervox_vision_text_free(char* text) { free(text); }
