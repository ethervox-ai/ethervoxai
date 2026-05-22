/**
 * @file rules_de.c
 * @brief German grapheme-to-phoneme conversion rules
 * 
 * Implements rule-based G2P for German using its regular orthography.
 * German has very predictable pronunciation rules compared to English.
 * 
 * Key German phonological patterns:
 * - Vowel length: doubled vowels (aa, ee, oo) or vowel+h (ah, eh, oh) = long
 * - ie = long i sound /iː/
 * - ch = /ç/ after front vowels (ich-Laut), /x/ elsewhere (ach-Laut)
 * - sch = /ʃ/
 * - -ig = /ɪç/ word-finally
 * - sp-/st- = /ʃp-/, /ʃt-/ word-initially
 * - German umlauts: ä, ö, ü
 * 
 * License: Part of EthervoxAI (CC BY-NC-SA 4.0)
 */

#include "rules_de.h"
#include "ethervox/error.h"
#include <string.h>
#include <ctype.h>
#include <stdint.h>

// Helper: Check if vowel (including umlauts)
static int is_german_vowel(const unsigned char* p) {
    if (*p < 128) {
        char c = tolower(*p);
        return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y');
    }
    // Check UTF-8 umlauts: ä(C3A4), ö(C3B6), ü(C3BC)
    if (p[0] == 0xC3 && (p[1] == 0xA4 || p[1] == 0xB6 || p[1] == 0xBC)) {
        return 1;
    }
    return 0;
}

// Helper: Check if front vowel (for ich-Laut vs ach-Laut)
static int is_front_vowel_context(const unsigned char* p, int offset) {
    const unsigned char* check = p + offset;
    if (*check < 128) {
        char c = tolower(*check);
        return (c == 'e' || c == 'i');
    }
    // UTF-8 ä(C3A4), ö(C3B6), ü(C3BC)
    if (check[0] == 0xC3 && (check[1] == 0xA4 || check[1] == 0xB6 || check[1] == 0xBC)) {
        return 1;
    }
    return 0;
}

// Helper: Append IPA to output
static int append_ipa(char* output, size_t max_len, const char* ipa) {
    size_t current = strlen(output);
    size_t ipa_len = strlen(ipa);
    if (current + ipa_len >= max_len) return -1;
    strcat(output, ipa);
    return 0;
}

// Helper: Check if word starts with pattern (case-insensitive)
static int starts_with(const char* word, const char* pattern) {
    size_t plen = strlen(pattern);
    for (size_t i = 0; i < plen; i++) {
        if (tolower(word[i]) != tolower(pattern[i])) return 0;
    }
    return 1;
}

