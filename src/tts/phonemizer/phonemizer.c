/**
 * @file phonemizer.c
 * @brief Core phonemizer implementation
 * 
 * Text preprocessing + dictionary lookup + G2P fallback + ARPAbet→IPA
 */

#include "phonemizer.h"
#include "ethervox/error.h"
#include "dictionary.h"
#include "arpabet_to_ipa.h"
#include "espeak_dict.h"
#include "rules_en.h"
#include "rules_de.h"
#include "dict_chinese.h"
#include "pinyin_to_ipa.h"
#include "chinese_segmenter.h"
#include "pronunciation_overrides.h"
#include "stress_reduction.h"
#include "data/espeak_dict_en_us.h"
#include "data/espeak_dict_de.h"
#include "data/espeak_dict_es_419.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 256
#define MAX_TOKEN_LENGTH 64
#define MAX_ARPABET_LENGTH 512

struct phonemizer_context {
    phonemizer_language_t language;
    dict_t* dictionary;  // CMU Dict for English
    dict_chinese_t* chinese_dict;  // CC-CEDICT for Chinese
    pronunciation_override_store_t* overrides;  // User-trainable overrides
};

/**
 * Parse language code to enum
 * Uses prefix matching to support variants like "en-gb-x-rp" → "en-gb"
 */
static phonemizer_language_t parse_language_code(const char* code) {
    if (!code) return PHONEMIZER_LANG_UNKNOWN;
    
    // Use strncmp for prefix matching to support language variants
    // e.g., "en-gb-x-rp" matches "en-gb", "en-us-1" matches "en-us"
    
    if (strncmp(code, "en-us", 5) == 0 || strncmp(code, "en_us", 5) == 0) {
        return PHONEMIZER_LANG_EN_US;
    }
    if (strncmp(code, "en-gb", 5) == 0 || strncmp(code, "en_gb", 5) == 0) {
        return PHONEMIZER_LANG_EN_GB;
    }
    if (strncmp(code, "zh", 2) == 0 || strncmp(code, "zh-cn", 5) == 0 || strncmp(code, "zh_cn", 5) == 0) {
        return PHONEMIZER_LANG_ZH_CN;
    }
    if (strncmp(code, "de", 2) == 0 || strncmp(code, "de-de", 5) == 0 || strncmp(code, "de_de", 5) == 0) {
        return PHONEMIZER_LANG_DE_DE;
    }
    if (strncmp(code, "es-mx", 5) == 0 || strncmp(code, "es_mx", 5) == 0) {
        return PHONEMIZER_LANG_ES_MX;
    }
    if (strncmp(code, "es-419", 6) == 0 || strncmp(code, "es_419", 6) == 0 ||
        strncmp(code, "es", 2) == 0) {  // Generic Spanish
        return PHONEMIZER_LANG_ES_419;
    }
    
    return PHONEMIZER_LANG_UNKNOWN;
}

