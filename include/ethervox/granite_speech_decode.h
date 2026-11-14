/**
 * @file granite_speech_decode.h
 * @brief Shared Granite Speech ASR/SAA decode routine.
 *
 * Extracted from src/stt/granite_speech_backend.c's
 * ethervox_stt_granite_speech_finalize() so the same prompt-build +
 * mtmd-tokenize + greedy-decode logic can be reused by both:
 *  - src/stt/granite_speech_backend.c (Mode 2 standalone context, seq_id=0)
 *  - src/governor/governor.c's ethervox_governor_transcribe_audio() (shared
 *    Governor context with chat generation, seq_id=2 - see
 *    docs/UNIFIED_VOICE_MODEL_ARCHITECTURE.md)
 *
 * No behavior change from the original inline implementation - purely a
 * parameterization on (model, ctx, mctx, seq_id) instead of assuming a
 * private granite_speech_context_t.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 */
#ifndef ETHERVOX_GRANITE_SPEECH_DECODE_H
#define ETHERVOX_GRANITE_SPEECH_DECODE_H

#include <stdint.h>
#include <stdbool.h>
#include "ethervox/error.h"
#include "ethervox/chat_template.h"

#if defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE && defined(MTMD_AVAILABLE) && MTMD_AVAILABLE

#include "llama.h"
#include "mtmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Shared Granite Speech ASR/SAA decode routine: builds the fixed ASR or SAA
 * prompt, tokenizes prompt+audio via mtmd, evaluates into the given
 * llama_context at the given sequence, then greedily decodes the transcript.
 *
 * Does NOT clear the target sequence itself - callers must do that first
 * (via llama_memory_seq_rm or llama_memory_clear as appropriate for their
 * context's sequence layout) since the right way to clear differs between
 * a single-sequence standalone context (Mode 2) and a multi-sequence shared
 * context (Governor's audio-capable path).
 *
 * @param model llama_model (vocab source)
 * @param ctx llama_context to decode into
 * @param mctx mtmd_context (must support audio - see mtmd_support_audio())
 * @param tmpl Chat template (CHAT_TEMPLATE_GRANITE - shared format)
 * @param seq_id Target sequence for prompt+audio tokens and generation
 * @param samples Raw float32 PCM audio, 16kHz mono
 * @param n_samples Sample count
 * @param is_saa true for Speaker-Attributed ASR prompt (Mode 2/PLUS), false
 *   for plain ASR-only prompt (Mode 1 unified voice / Mode 4)
 * @param prefix_text Optional incremental-decoding carry-over (Mode 2 only,
 *   NULL for Mode 1/4)
 * @param max_tokens Cap on generated transcript tokens (0 = backend default:
 *   256 for ASR-only, 1024 for SAA)
 * @param out_text Output: malloc'd transcript (caller must free), set to
 *   NULL on failure
 * @return ETHERVOX_SUCCESS or error code
 */
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
    char** out_text);

#ifdef __cplusplus
}
#endif

#endif  // LLAMA_CPP_AVAILABLE && MTMD_AVAILABLE

#endif  // ETHERVOX_GRANITE_SPEECH_DECODE_H
