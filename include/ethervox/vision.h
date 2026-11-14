/**
 * @file vision.h
 * @brief Image-to-text generation over a multimodal (mmproj) model, e.g. granite-docling-258M.
 *
 * Closes the gap media.h documents: it could query capabilities and prepare media but had no
 * entry point that decodes a response against an attached image. This is that entry point.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_VISION_H
#define ETHERVOX_VISION_H

#include <stddef.h>
#include <stdint.h>

#include "ethervox/cancel_token.h"
#include "ethervox/error.h"
#include "ethervox/model_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generates text from one image and an instruction, greedily, on sequence 0 of the handle's own
 * context (cleared first; nothing carries over between calls).
 *
 * @param handle     Model handle loaded with an mmproj_path (vision-capable)
 * @param image      Encoded image bytes (PNG, JPEG, ...) as mtmd can decode
 * @param image_size Size of `image` in bytes
 * @param prompt     The instruction placed after the image, e.g. "Convert this page to docling."
 * @param max_tokens Cap on generated tokens (0 = 8192, IBM's reference cap; generation also stops at the end of the context and returns what was read)
 * @param cancel     Optional; checked between tokens
 * @param out_text   Receives a malloc'd, NUL-terminated string the caller frees with
 *                   ethervox_vision_text_free()
 * @return ETHERVOX_SUCCESS; ETHERVOX_ERROR_NOT_SUPPORTED if the handle has no vision projector;
 *         ETHERVOX_ERROR_INTERRUPTED if cancelled; another error otherwise
 */
ethervox_result_t ethervox_vision_generate(
    ethervox_model_handle_t* handle,
    const void* image,
    size_t image_size,
    const char* prompt,
    uint32_t max_tokens,
    const ethervox_cancel_token_t* cancel,
    char** out_text
);

/** Frees a string returned by ethervox_vision_generate(). NULL is ignored. */
void ethervox_vision_text_free(char* text);

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_VISION_H */