phonemizer_t* phonemizer_create(const char* lang_code) {
    phonemizer_language_t lang = parse_language_code(lang_code);
    if (lang == PHONEMIZER_LANG_UNKNOWN) {
        ETHERVOX_LOG_ERROR("Unsupported language: %s\n", lang_code);
        return NULL;
    }
    
    phonemizer_t* ctx = calloc(1, sizeof(phonemizer_t));
    if (!ctx) return NULL;
    
    ctx->language = lang;
    
    // Load pronunciation overrides (highest priority)
    ctx->overrides = pronunciation_overrides_load();
    if (ctx->overrides) {
        int total, community;
        float avg_conf;
        pronunciation_overrides_get_stats(ctx->overrides, &total, &community, &avg_conf);
         ETHERVOX_LOG_DEBUG("[Phonemizer] ✅ Loaded %d pronunciation overrides (%d community, avg confidence: %.2f)\n",
               total, community, avg_conf);
    } else {
         ETHERVOX_LOG_DEBUG("[Phonemizer] ⚠️  No pronunciation overrides loaded\n");
    }
    
    // Load dictionary for English
    if (lang == PHONEMIZER_LANG_EN_US || lang == PHONEMIZER_LANG_EN_GB) {
        // Try to find cmudict relative to binary or in data directory
        const char* dict_paths[] = {
            "src/tts/phonemizer/data/cmudict-0.7b.txt",
            "../../src/tts/phonemizer/data/cmudict-0.7b.txt",  // From build/tests/
            "data/cmudict-0.7b.txt",
            "/usr/share/ethervox/cmudict-0.7b.txt",
            NULL
        };
        
        for (int i = 0; dict_paths[i]; i++) {
            ctx->dictionary = dict_load(dict_paths[i]);
            if (ctx->dictionary) {
                ETHERVOX_LOG_INFO("Loaded dictionary from: %s\n", dict_paths[i]);
                break;
            }
        }
        
        if (!ctx->dictionary) {
            ETHERVOX_LOG_WARN("Could not load CMU dictionary, G2P rules only");
        }
    }
    
    // Load dictionary for Chinese
    if (lang == PHONEMIZER_LANG_ZH_CN) {
        const char* unihan_paths[] = {
            "src/tts/phonemizer/data/Unihan_Readings.txt",
            "../../src/tts/phonemizer/data/Unihan_Readings.txt",  // From build/tests/
            "data/Unihan_Readings.txt",
            "/usr/share/ethervox/Unihan_Readings.txt",
            NULL
        };
        
        for (int i = 0; unihan_paths[i]; i++) {
            ctx->chinese_dict = dict_chinese_load(unihan_paths[i]);
            if (ctx->chinese_dict) {
                ETHERVOX_LOG_INFO("Loaded Chinese dictionary from: %s\n", unihan_paths[i]);
                break;
            }
        }
        
        if (!ctx->chinese_dict) {
            ETHERVOX_LOG_ERROR("Could not load Unihan dictionary");
            free(ctx);
            return NULL;
        }
    }
    
    return ctx;
}

/**
 * Tokenize text into words AND punctuation
 * Punctuation is kept as separate tokens so Piper can add pauses
 */
static int tokenize_text(const char* text, char tokens[][MAX_TOKEN_LENGTH], int max_tokens) {
    int count = 0;
    const char* p = text;
    char current[MAX_TOKEN_LENGTH];
    int idx = 0;
    
    while (*p && count < max_tokens) {
        if (isalpha(*p) || *p == '\'') {
            // Build word
            if (idx < MAX_TOKEN_LENGTH - 1) {
                current[idx++] = *p;
            }
        } else if (ispunct(*p) && *p != '\'') {
            // End current word if any
            if (idx > 0) {
                current[idx] = '\0';
                strncpy(tokens[count++], current, MAX_TOKEN_LENGTH);
                idx = 0;
            }
            
            // Add punctuation as separate token (important for pauses!)
            if (count < max_tokens) {
                tokens[count][0] = *p;
                tokens[count][1] = '\0';
                count++;
            }
        } else {
            // Whitespace or other - end current word
            if (idx > 0) {
                current[idx] = '\0';
                strncpy(tokens[count++], current, MAX_TOKEN_LENGTH);
                idx = 0;
            }
        }
        p++;
    }
    
    // Final word
    if (idx > 0 && count < max_tokens) {
        current[idx] = '\0';
        strncpy(tokens[count++], current, MAX_TOKEN_LENGTH);
    }
    
    return count;
}

/**
 * Convert ARPAbet phoneme string to IPA
 */
static int arpabet_string_to_ipa(const char* arpabet, char* ipa_out, size_t max_len) {
    ipa_out[0] = '\0';
    
    char arpabet_copy[MAX_ARPABET_LENGTH];
    strncpy(arpabet_copy, arpabet, MAX_ARPABET_LENGTH - 1);
    arpabet_copy[MAX_ARPABET_LENGTH - 1] = '\0';
    
    char* token = strtok(arpabet_copy, " ");
    while (token) {
        const char* ipa = arpabet_to_ipa(token);
        if (ipa) {
            size_t current_len = strlen(ipa_out);
            size_t ipa_len = strlen(ipa);
            
            if (current_len + ipa_len + 2 > max_len) {
                return -1; // Overflow
            }
            
            if (current_len > 0) {
                strcat(ipa_out, " ");
            }
            strcat(ipa_out, ipa);
        } else {
            ETHERVOX_LOG_ERROR("Unknown ARPAbet phoneme: %s\n", token);
        }
        
        token = strtok(NULL, " ");
    }
    
    return 0;
}

