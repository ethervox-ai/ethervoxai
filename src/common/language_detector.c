/**
 * @file language_detector.c
 * @brief Simple heuristic-based language detection for TTS
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/language_detector.h"
#include "ethervox/settings.h"
#include "ethervox/logging.h"
#include "ethervox/dialogue.h"
#include "ethervox/tts.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

/**
 * Check if character is in CJK (Chinese, Japanese, Korean) Unicode range
 */
static bool is_cjk_char(unsigned int codepoint) {
    return (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||    // CJK Unified Ideographs
           (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||    // CJK Extension A
           (codepoint >= 0x20000 && codepoint <= 0x2A6DF) ||  // CJK Extension B
           (codepoint >= 0x2A700 && codepoint <= 0x2B73F) ||  // CJK Extension C
           (codepoint >= 0x2B740 && codepoint <= 0x2B81F) ||  // CJK Extension D
           (codepoint >= 0x2B820 && codepoint <= 0x2CEAF) ||  // CJK Extension E
           (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||    // CJK Compatibility Ideographs
           (codepoint >= 0x3040 && codepoint <= 0x309F) ||    // Hiragana
           (codepoint >= 0x30A0 && codepoint <= 0x30FF);      // Katakana
}

/**
 * Decode UTF-8 character and return codepoint
 * Returns 0 on error
 */
static unsigned int decode_utf8(const char** text_ptr) {
    const unsigned char* text = (const unsigned char*)*text_ptr;
    unsigned int codepoint = 0;
    int bytes = 0;
    
    if (text[0] < 0x80) {
        // 1-byte ASCII
        codepoint = text[0];
        bytes = 1;
    } else if ((text[0] & 0xE0) == 0xC0) {
        // 2-byte
        codepoint = ((text[0] & 0x1F) << 6) | (text[1] & 0x3F);
        bytes = 2;
    } else if ((text[0] & 0xF0) == 0xE0) {
        // 3-byte
        codepoint = ((text[0] & 0x0F) << 12) | ((text[1] & 0x3F) << 6) | (text[2] & 0x3F);
        bytes = 3;
    } else if ((text[0] & 0xF8) == 0xF0) {
        // 4-byte
        codepoint = ((text[0] & 0x07) << 18) | ((text[1] & 0x3F) << 12) | 
                    ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
        bytes = 4;
    }
    
    *text_ptr += bytes;
    return codepoint;
}

/**
 * Check if character is a word boundary
 */
static inline bool is_word_boundary(char c) {
    return c == '\0' || c == ' ' || c == ',' || c == '.' || c == '!' || 
           c == '?' || c == ';' || c == ':' || c == '\n' || c == '\t' ||
           c == '(' || c == ')' || c == '[' || c == ']' || c == '"' || c == '\'';
}

/**
 * Case-insensitive substring search
 */
static const char* stristr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return haystack;
    
    for (const char* p = haystack; *p != '\0'; p++) {
        if (tolower((unsigned char)*p) == tolower((unsigned char)*needle)) {
            // Potential match - check rest of string
            size_t i;
            for (i = 0; i < needle_len; i++) {
                if (tolower((unsigned char)p[i]) != tolower((unsigned char)needle[i])) {
                    break;
                }
            }
            if (i == needle_len) {
                return p;  // Found match
            }
        }
    }
    return NULL;
}

// Abandoning this approach for now - too many false positives 2026-01-30
const char* ethervox_detect_language(const char* text) {
    if (!text || text[0] == '\0') {
        return "en";  // Default to English
    }
    
    // Performance optimization: only analyze first sentence (up to first .!? followed by space/end)
    // This is sufficient for language detection and much faster for long texts
    char first_sentence[512] = {0};
    size_t i = 0;
    bool found_sentence_end = false;
    
    for (i = 0; i < sizeof(first_sentence) - 1 && text[i] != '\0'; i++) {
        first_sentence[i] = text[i];
        // Check for sentence-ending punctuation followed by space or end
        if ((text[i] == '.' || text[i] == '!' || text[i] == '?') && 
            (text[i+1] == ' ' || text[i+1] == '\0' || text[i+1] == '\n')) {
            first_sentence[i+1] = '\0';
            found_sentence_end = true;
            break;
        }
    }
    
    if (!found_sentence_end && i == sizeof(first_sentence) - 1) {
        // Truncated - ensure null termination
        first_sentence[sizeof(first_sentence) - 1] = '\0';
    }
    
    // Use first sentence (or truncated text) for analysis
    text = first_sentence;
    
    // Language detection scores
    int cjk_count = 0;
    int german_markers = 0;
    int spanish_markers = 0;
    int total_chars = 0;
    
    // Check for common English words
    const char* english_words[] = {"the", "be", "to", "of", "and", "a", "in", "that", "have", "it",
                                    "for", "not", "on", "with", "he", "as", "you", "do", "at", "this",
                                    "but", "his", "by", "from", "they", "we", "say", "her", "she", "or",
                                    "an", "will", "my", "one", "all", "would", "there", "their", NULL};
    int english_word_count = 0;
    
    for (const char** word = english_words; *word != NULL; word++) {
        size_t word_len = strlen(*word);
        const char* pos = text;
        
        // Search for word with proper boundaries (case-insensitive)
        while ((pos = stristr(pos, *word)) != NULL) {
            bool is_start_boundary = (pos == text) || is_word_boundary(*(pos - 1));
            bool is_end_boundary = is_word_boundary(pos[word_len]);
            
            if (is_start_boundary && is_end_boundary) {
                english_word_count++;
                ETHERVOX_LOG_DEBUG("[Language Detection] English word match: '%s' in text", *word);
                break;
            }
            pos++;
        }
    }
    
    // Check for common German words as additional signal
    // IMPORTANT: Must match complete words with boundaries on BOTH sides to avoid false positives
    // (e.g., "und" not as substring in "found", "der" not in "wonder")
    const char* german_words[] = {"ich", "und", "der", "die", "das", "ist", 
                                   "nicht", "sich", "auf", "für", "mit", "nach", 
                                   "bei", "über", "möchte", "guten", "auch", "jetzt", 
                                   "hallo", "wie", "von", "wir", "sehr", "gut", 
                                   "haben", "werden", "dich", NULL};
    int german_word_count = 0;
    
    for (const char** word = german_words; *word != NULL; word++) {
        size_t word_len = strlen(*word);
        const char* pos = text;
        
        // Search for word with proper boundaries (case-insensitive)
        while ((pos = stristr(pos, *word)) != NULL) {
            // Check if it's a complete word (boundaries on both sides)
            bool is_start_boundary = (pos == text) || is_word_boundary(*(pos - 1));
            bool is_end_boundary = is_word_boundary(pos[word_len]);
            
            if (is_start_boundary && is_end_boundary) {
                german_word_count++;
                ETHERVOX_LOG_DEBUG("[Language Detection] German word match: '%s' in text", *word);
                break;  // Count each word only once
            }
            pos++;  // Continue searching
        }
    }
    
    // Check for common Spanish words
    const char* spanish_words[] = {"hola", "esta", "bienvenido", "gracias", "señor", "señora",
                                    "donde", "cuando", "como", "porque", "también", "español",
                                    "que", "con", "para", "por", "una", "hacer", "todo", "bueno", NULL};
    int spanish_word_count = 0;
    for (const char** word = spanish_words; *word != NULL; word++) {
        if (strstr(text, *word) != NULL) {
            spanish_word_count++;
        }
    }
    
    const char* ptr = text;
    while (*ptr) {
        unsigned int codepoint = decode_utf8(&ptr);
        if (codepoint == 0) break;
        
        total_chars++;
        
        // Chinese/Japanese/Korean detection
        if (is_cjk_char(codepoint)) {
            cjk_count++;
        }
        
        // German-specific characters: ä, ö, ü, Ä, Ö, Ü, ß
        if (codepoint == 0xE4 || codepoint == 0xF6 || codepoint == 0xFC ||  // ä, ö, ü
            codepoint == 0xC4 || codepoint == 0xD6 || codepoint == 0xDC ||  // Ä, Ö, Ü
            codepoint == 0xDF) {                                             // ß
            german_markers++;
        }
        
        // Spanish-specific accented characters: á, é, í, ó, ú, ñ, Á, É, Í, Ó, Ú, Ñ, ¿, ¡
        if (codepoint == 0xE1 || codepoint == 0xE9 || codepoint == 0xED ||  // á, é, í
            codepoint == 0xF3 || codepoint == 0xFA || codepoint == 0xF1 ||  // ó, ú, ñ
            codepoint == 0xC1 || codepoint == 0xC9 || codepoint == 0xCD ||  // Á, É, Í
            codepoint == 0xD3 || codepoint == 0xDA || codepoint == 0xD1 ||  // Ó, Ú, Ñ
            codepoint == 0xBF || codepoint == 0xA1) {                       // ¿, ¡
            spanish_markers++;
        }
    }
    
    if (total_chars == 0) {
        return "en";
    }
    
    ETHERVOX_LOG_DEBUG("[Language Detection] Counts: english_words=%d, german_markers=%d, german_words=%d, spanish_markers=%d, spanish_words=%d, cjk=%d, total=%d",
                      english_word_count, german_markers, german_word_count, spanish_markers, spanish_word_count, cjk_count, total_chars);
    
    // Decision logic: score-based with CJK priority
    // CJK takes priority (most distinctive)
    if (cjk_count > 0 && (cjk_count * 100 / total_chars) > 10) {
        return "zh";  // Chinese (also covers Japanese)
    }
    
    // Calculate scores for all languages
    // Score = (character markers * 2) + (word count * 1)
    // Character markers weighted higher as they're more distinctive
    // English has no special markers, so only word count
    int english_score = english_word_count;
    int german_score = (german_markers * 2) + german_word_count;
    int spanish_score = (spanish_markers * 2) + spanish_word_count;
    
    ETHERVOX_LOG_DEBUG("[Language Detection] Scores: en=%d (%d words), de=%d (%d markers, %d words), es=%d (%d markers, %d words)",
                      english_score, english_word_count,
                      german_score, german_markers, german_word_count,
                      spanish_score, spanish_markers, spanish_word_count);
    
    // Return language with highest score (minimum threshold: 2)
    int max_score = english_score;
    const char* detected_lang = "en";
    
    if (german_score > max_score) {
        max_score = german_score;
        detected_lang = "de";
    }
    
    if (spanish_score > max_score) {
        max_score = spanish_score;
        detected_lang = "es";
    }
    
    // Require minimum confidence (at least 2 points)
    if (max_score < 2) {
        ETHERVOX_LOG_DEBUG("[Language Detection] All scores below threshold (max=%d), defaulting to 'en'", max_score);
        return "en";
    }
    
    ETHERVOX_LOG_DEBUG("[Language Detection] Returning '%s' (score: %d)", detected_lang, max_score);
    return detected_lang;
}

const char* ethervox_get_voice_for_language(const char* language, 
                                            const ethervox_persistent_settings_t* settings) {
    if (!language || !settings) {
        return settings ? settings->tts.voice_en : NULL;
    }
    
    // Map language codes to voice settings
    // Handle both ISO 639-1 (2-letter) and Whisper's extended codes
    
    if (strncmp(language, "en", 2) == 0) {
        // English (en, en-US, en-GB, etc.)
        return settings->tts.voice_en;
    } else if (strcmp(language, "zh") == 0 || strcmp(language, "cmn") == 0 || 
               strncmp(language, "zh-", 3) == 0) {
        // Chinese (zh, cmn=Mandarin, zh-CN, zh-TW, etc.)
        return settings->tts.voice_zh;
    } else if (strncmp(language, "de", 2) == 0) {
        // German (de, de-DE, de-AT, de-CH)
        return settings->tts.voice_de;
    } else if (strncmp(language, "es", 2) == 0) {
        // Spanish (es, es-ES, es-MX, es-419)
        return settings->tts.voice_es;
    }
    
    // Fallback to English for unsupported languages
    return settings->tts.voice_en;
}

const char* ethervox_detect_and_switch_voice(const char* text,
                                            const char* last_detected_language,
                                            void** tts_context) {
    if (!text) {
        return "en";  // Default
    }
    
    // Priority 1: Use STT-detected language from Whisper if available
    const char* detected_language = NULL;
    if (last_detected_language && last_detected_language[0] != '\0') {
        detected_language = last_detected_language;
        ETHERVOX_LOG_INFO("[Language Detection] Using STT-detected language: %s", detected_language);
    }
    
    // Priority 2: Fallback to heuristic text-based detection
    if (!detected_language) {
        detected_language = ethervox_detect_language(text);
        ETHERVOX_LOG_INFO("[Language Detection] Heuristic detection: %s (text: %.50s...)",
                         detected_language, text);
    }
    
    // Load settings to check and update voice
    ethervox_persistent_settings_t settings;
    if (ethervox_is_error(ethervox_settings_load(&settings, NULL))) {
        ETHERVOX_LOG_ERROR("[Language Detection] Failed to load settings");
        return detected_language;
    }
    
    // Get target voice for detected language
    const char* target_voice = ethervox_get_voice_for_language(detected_language, &settings);
    if (!target_voice) {
        ETHERVOX_LOG_ERROR("[Language Detection] No voice configured for language: %s", detected_language);
        return detected_language;
    }
    
    ETHERVOX_LOG_DEBUG("[Language Detection] Target voice for %s: %s", detected_language, target_voice);
    
    // Determine what voice is currently loaded by checking piper_model_path
    // Extract voice ID from path like "/path/to/en_US-libritts-high.onnx"
    const char* current_model = settings.tts.piper_model_path;
    const char* last_slash = strrchr(current_model, '/');
    if (!last_slash) last_slash = current_model;
    else last_slash++; // Skip the '/'
    
    ETHERVOX_LOG_DEBUG("[Language Detection] Current model filename: %s", last_slash);
    
    // Check if target voice is in the current model path
    bool voice_needs_change = (strstr(last_slash, target_voice) == NULL);
    
    ETHERVOX_LOG_DEBUG("[Language Detection] Voice needs change? %s (checking if '%s' contains '%s')",
                       voice_needs_change ? "YES" : "NO", last_slash, target_voice);
    
    if (voice_needs_change) {
        ETHERVOX_LOG_INFO("[Language Switch] Changing TTS voice: %s → %s (language: %s)",
                         last_slash, target_voice, detected_language);
        
        // Update settings voice field for the detected language
        if (strcmp(detected_language, "en") == 0) {
            strncpy(settings.tts.voice_en, target_voice, sizeof(settings.tts.voice_en) - 1);
        } else if (strcmp(detected_language, "zh") == 0) {
            strncpy(settings.tts.voice_zh, target_voice, sizeof(settings.tts.voice_zh) - 1);
        } else if (strcmp(detected_language, "de") == 0) {
            strncpy(settings.tts.voice_de, target_voice, sizeof(settings.tts.voice_de) - 1);
        } else if (strcmp(detected_language, "es") == 0) {
            strncpy(settings.tts.voice_es, target_voice, sizeof(settings.tts.voice_es) - 1);
        }
        
        // Reconstruct piper_model_path from target voice
        const char* home = getenv("HOME");
        if (home) {
            snprintf(settings.tts.piper_model_path,
                    sizeof(settings.tts.piper_model_path),
                    "%s/.ethervox/models/piper/%s.onnx",
                    home, target_voice);
        } else {
            snprintf(settings.tts.piper_model_path,
                    sizeof(settings.tts.piper_model_path),
                    ".ethervox/models/piper/%s.onnx",
                    target_voice);
        }
        
        ETHERVOX_LOG_DEBUG("[Language Switch] New model path: %s", settings.tts.piper_model_path);
        
        // Reload TTS with new voice
        if (ethervox_reload_global_tts(&settings.tts, NULL, NULL) == 0) {
            ETHERVOX_LOG_INFO("[Language Switch] ✅ TTS reloaded with %s voice", detected_language);
            
            // Update caller's TTS context pointer if provided
            if (tts_context) {
                extern ethervox_tts_context_t* g_global_tts;
                *tts_context = g_global_tts;
            }
        } else {
            ETHERVOX_LOG_ERROR("[Language Switch] ❌ Failed to reload TTS");
        }
    } else {
        ETHERVOX_LOG_DEBUG("[Language Switch] Voice already correct for language %s: %s",
                          detected_language, target_voice);
    }
    
    return detected_language;
}
const char* ethervox_switch_to_language(const char* language, void** tts_context) {
    if (!language) {
        return "en";  // Default
    }
    
    ETHERVOX_LOG_INFO("[Language Switch] Switching to explicit language: %s", language);
    
    // Load settings to check and update voice
    ethervox_persistent_settings_t settings;
    if (ethervox_is_error(ethervox_settings_load(&settings, NULL))) {
        ETHERVOX_LOG_ERROR("[Language Switch] Failed to load settings");
        return language;
    }
    
    // Get target voice for specified language
    const char* target_voice = ethervox_get_voice_for_language(language, &settings);
    if (!target_voice) {
        ETHERVOX_LOG_ERROR("[Language Switch] No voice configured for language: %s", language);
        return language;
    }
    
    ETHERVOX_LOG_DEBUG("[Language Switch] Target voice for %s: %s", language, target_voice);
    
    // Determine what voice is currently loaded by checking piper_model_path
    // Extract voice ID from path like "/path/to/en_US-libritts-high.onnx"
    const char* current_model = settings.tts.piper_model_path;
    const char* last_slash = strrchr(current_model, '/');
    if (!last_slash) last_slash = current_model;
    else last_slash++; // Skip the '/'
    
    ETHERVOX_LOG_DEBUG("[Language Switch] Current model filename: %s", last_slash);
    
    // Check if target voice is in the current model path
    bool voice_needs_change = (strstr(last_slash, target_voice) == NULL);
    
    ETHERVOX_LOG_DEBUG("[Language Switch] Voice needs change? %s (checking if '%s' contains '%s')",
                       voice_needs_change ? "YES" : "NO", last_slash, target_voice);
    
    if (voice_needs_change) {
        ETHERVOX_LOG_INFO("[Language Switch] Changing TTS voice: %s → %s (language: %s)",
                         last_slash, target_voice, language);
        
        // Update settings voice field for the specified language
        if (strcmp(language, "en") == 0) {
            strncpy(settings.tts.voice_en, target_voice, sizeof(settings.tts.voice_en) - 1);
        } else if (strcmp(language, "zh") == 0) {
            strncpy(settings.tts.voice_zh, target_voice, sizeof(settings.tts.voice_zh) - 1);
        } else if (strcmp(language, "de") == 0) {
            strncpy(settings.tts.voice_de, target_voice, sizeof(settings.tts.voice_de) - 1);
        } else if (strcmp(language, "es") == 0) {
            strncpy(settings.tts.voice_es, target_voice, sizeof(settings.tts.voice_es) - 1);
        }
        
        // Reconstruct piper_model_path from target voice
        const char* home = getenv("HOME");
        if (home) {
            snprintf(settings.tts.piper_model_path,
                    sizeof(settings.tts.piper_model_path),
                    "%s/.ethervox/models/piper/%s.onnx",
                    home, target_voice);
        } else {
            snprintf(settings.tts.piper_model_path,
                    sizeof(settings.tts.piper_model_path),
                    ".ethervox/models/piper/%s.onnx",
                    target_voice);
        }
        
        ETHERVOX_LOG_DEBUG("[Language Switch] New model path: %s", settings.tts.piper_model_path);
        
        // Reload TTS with new voice
        if (ethervox_reload_global_tts(&settings.tts, NULL, NULL) == 0) {
            ETHERVOX_LOG_INFO("[Language Switch] ✅ TTS reloaded with %s voice", language);
            
            // Update caller's TTS context pointer if provided
            if (tts_context) {
                extern ethervox_tts_context_t* g_global_tts;
                *tts_context = g_global_tts;
            }
        } else {
            ETHERVOX_LOG_ERROR("[Language Switch] ❌ Failed to reload TTS");
        }
    } else {
        ETHERVOX_LOG_DEBUG("[Language Switch] Voice already correct for language %s: %s",
                          language, target_voice);
    }
    
    return language;
}