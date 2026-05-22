/**
 * @file phonemizer.h
 * @brief Custom phonemizer for text-to-IPA conversion
 * 
 * GPL-free phonemizer implementation for Piper TTS backend.
 * Supports English, Chinese, and German.
 */

#ifndef ETHERVOX_PHONEMIZER_H
#define ETHERVOX_PHONEMIZER_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Supported languages for phonemization
 */
typedef enum {
    PHONEMIZER_LANG_UNKNOWN = 0,
    PHONEMIZER_LANG_EN_US,      // English (American)
    PHONEMIZER_LANG_EN_GB,      // English (British)
    PHONEMIZER_LANG_ZH_CN,      // Chinese (Mandarin, Simplified)
    PHONEMIZER_LANG_DE_DE,      // German (Standard)
    PHONEMIZER_LANG_ES_MX,      // Spanish (Mexican)
    PHONEMIZER_LANG_ES_419,     // Spanish (Latin America)
} phonemizer_language_t;

/**
 * Opaque phonemizer context
 */
typedef struct phonemizer_context phonemizer_t;

/**
 * Create phonemizer for specified language
 * 
 * @param lang_code Language identifier (e.g., "en-us", "zh", "de")
 * @return Phonemizer context or NULL on failure
 */
phonemizer_t* phonemizer_create(const char* lang_code);

/**
 * Convert text to IPA phoneme string
 * 
 * @param ctx Phonemizer context
 * @param text Input text (UTF-8)
 * @param ipa_output Buffer for IPA output (UTF-8)
 * @param max_len Maximum output buffer size
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 * 
 * Output format: Space-separated IPA tokens with stress markers
 * Example: "hello world" → "h ə ˈ l o ʊ   w ɝ l d"
 */
ethervox_result_t phonemizer_text_to_ipa(phonemizer_t* ctx,
                           const char* text,
                           char* ipa_output,
                           size_t max_len);

/**
 * Get phonemizer language
 */
phonemizer_language_t phonemizer_get_language(phonemizer_t* ctx);

/**
 * Get pronunciation override store (for saving trained pronunciations)
 */
void* phonemizer_get_override_store(phonemizer_t* ctx);

/**
 * Destroy phonemizer and free resources
 */
void phonemizer_destroy(phonemizer_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PHONEMIZER_H