/**
 * Phonemize Chinese text
 */
static int phonemize_chinese(phonemizer_t* ctx, const char* text, char* ipa_output, size_t max_len) {
    if (!ctx->chinese_dict) return -1;
    
    // Segment text into words
    char* words[MAX_TOKENS];
    int word_count = segment_chinese_text(ctx->chinese_dict, text, words, MAX_TOKENS);
    
    if (word_count < 0) {
        fprintf(stderr, "Failed to segment Chinese text\n");
        return -1;
    }
    
    ipa_output[0] = '\0';
    
    // Process each word
    for (int i = 0; i < word_count; i++) {
        char pinyin[256];
        
        // Lookup word in dictionary
        if (dict_chinese_lookup(ctx->chinese_dict, words[i], pinyin, sizeof(pinyin)) != 0) {
            fprintf(stderr, "Chinese word not in dictionary: %s\n", words[i]);
            free(words[i]);
            continue;
        }
        
        // Convert pinyin syllables to IPA
        char* syllable = strtok(pinyin, " ");
        while (syllable) {
            char syllable_ipa[64];
            if (pinyin_syllable_to_ipa(syllable, syllable_ipa, sizeof(syllable_ipa)) == 0) {
                size_t current_len = strlen(ipa_output);
                if (current_len + strlen(syllable_ipa) + 2 < max_len) {
                    if (current_len > 0) strcat(ipa_output, " ");
                    strcat(ipa_output, syllable_ipa);
                }
            }
            syllable = strtok(NULL, " ");
        }
        
        free(words[i]);
    }
    
    return 0;
}

/**
 * Phonemize German text using espeak dictionary + fallback rules
 * Matches English phonemization structure for consistency
 */
static int phonemize_german(phonemizer_t* ctx, const char* text, char* ipa_output, size_t max_len) {
    ipa_output[0] = '\0';
    
    // Tokenize text into words (same as English)
    char tokens[MAX_TOKENS][MAX_TOKEN_LENGTH];
    int token_count = tokenize_text(text, tokens, MAX_TOKENS);
    
    if (token_count == 0) {
        return -1;
    }
    
    ETHERVOX_LOG_DEBUG("[Phonemizer DEBUG] Processing %d German tokens from text: '%s'\n", token_count, text);
    
    // Process each word
    for (int i = 0; i < token_count; i++) {
        char word_ipa[MAX_ARPABET_LENGTH];
        int found = 0;
        
        // Check if this token is punctuation
        if (strlen(tokens[i]) == 1 && ispunct(tokens[i][0])) {
            // Pass punctuation through directly - Piper uses it for pauses
            size_t current_len = strlen(ipa_output);
            if (current_len + 3 < max_len) {
                if (current_len > 0) strcat(ipa_output, " ");
                strcat(ipa_output, tokens[i]);
            }
            continue;
        }
        
        // Priority 1: User pronunciation overrides
        pronunciation_override_t override;
        if (ctx->overrides && 
            pronunciation_overrides_lookup(ctx->overrides, tokens[i], &override) == 0) {
            
            ETHERVOX_LOG_DEBUG("[Phonemizer] 🎯 Override found for '%s': ipa='%s' (confidence=%.3f)\n", 
                    tokens[i], override.ipa, override.confidence);
            
            if (strlen(override.ipa) > 0) {
                strncpy(word_ipa, override.ipa, MAX_ARPABET_LENGTH - 1);
                word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                found = 1;
                ETHERVOX_LOG_DEBUG("[Phonemizer] Using IPA from override: '%s'\n", word_ipa);
            }
            
            pronunciation_overrides_record_usage(ctx->overrides, tokens[i]);
        }
        
        // Priority 2: Espeak dictionary (high-quality pre-trained pronunciations)
        #ifdef ESPEAK_DICT_DE_ENABLED
        if (!found) {
            char espeak_ipa[256];
            ETHERVOX_LOG_DEBUG("[Phonemizer] 📚 Searching de espeak dict (%zu entries)...", espeak_dict_de_size);
            if (espeak_dict_lookup(espeak_dict_de, espeak_dict_de_size,
                                  tokens[i], espeak_ipa, sizeof(espeak_ipa)) == 0) {
                strncpy(word_ipa, espeak_ipa, MAX_ARPABET_LENGTH - 1);
                word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                found = 1;
                ETHERVOX_LOG_DEBUG("[Phonemizer] ✅ Espeak dict (de): '%s' → '%s'", tokens[i], word_ipa);
            } else {
                ETHERVOX_LOG_DEBUG("[Phonemizer] ❌ Not found in de espeak dict: '%s'", tokens[i]);
            }
        }
        #endif
        
        // Priority 3: German G2P rules (fallback)
        if (!found) {
            if (apply_german_g2p_rules(tokens[i], word_ipa, MAX_ARPABET_LENGTH) != 0) {
                ETHERVOX_LOG_ERROR("[Phonemizer] Failed to phonemize German: %s\n", tokens[i]);
                continue;
            } else {
                ETHERVOX_LOG_DEBUG("[Phonemizer] G2P rules produced '%s' → '%s'\n", tokens[i], word_ipa);
            }
        }
        
        // Append to output with word boundary space
        size_t current_len = strlen(ipa_output);
        size_t word_len = strlen(word_ipa);
        
        if (current_len + word_len + 4 > max_len) {
            fprintf(stderr, "German IPA output buffer overflow\n");
            return -1;
        }
        
        if (current_len > 0) {
            strcat(ipa_output, " ");  // Word boundary
        }
        strcat(ipa_output, word_ipa);
    }
    
    return 0;
}

