/**
 * @file language_detector.h
 * @brief Simple heuristic-based language detection for TTS
 * 
 * Provides fallback language detection when Whisper detection is unavailable
 * (e.g., text-only input modes). Uses character pattern analysis.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_LANGUAGE_DETECTOR_H
#define ETHERVOX_LANGUAGE_DETECTOR_H

#include <stddef.h>
#include "ethervox/settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detect language from text using heuristics
 * 
 * Primary method: Use Whisper's detection from ethervox_stt_result_t.language
 * Fallback method (this function): Character pattern analysis
 * 
 * Detection rules:
 * - CJK characters → "zh" (Chinese)
 * - German umlauts/ß → "de" (German)
 * - Spanish accents → "es" (Spanish)
 * - Default → "en" (English)
 * 
 * @param text Text to analyze (UTF-8 encoded)
 * @return Two-letter ISO 639-1 language code ("en", "zh", "de", "es")
 *         Returns "en" if detection fails or text is NULL
 * 
 * @note This is a simple heuristic fallback. For accurate detection,
 *       always prefer Whisper's language detection from STT results.
 * 
 * @example
 * ```c
 * const char* detected = ethervox_detect_language("Guten Tag");
 * // Returns "de"
 * 
 * const char* with_whisper = stt_result->language ? stt_result->language : 
 *                           ethervox_detect_language(text);
 * ```
 */
const char* ethervox_detect_language(const char* text);

/**
 * @brief Get voice ID for detected language from settings
 * 
 * Maps language code to appropriate voice setting field:
 * - "en" / "en-*" → settings->tts.voice_en
 * - "zh" / "cmn" → settings->tts.voice_zh
 * - "de" → settings->tts.voice_de
 * - "es" → settings->tts.voice_es
 * - default → settings->tts.voice_en (English fallback)
 * 
 * @param language Two-letter ISO language code or Whisper language string
 * @param settings Persistent settings containing voice preferences
 * @return Voice ID string (e.g., "en_US-libritts-high")
 * 
 * @example
 * ```c
 * const char* lang = stt_result->language ? stt_result->language : 
 *                    ethervox_detect_language(llm_response);
 * const char* voice = ethervox_get_voice_for_language(lang, &settings);
 * // Use voice to configure TTS
 * ```
 */
const char* ethervox_get_voice_for_language(const char* language, 
                                            const ethervox_persistent_settings_t* settings);

/**
 * @brief Detect language and switch TTS voice automatically
 * 
 * Two-tier detection:
 * 1. Use last_detected_language if available (from Whisper STT)
 * 2. Fallback to heuristic text analysis via ethervox_detect_language()
 * 
 * Automatically reloads TTS with appropriate voice if language changed.
 * Thread-safe - loads settings internally.
 * 
 * @param text Text to analyze (if last_detected_language is empty)
 * @param last_detected_language Last language detected by STT (or NULL/empty for fallback)
 * @param tts_context Pointer to TTS context pointer (will be updated if reloaded), can be NULL
 * @return Detected language code (\"en\", \"zh\", \"de\", \"es\")
 * 
 * @example
 * ```c
 * // In conversation callback:
 * const char* lang = ethervox_detect_and_switch_voice(
 *     llm_response,
 *     session->last_detected_language,  // From STT
 *     (void**)&session->tts_context     // Will update if voice changed
 * );
 * printf(\"Speaking in %s\\n\", lang);
 * ```
 */
const char* ethervox_detect_and_switch_voice(const char* text,
                                            const char* last_detected_language,
                                            void** tts_context);

/**
 * @brief Switch TTS voice to a specific language
 * 
 * Directly loads the TTS voice for the specified language without detection.
 * Used when the language is already known (e.g., from explicit parameter).
 * 
 * @param language Target language code ("en", "de", "es", "zh")
 * @param tts_context Pointer to TTS context pointer (will be updated if reloaded), can be NULL
 * @return Same as input language, or "en" on error
 * 
 * @example
 * ```c
 * // Switch to German voice explicitly
 * ethervox_switch_to_language("de", (void**)&session->tts_context);
 * ```
 */
const char* ethervox_switch_to_language(const char* language, void** tts_context);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_LANGUAGE_DETECTOR_H
