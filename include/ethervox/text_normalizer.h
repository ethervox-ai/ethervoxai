/**
 * @file text_normalizer.h
 * @brief Text normalization for TTS (numbers, times, abbreviations)
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_TEXT_NORMALIZER_H
#define ETHERVOX_TEXT_NORMALIZER_H

#include "ethervox/error.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Normalize text for TTS
 * Converts numbers, times, and symbols to speakable text
 *
 * Examples:
 *   "The time is 07:46" → "The time is seven forty-six"
 *   "I have 5 apples" → "I have five apples"
 *   "Call (555) 123-4567" → "Call five five five one two three four five six seven"
 *
 * @param input Input text with numbers/times
 * @param output Buffer for normalized text
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_tts_normalize_text(const char* input, char* output, size_t output_size);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_TEXT_NORMALIZER_H