ethervox_result_t apply_german_g2p_rules(const char* word, char* ipa_out, size_t max_len) {
    ETHERVOX_CHECK_PTR(word);
    ETHERVOX_CHECK_PTR(ipa_out);
    
    if (max_len == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ipa_out[0] = '\0';
    size_t len = strlen(word);
    if (len == 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
    
    const unsigned char* p = (const unsigned char*)word;
    const unsigned char* end = p + len;
    
    while (p < end && *p) {
        char c = tolower(*p);
        const unsigned char* next = p + 1;
        const unsigned char* next2 = next < end ? next + 1 : NULL;
        char c_next = (next < end) ? tolower(*next) : '\0';
        char c_next2 = (next2 && next2 < end) ? tolower(*next2) : '\0';
        
        // === Consonant clusters and digraphs ===
        
        // "sch" -> /ʃ/
        if (c == 's' && c_next == 'c' && c_next2 == 'h') {
            if (append_ipa(ipa_out, max_len, "ʃ") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 3;
            continue;
        }
        
        // "sp-" word-initially -> /ʃp/
        if (c == 's' && c_next == 'p' && (p == (unsigned char*)word || !isalpha(*(p-1)))) {
            if (append_ipa(ipa_out, max_len, "ʃp") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "st-" word-initially -> /ʃt/
        if (c == 's' && c_next == 't' && (p == (unsigned char*)word || !isalpha(*(p-1)))) {
            if (append_ipa(ipa_out, max_len, "ʃt") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "ch" -> /ç/ (ich-Laut) or /x/ (ach-Laut)
        if (c == 'c' && c_next == 'h') {
            // Check preceding character for front vs back vowel
            if (p > (unsigned char*)word && is_front_vowel_context(p, -1)) {
                if (append_ipa(ipa_out, max_len, "ç") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            } else {
                if (append_ipa(ipa_out, max_len, "x") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            }
            p += 2;
            continue;
        }
        
        // "tsch" -> /tʃ/ (borrowed words)
        if (c == 't' && c_next == 's' && c_next2 == 'c' && next2 < end && tolower(*(next2+1)) == 'h') {
            if (append_ipa(ipa_out, max_len, "tʃ") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 4;
            continue;
        }
        
        // "ck" -> /k/
        if (c == 'c' && c_next == 'k') {
            if (append_ipa(ipa_out, max_len, "k") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "tion" -> /tsi̯oːn/ (borrowed)
        if (c == 't' && starts_with((char*)p, "tion")) {
            if (append_ipa(ipa_out, max_len, "tsi̯oːn") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 4;
            continue;
        }
        
        // "-ig" word-finally -> /ɪç/
        if (c == 'i' && c_next == 'g' && (next + 1 >= end || !isalpha(c_next2))) {
            if (append_ipa(ipa_out, max_len, "ɪç") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // === Vowels and diphthongs ===
        
        // "ie" -> /iː/ (long i)
        if (c == 'i' && c_next == 'e') {
            if (append_ipa(ipa_out, max_len, "iː") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "ei" -> /aɪ/
        if (c == 'e' && c_next == 'i') {
            if (append_ipa(ipa_out, max_len, "aɪ") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "ai" -> /aɪ/
        if (c == 'a' && c_next == 'i') {
            if (append_ipa(ipa_out, max_len, "aɪ") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "au" -> /aʊ/
        if (c == 'a' && c_next == 'u') {
            if (append_ipa(ipa_out, max_len, "aʊ") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "eu" -> /ɔʏ/
        if (c == 'e' && c_next == 'u') {
            if (append_ipa(ipa_out, max_len, "ɔʏ") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 2;
            continue;
        }
        
        // "äu" -> /ɔʏ/
        if (*p == 0xC3 && *(p+1) == 0xA4 && c_next == 'u') {
            if (append_ipa(ipa_out, max_len, "ɔʏ") < 0) return ETHERVOX_ERROR_TTS_PHONEMIZATION_FAILED;
            p += 3; // UTF-8 ä + u
            continue;
        }
        
        // Vowel + 'h' -> long vowel (ah, eh, ih, oh, uh)
        if (is_german_vowel(p) && c_next == 'h') {
            switch (c) {
                case 'a': append_ipa(ipa_out, max_len, "aː"); break;
                case 'e': append_ipa(ipa_out, max_len, "eː"); break;
                case 'i': append_ipa(ipa_out, max_len, "iː"); break;
                case 'o': append_ipa(ipa_out, max_len, "oː"); break;
                case 'u': append_ipa(ipa_out, max_len, "uː"); break;
            }
            p += 2;
            continue;
        }
        
        // Doubled vowels -> long (aa, ee, oo)
        if (is_german_vowel(p) && *next == *p) {
            switch (c) {
                case 'a': append_ipa(ipa_out, max_len, "aː"); break;
                case 'e': append_ipa(ipa_out, max_len, "eː"); break;
                case 'o': append_ipa(ipa_out, max_len, "oː"); break;
            }
            p += 2;
            continue;
        }
        
        // === Simple vowels ===
        if (c == 'a') {
            append_ipa(ipa_out, max_len, "a");
            p++;
            continue;
        }
        if (c == 'e') {
            // Word-final -e is typically schwa /ə/
            if (next >= end || !isalpha(c_next)) {
                append_ipa(ipa_out, max_len, "ə");
            } else {
                append_ipa(ipa_out, max_len, "ɛ");
            }
            p++;
            continue;
        }
        if (c == 'i') {
            append_ipa(ipa_out, max_len, "ɪ");
            p++;
            continue;
        }
        if (c == 'o') {
            append_ipa(ipa_out, max_len, "ɔ");
            p++;
            continue;
        }
        if (c == 'u') {
            append_ipa(ipa_out, max_len, "ʊ");
            p++;
            continue;
        }
        if (c == 'y') {
            append_ipa(ipa_out, max_len, "ʏ");
            p++;
            continue;
        }
        
        // === Umlauts (UTF-8) ===
        if (*p == 0xC3) {
            if (*(p+1) == 0xA4) { // ä
                append_ipa(ipa_out, max_len, "ɛ");
                p += 2;
                continue;
            }
            if (*(p+1) == 0xB6) { // ö
                append_ipa(ipa_out, max_len, "œ");
                p += 2;
                continue;
            }
            if (*(p+1) == 0xBC) { // ü
                append_ipa(ipa_out, max_len, "ʏ");
                p += 2;
                continue;
            }
            if (*(p+1) == 0x9F) { // ß
                append_ipa(ipa_out, max_len, "s");
                p += 2;
                continue;
            }
        }
        
        // === Consonants ===
        if (c == 'b') {
            // Word-final devoicing: -b -> /p/
            if (next >= end || !isalpha(c_next)) {
                append_ipa(ipa_out, max_len, "p");
            } else {
                append_ipa(ipa_out, max_len, "b");
            }
            p++;
            continue;
        }
        if (c == 'd') {
            // Word-final devoicing: -d -> /t/
            if (next >= end || !isalpha(c_next)) {
                append_ipa(ipa_out, max_len, "t");
            } else {
                append_ipa(ipa_out, max_len, "d");
            }
            p++;
            continue;
        }
        if (c == 'g') {
            // Word-final devoicing: -g -> /k/
            if (next >= end || !isalpha(c_next)) {
                append_ipa(ipa_out, max_len, "k");
            } else {
                append_ipa(ipa_out, max_len, "ɡ");
            }
            p++;
            continue;
        }
        if (c == 'f') { append_ipa(ipa_out, max_len, "f"); p++; continue; }
        if (c == 'h') { append_ipa(ipa_out, max_len, "h"); p++; continue; }
        if (c == 'j') { append_ipa(ipa_out, max_len, "j"); p++; continue; }
        if (c == 'k') { append_ipa(ipa_out, max_len, "k"); p++; continue; }
        if (c == 'l') { append_ipa(ipa_out, max_len, "l"); p++; continue; }
        if (c == 'm') { append_ipa(ipa_out, max_len, "m"); p++; continue; }
        if (c == 'n') { append_ipa(ipa_out, max_len, "n"); p++; continue; }
        if (c == 'p') { append_ipa(ipa_out, max_len, "p"); p++; continue; }
        if (c == 'q') { append_ipa(ipa_out, max_len, "kv"); p++; continue; }
        if (c == 'r') { append_ipa(ipa_out, max_len, "ʁ"); p++; continue; }
        if (c == 's') {
            // 's' before vowel -> /z/, otherwise /s/
            if (next < end && is_german_vowel(next)) {
                append_ipa(ipa_out, max_len, "z");
            } else {
                append_ipa(ipa_out, max_len, "s");
            }
            p++;
            continue;
        }
        if (c == 't') { append_ipa(ipa_out, max_len, "t"); p++; continue; }
        if (c == 'v') { append_ipa(ipa_out, max_len, "f"); p++; continue; }
        if (c == 'w') { append_ipa(ipa_out, max_len, "v"); p++; continue; }
        if (c == 'x') { append_ipa(ipa_out, max_len, "ks"); p++; continue; }
        if (c == 'z') { append_ipa(ipa_out, max_len, "ts"); p++; continue; }
        if (c == 'c') { append_ipa(ipa_out, max_len, "k"); p++; continue; }
        
        // Unknown character - skip
        p++;
    }
    
    return ETHERVOX_SUCCESS;
}