/**
 * Phonemize Spanish text using espeak dictionary
 * Spanish is mostly phonetic, so espeak dictionary should cover most words
 */
static int phonemize_spanish(phonemizer_t* ctx, const char* text, char* ipa_output, size_t max_len) {
    ipa_output[0] = '\0';
    
    // Tokenize text into words (same as English/German)
    char tokens[MAX_TOKENS][MAX_TOKEN_LENGTH];
    int token_count = tokenize_text(text, tokens, MAX_TOKENS);
    
    if (token_count == 0) {
        return -1;
    }
    
    ETHERVOX_LOG_DEBUG("[Phonemizer DEBUG] Processing %d Spanish tokens from text: '%s'\n", token_count, text);
    
    // Process each word
    for (int i = 0; i < token_count; i++) {
        char word_ipa[MAX_ARPABET_LENGTH];
        int found = 0;
        
        // Check if this token is punctuation
        if (strlen(tokens[i]) == 1 && ispunct(tokens[i][0])) {
            // Pass punctuation through directly - Piper uses it for pauses
            size_t current_len = strlen(ipa_output);
            if (current_len + 3 < max_len) {
                if (current_len > 0) strcat(ipa_output, " ");
                strcat(ipa_output, tokens[i]);
            }
            continue;
        }
        
        // Priority 1: User pronunciation overrides
        pronunciation_override_t override;
        if (ctx->overrides && 
            pronunciation_overrides_lookup(ctx->overrides, tokens[i], &override) == 0) {
            
            ETHERVOX_LOG_DEBUG("[Phonemizer] 🎯 Override found for '%s': ipa='%s' (confidence=%.3f)\n", 
                    tokens[i], override.ipa, override.confidence);
            
            if (strlen(override.ipa) > 0) {
                strncpy(word_ipa, override.ipa, MAX_ARPABET_LENGTH - 1);
                word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                found = 1;
                ETHERVOX_LOG_DEBUG("[Phonemizer] Using IPA from override: '%s'\n", word_ipa);
            }
            
            pronunciation_overrides_record_usage(ctx->overrides, tokens[i]);
        }
        
        // Priority 2: Espeak dictionary (high-quality pre-trained pronunciations)
        #ifdef ESPEAK_DICT_ES_419_ENABLED
        if (!found) {
            char espeak_ipa[256];
            ETHERVOX_LOG_DEBUG("[Phonemizer] 📚 Searching es espeak dict (%zu entries)...", espeak_dict_es_419_size);
            if (espeak_dict_lookup(espeak_dict_es_419, espeak_dict_es_419_size,
                                  tokens[i], espeak_ipa, sizeof(espeak_ipa)) == 0) {
                strncpy(word_ipa, espeak_ipa, MAX_ARPABET_LENGTH - 1);
                word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                found = 1;
                ETHERVOX_LOG_DEBUG("[Phonemizer] ✅ Espeak dict (es): '%s' → '%s'", tokens[i], word_ipa);
            } else {
                ETHERVOX_LOG_DEBUG("[Phonemizer] ❌ Not found in es espeak dict: '%s'", tokens[i]);
            }
        }
        #endif
        
        // Priority 3: Spanish is mostly phonetic, so if not in dict, log error
        if (!found) {
            ETHERVOX_LOG_ERROR("[Phonemizer] Spanish word not in dictionary (no G2P fallback yet): %s\n", tokens[i]);
            // For now, skip unknown words rather than failing completely
            continue;
        }
        
        // Append to output with word boundary space
        size_t current_len = strlen(ipa_output);
        size_t word_len = strlen(word_ipa);
        
        if (current_len + word_len + 4 > max_len) {
            fprintf(stderr, "Spanish IPA output buffer overflow\n");
            return -1;
        }
        
        if (current_len > 0) {
            strcat(ipa_output, " ");  // Word boundary
        }
        strcat(ipa_output, word_ipa);
    }
    
    return 0;
}

