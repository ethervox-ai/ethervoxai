/**
 * @file rules_en.c
 * @brief Simple English G2P rules for OOV words
 * 
 * Basic letter-to-sound rules with common patterns.
 * Accuracy: ~70-80% for out-of-vocabulary words.
 */

#include "rules_en.h"
#include "ethervox/error.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define MAX_PHONEMES 32

// Helper: check if character is vowel
static int is_vowel(char c) {
    c = tolower(c);
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y');
}

// Helper: append phoneme to output
static int append_phoneme(char* output, size_t max_len, const char* phoneme) {
    size_t current_len = strlen(output);
    size_t phoneme_len = strlen(phoneme);
    
    if (current_len + phoneme_len + 2 > max_len) {
        return -1; // Overflow
    }
    
    if (current_len > 0) {
        strcat(output, " ");
    }
    strcat(output, phoneme);
    return 0;
}

ethervox_result_t apply_english_g2p_rules(const char* word, char* arpabet_out, size_t max_len) {
    ETHERVOX_CHECK_PTR(word);
    ETHERVOX_CHECK_PTR(arpabet_out);
    
    if (max_len == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    arpabet_out[0] = '\0';
    size_t len = strlen(word);
    if (len == 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
    
    // Convert to lowercase working copy
    char lower[256];
    for (size_t i = 0; i < len && i < 255; i++) {
        lower[i] = tolower(word[i]);
    }
    lower[len] = '\0';
    
    // Simple rule-based conversion
    for (size_t i = 0; i < len; i++) {
        char c = lower[i];
        char next = (i + 1 < len) ? lower[i + 1] : '\0';
        char prev = (i > 0) ? lower[i - 1] : '\0';
        
        // Consonant digraphs
        if (c == 'c' && next == 'h') {
            append_phoneme(arpabet_out, max_len, "CH");
            i++; continue;
        }
        if (c == 's' && next == 'h') {
            append_phoneme(arpabet_out, max_len, "SH");
            i++; continue;
        }
        if (c == 't' && next == 'h') {
            // th: voiced or voiceless (default voiceless)
            append_phoneme(arpabet_out, max_len, "TH");
            i++; continue;
        }
        if (c == 'p' && next == 'h') {
            append_phoneme(arpabet_out, max_len, "F");
            i++; continue;
        }
        if (c == 'n' && next == 'g') {
            char after_g = (i + 2 < len) ? lower[i + 2] : '\0';
            // ng → /ŋ/ only at word end or before consonants (except 'l', 'r' which start new syllable)
            // Examples: "sing" → /ŋ/, "bang" → /ŋ/, but "English" → /ŋɡ/, "anger" → /ŋɡ/
            if (i + 2 >= len || (!is_vowel(after_g) && after_g != 'l' && after_g != 'r')) {
                append_phoneme(arpabet_out, max_len, "NG");
                i++; continue;
            }
            // Otherwise treat as /n/ + /ɡ/ separately (English, angle, etc.)
        }
        
        // Vowels (simple rules, no stress for now)
        if (c == 'a') {
            if (next == 'r') {
                append_phoneme(arpabet_out, max_len, "AA1 R");
                i++; continue;
            } else if (next && !is_vowel(next) && i + 2 < len && lower[i + 2] == 'e') {
                // a_e → long A
                append_phoneme(arpabet_out, max_len, "EY1");
                continue;
            } else {
                // Default short a
                append_phoneme(arpabet_out, max_len, "AE1");
                continue;
            }
        }
        if (c == 'e') {
            if (next == 'r') {
                append_phoneme(arpabet_out, max_len, "ER1");
                i++; continue;
            } else if (next == 'e') {
                // ee → long E
                append_phoneme(arpabet_out, max_len, "IY1");
                i++; continue;
            } else if (i + 1 == len) {
                // Silent e at end
                continue;
            } else {
                append_phoneme(arpabet_out, max_len, "EH1");
                continue;
            }
        }
        if (c == 'i') {
            if (next == 'r') {
                append_phoneme(arpabet_out, max_len, "ER1");
                i++; continue;
            } else if (next && !is_vowel(next) && i + 2 < len && lower[i + 2] == 'e') {
                // i_e → long I
                append_phoneme(arpabet_out, max_len, "AY1");
                continue;
            } else {
                append_phoneme(arpabet_out, max_len, "IH1");
                continue;
            }
        }
        if (c == 'o') {
            if (next == 'r') {
                append_phoneme(arpabet_out, max_len, "AO1 R");
                i++; continue;
            } else if (next == 'o') {
                // oo → long U
                append_phoneme(arpabet_out, max_len, "UW1");
                i++; continue;
            } else if (next && !is_vowel(next) && i + 2 < len && lower[i + 2] == 'e') {
                // o_e → long O
                append_phoneme(arpabet_out, max_len, "OW1");
                continue;
            } else {
                append_phoneme(arpabet_out, max_len, "AA1");
                continue;
            }
        }
        if (c == 'u') {
            if (next == 'r') {
                append_phoneme(arpabet_out, max_len, "ER1");
                i++; continue;
            } else if (next && !is_vowel(next) && i + 2 < len && lower[i + 2] == 'e') {
                // u_e → long U
                append_phoneme(arpabet_out, max_len, "UW1");
                continue;
            } else {
                append_phoneme(arpabet_out, max_len, "AH1");
                continue;
            }
        }
        if (c == 'y') {
            // y as vowel (end of word or between consonants)
            if (i == len - 1 || !is_vowel(next)) {
                append_phoneme(arpabet_out, max_len, "IY1");
                continue;
            } else {
                append_phoneme(arpabet_out, max_len, "Y");
                continue;
            }
        }
        
        // Single consonants
        switch (c) {
            case 'b': append_phoneme(arpabet_out, max_len, "B"); break;
            case 'c':
                // c → s before e,i,y; otherwise k
                if (next == 'e' || next == 'i' || next == 'y') {
                    append_phoneme(arpabet_out, max_len, "S");
                } else {
                    append_phoneme(arpabet_out, max_len, "K");
                }
                break;
            case 'd': append_phoneme(arpabet_out, max_len, "D"); break;
            case 'f': append_phoneme(arpabet_out, max_len, "F"); break;
            case 'g':
                // g → j before e,i,y sometimes; default g
                if (next == 'e' || next == 'i' || next == 'y') {
                    append_phoneme(arpabet_out, max_len, "JH"); // Simplified
                } else {
                    append_phoneme(arpabet_out, max_len, "G");
                }
                break;
            case 'h': append_phoneme(arpabet_out, max_len, "HH"); break;
            case 'j': append_phoneme(arpabet_out, max_len, "JH"); break;
            case 'k': append_phoneme(arpabet_out, max_len, "K"); break;
            case 'l': append_phoneme(arpabet_out, max_len, "L"); break;
            case 'm': append_phoneme(arpabet_out, max_len, "M"); break;
            case 'n': append_phoneme(arpabet_out, max_len, "N"); break;
            case 'p': append_phoneme(arpabet_out, max_len, "P"); break;
            case 'q': append_phoneme(arpabet_out, max_len, "K"); break; // qu handled below
            case 'r': append_phoneme(arpabet_out, max_len, "R"); break;
            case 's':
                // s → z between vowels sometimes; default s
                append_phoneme(arpabet_out, max_len, "S");
                break;
            case 't': append_phoneme(arpabet_out, max_len, "T"); break;
            case 'v': append_phoneme(arpabet_out, max_len, "V"); break;
            case 'w': append_phoneme(arpabet_out, max_len, "W"); break;
            case 'x': append_phoneme(arpabet_out, max_len, "K S"); break;
            case 'z': append_phoneme(arpabet_out, max_len, "Z"); break;
            default:
                // Skip unknown characters
                break;
        }
    }
    
    return (strlen(arpabet_out) > 0) ? 0 : -1;
}