ethervox_result_t phonemizer_text_to_ipa(phonemizer_t* ctx, const char* text, char* ipa_output, size_t max_len) {
    ETHERVOX_CHECK_PTR(ctx);
    ETHERVOX_CHECK_PTR(text);
    ETHERVOX_CHECK_PTR(ipa_output);
    
    if (max_len == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ipa_output[0] = '\0';
    
    // Route to language-specific implementation
    if (ctx->language == PHONEMIZER_LANG_ZH_CN) {
        return phonemize_chinese(ctx, text, ipa_output, max_len);
    }
    
    if (ctx->language == PHONEMIZER_LANG_DE_DE) {
        return phonemize_german(ctx, text, ipa_output, max_len);
    }
    
    if (ctx->language == PHONEMIZER_LANG_ES_MX || ctx->language == PHONEMIZER_LANG_ES_419) {
        return phonemize_spanish(ctx, text, ipa_output, max_len);
    }
    
    // English (EN_US, EN_GB) continues below with the original implementation
    // Language dispatch based on phonemizer context
    // Each language has its own phonemization function
    
    // Tokenize text into words
    char tokens[MAX_TOKENS][MAX_TOKEN_LENGTH];
    int token_count = tokenize_text(text, tokens, MAX_TOKENS);
    
    if (token_count == 0) {
        ETHERVOX_LOG_DEBUG("[Phonemizer] No tokens found in text: '%s'\n", text);
        return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED; // No words found
    }
    
    ETHERVOX_LOG_DEBUG("[Phonemizer DEBUG] Processing %d tokens from text: '%s'\n", token_count, text);
    
    // Process each word
    for (int i = 0; i < token_count; i++) {
        char arpabet[MAX_ARPABET_LENGTH];
        char word_ipa[MAX_ARPABET_LENGTH];
        
        // Check if this token is punctuation
        if (strlen(tokens[i]) == 1 && ispunct(tokens[i][0])) {
            // Pass punctuation through directly - Piper uses it for pauses
            size_t current_len = strlen(ipa_output);
            if (current_len + 3 < max_len) {
                if (current_len > 0) strcat(ipa_output, " ");
                strcat(ipa_output, tokens[i]);
            }
            continue;
        }
        
        // Try pronunciation overrides first (highest priority)
        int found = 0;
        int override_is_ipa = 0;  // Track if override is already in IPA format
        pronunciation_override_t override;
        if (ctx->overrides && 
            pronunciation_overrides_lookup(ctx->overrides, tokens[i], &override) == 0) {
            
            ETHERVOX_LOG_DEBUG("[Phonemizer] 🎯 Override found for '%s': ipa='%s' phonemes='%s' (confidence=%.3f)\n", 
                    tokens[i], override.ipa, override.phonemes, override.confidence);
            
            // Check if override.ipa is populated - if so, use it directly
            if (strlen(override.ipa) > 0) {
                strncpy(word_ipa, override.ipa, MAX_ARPABET_LENGTH - 1);
                word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                override_is_ipa = 1;
                found = 1;
                ETHERVOX_LOG_DEBUG("[Phonemizer] Using IPA from override: '%s'\n", word_ipa);
            } else if (strlen(override.phonemes) > 0) {
                // Fallback to phonemes field (should be ARPABET)
                strncpy(arpabet, override.phonemes, MAX_ARPABET_LENGTH - 1);
                arpabet[MAX_ARPABET_LENGTH - 1] = '\0';
                found = 1;
                ETHERVOX_LOG_DEBUG("[Phonemizer] Using phonemes from override (will convert): '%s'\n", arpabet);
            } else {
                ETHERVOX_LOG_DEBUG("[Phonemizer] WARNING: Override has empty ipa AND phonemes fields!");
            }
            
            // Record usage for promotion tracking
            pronunciation_overrides_record_usage(ctx->overrides, tokens[i]);
        }
        
        // Try embedded espeak dictionary (2nd priority)
        if (!found) {
            char espeak_ipa[MAX_ARPABET_LENGTH];
            int espeak_found = 0;
            
            ETHERVOX_LOG_DEBUG("[Phonemizer] 🔍 Trying espeak dictionary lookup for '%s' (language=%d)", tokens[i], ctx->language);
            
            #ifdef ESPEAK_DICT_EN_US_ENABLED
            if (ctx->language == PHONEMIZER_LANG_EN_US) {
                ETHERVOX_LOG_DEBUG("[Phonemizer] 📚 Searching en-us espeak dict (%zu entries)...", espeak_dict_en_us_size);
                if (espeak_dict_lookup(espeak_dict_en_us, espeak_dict_en_us_size,
                                      tokens[i], espeak_ipa, sizeof(espeak_ipa)) == 0) {
                    strncpy(word_ipa, espeak_ipa, MAX_ARPABET_LENGTH - 1);
                    word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                    found = 1;
                    override_is_ipa = 1;
                    espeak_found = 1;
                    ETHERVOX_LOG_DEBUG("[Phonemizer] ✅ Espeak dict (en-us): '%s' → '%s'", tokens[i], word_ipa);
                } else {
                    ETHERVOX_LOG_DEBUG("[Phonemizer] ❌ Not found in en-us espeak dict: '%s'", tokens[i]);
                }
            }
            #else
            ETHERVOX_LOG_DEBUG("[Phonemizer] ⚠️  ESPEAK_DICT_EN_US_ENABLED not defined");
            #endif
            
            #ifdef ESPEAK_DICT_EN_GB_RP_ENABLED
            if (!espeak_found && ctx->language == PHONEMIZER_LANG_EN_GB) {
                ETHERVOX_LOG_DEBUG("[Phonemizer] 📚 Searching en-gb-rp espeak dict (%zu entries)...", espeak_dict_en_gb_rp_size);
                if (espeak_dict_lookup(espeak_dict_en_gb_rp, espeak_dict_en_gb_rp_size,
                                      tokens[i], espeak_ipa, sizeof(espeak_ipa)) == 0) {
                    strncpy(word_ipa, espeak_ipa, MAX_ARPABET_LENGTH - 1);
                    word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                    found = 1;
                    override_is_ipa = 1;
                    espeak_found = 1;
                    ETHERVOX_LOG_DEBUG("[Phonemizer] ✅ Espeak dict (en-gb-rp): '%s' → '%s'", tokens[i], word_ipa);
                } else {
                    ETHERVOX_LOG_DEBUG("[Phonemizer] ❌ Not found in en-gb-rp espeak dict: '%s'", tokens[i]);
                }
            }
            #endif
            
            #ifdef ESPEAK_DICT_DE_ENABLED
            if (!espeak_found && ctx->language == PHONEMIZER_LANG_DE_DE) {
                ETHERVOX_LOG_DEBUG("[Phonemizer] 📚 Searching de espeak dict (%zu entries)...", espeak_dict_de_size);
                if (espeak_dict_lookup(espeak_dict_de, espeak_dict_de_size,
                                      tokens[i], espeak_ipa, sizeof(espeak_ipa)) == 0) {
                    strncpy(word_ipa, espeak_ipa, MAX_ARPABET_LENGTH - 1);
                    word_ipa[MAX_ARPABET_LENGTH - 1] = '\0';
                    found = 1;
                    override_is_ipa = 1;
                    espeak_found = 1;
                    ETHERVOX_LOG_DEBUG("[Phonemizer] ✅ Espeak dict (de): '%s' → '%s'", tokens[i], word_ipa);
                } else {
                    ETHERVOX_LOG_DEBUG("[Phonemizer] ❌ Not found in de espeak dict: '%s'", tokens[i]);
                }
            }
            #endif
            
            if (!espeak_found) {
                ETHERVOX_LOG_DEBUG("[Phonemizer] ⚠️  Espeak lookup failed for '%s', falling back to next priority", tokens[i]);
            }
        }
        
        // Try CMU/traditional dictionary (3rd priority)
        if (!found && ctx->dictionary) {
            if (dict_lookup(ctx->dictionary, tokens[i], arpabet, MAX_ARPABET_LENGTH) == 0) {
                found = 1;
                ETHERVOX_LOG_DEBUG("[Phonemizer] Dictionary found '%s' → '%s'\n", tokens[i], arpabet);
            } else {
                ETHERVOX_LOG_DEBUG("[Phonemizer] Dictionary lookup failed for '%s'\n", tokens[i]);
            }
        }
        
        // Fallback to G2P rules
        if (!found) {
            if (apply_english_g2p_rules(tokens[i], arpabet, MAX_ARPABET_LENGTH) != 0) {
                ETHERVOX_LOG_ERROR("Failed to phonemize: %s\n", tokens[i]);
                continue;
            } else {
                ETHERVOX_LOG_DEBUG("[Phonemizer] G2P rules produced '%s' → '%s'\n", tokens[i], arpabet);
            }
        }
        
        // Convert ARPAbet to IPA (skip if override was already IPA)
        if (!override_is_ipa) {
            if (arpabet_string_to_ipa(arpabet, word_ipa, MAX_ARPABET_LENGTH) != 0) {
                fprintf(stderr, "Failed ARPAbet→IPA conversion\n");
                continue;
            }
        }
        
        // Apply stress reduction for natural connected speech
        stress_reduction_context_t context;
        if (token_count == 1) {
            context = STRESS_CONTEXT_ISOLATED;
        } else if (i == 0) {
            // Check if sentence ends with question mark
            if (text[strlen(text) - 1] == '?') {
                context = STRESS_CONTEXT_QUESTION;
            } else {
                context = STRESS_CONTEXT_SENTENCE_INITIAL;
            }
        } else if (i == token_count - 1) {
            if (text[strlen(text) - 1] == '?') {
                context = STRESS_CONTEXT_QUESTION;
            } else {
                context = STRESS_CONTEXT_SENTENCE_FINAL;
            }
        } else {
            if (text[strlen(text) - 1] == '?') {
                context = STRESS_CONTEXT_QUESTION;
            } else {
                context = STRESS_CONTEXT_SENTENCE_MEDIAL;
            }
        }
        
        apply_stress_reduction(tokens[i], word_ipa, MAX_ARPABET_LENGTH, context);
        
        // Append to output with word boundary space
        size_t current_len = strlen(ipa_output);
        size_t word_len = strlen(word_ipa);
        
        if (current_len + word_len + 2 > max_len) {
            fprintf(stderr, "IPA output buffer overflow\n");
            return ETHERVOX_ERROR_TTS_TEXT_TOO_LONG;
        }
        
        if (current_len > 0) {
            strcat(ipa_output, " ");  // Space between words only
        }
        strcat(ipa_output, word_ipa);
    }
    
    return ETHERVOX_SUCCESS;
}

phonemizer_language_t phonemizer_get_language(phonemizer_t* ctx) {
    return ctx ? ctx->language : PHONEMIZER_LANG_UNKNOWN;
}

void* phonemizer_get_override_store(phonemizer_t* ctx) {
    return ctx ? ctx->overrides : NULL;
}

void phonemizer_destroy(phonemizer_t* ctx) {
    if (!ctx) return;
    
    if (ctx->dictionary) {
        dict_free(ctx->dictionary);
    }
    
    if (ctx->chinese_dict) {
        dict_chinese_free(ctx->chinese_dict);
    }
    
    if (ctx->overrides) {
        // Save overrides and promote qualifying ones before cleanup
        pronunciation_overrides_promote(ctx->overrides);
        pronunciation_overrides_save(ctx->overrides);
        pronunciation_overrides_free(ctx->overrides);
    }
    
    free(ctx);
}
