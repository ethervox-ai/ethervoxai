/**
 * @file pinyin_to_ipa.h
 * @brief Pinyin to IPA conversion for Mandarin Chinese
 * 
 * Converts pinyin syllables with tone marks to IPA representation.
 * Tone marks: 1=˥ (high), 2=˧˥ (rising), 3=˨˩˦ (dipping), 4=˥˩ (falling), 5=neutral
 */

#ifndef ETHERVOX_PINYIN_TO_IPA_H
#define ETHERVOX_PINYIN_TO_IPA_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/**
 * Pinyin to IPA mapping structure
 */
typedef struct {
    const char* pinyin;
    const char* ipa;
} pinyin_ipa_map_t;

// Pinyin initial consonants
static const pinyin_ipa_map_t PINYIN_INITIALS[] = {
    {"b", "p"},
    {"p", "pʰ"},
    {"m", "m"},
    {"f", "f"},
    {"d", "t"},
    {"t", "tʰ"},
    {"n", "n"},
    {"l", "l"},
    {"g", "k"},
    {"k", "kʰ"},
    {"h", "x"},
    {"j", "tɕ"},
    {"q", "tɕʰ"},
    {"x", "ɕ"},
    {"zh", "ʈʂ"},
    {"ch", "ʈʂʰ"},
    {"sh", "ʂ"},
    {"r", "ʐ"},
    {"z", "ts"},
    {"c", "tsʰ"},
    {"s", "s"},
    {"y", "j"},
    {"w", "w"},
};

// Pinyin finals (vowels and diphthongs)
static const pinyin_ipa_map_t PINYIN_FINALS[] = {
    {"a", "a"},
    {"o", "o"},
    {"e", "ɤ"},
    {"i", "i"},
    {"u", "u"},
    {"ü", "y"},
    {"v", "y"},  // Alternative notation for ü
    {"ai", "aɪ"},
    {"ei", "eɪ"},
    {"ui", "weɪ"},
    {"ao", "ɑʊ"},
    {"ou", "oʊ"},
    {"iu", "joʊ"},
    {"ie", "jɛ"},
    {"üe", "ɥɛ"},
    {"ve", "ɥɛ"},
    {"er", "ɚ"},
    {"an", "an"},
    {"en", "ən"},
    {"in", "in"},
    {"un", "wən"},
    {"ün", "yn"},
    {"vn", "yn"},
    {"ang", "ɑŋ"},
    {"eng", "ɤŋ"},
    {"ing", "iŋ"},
    {"ong", "ʊŋ"},
    {"ia", "ja"},
    {"ua", "wa"},
    {"uo", "wo"},
    {"iao", "jɑʊ"},
    {"uai", "waɪ"},
    {"ian", "jɛn"},
    {"uan", "wan"},
    {"üan", "ɥɛn"},
    {"van", "ɥɛn"},
    {"iang", "jɑŋ"},
    {"uang", "wɑŋ"},
    {"iong", "jʊŋ"},
};

// Tone markers (IPA tone letters)
static const char* TONE_MARKERS[] = {
    "",      // 0 = no tone (shouldn't occur)
    "˥",     // 1 = high level (55)
    "˧˥",    // 2 = rising (35)
    "˨˩˦",   // 3 = dipping (214)
    "˥˩",    // 4 = falling (51)
    "",      // 5 = neutral/light (no marker)
};

#define PINYIN_INITIALS_COUNT (sizeof(PINYIN_INITIALS) / sizeof(PINYIN_INITIALS[0]))
#define PINYIN_FINALS_COUNT (sizeof(PINYIN_FINALS) / sizeof(PINYIN_FINALS[0]))

/**
 * Convert single pinyin syllable to IPA
 * 
 * @param pinyin Input pinyin (e.g., "ni3", "hao3", "ma5")
 * @param ipa_out Output buffer for IPA
 * @param max_len Maximum output length
 * @return 0 on success, -1 on error
 */
static inline int pinyin_syllable_to_ipa(const char* pinyin, char* ipa_out, size_t max_len) {
    if (!pinyin || !ipa_out || max_len < 32) return -1;
    
    ipa_out[0] = '\0';
    char lower[32];
    size_t len = strlen(pinyin);
    if (len >= sizeof(lower)) return -1;
    
    // Convert to lowercase and extract tone
    int tone = 5;  // Default neutral
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower(pinyin[i]);
    }
    lower[len] = '\0';
    
    // Extract tone number from end
    if (len > 0 && lower[len-1] >= '1' && lower[len-1] <= '5') {
        tone = lower[len-1] - '0';
        lower[len-1] = '\0';
        len--;
    }
    
    // Handle special case: standalone vowels
    if (len == 1) {
        for (size_t i = 0; i < PINYIN_FINALS_COUNT; i++) {
            if (strcmp(lower, PINYIN_FINALS[i].pinyin) == 0) {
                snprintf(ipa_out, max_len, "%s%s", PINYIN_FINALS[i].ipa, TONE_MARKERS[tone]);
                return 0;
            }
        }
    }
    
    // Try to match initial + final
    const char* initial_ipa = "";
    const char* final_ipa = NULL;
    size_t initial_len = 0;
    
    // Match longest initial first (zh, ch, sh before z, c, s)
    for (size_t i = 0; i < PINYIN_INITIALS_COUNT; i++) {
        size_t init_len = strlen(PINYIN_INITIALS[i].pinyin);
        if (init_len <= len && strncmp(lower, PINYIN_INITIALS[i].pinyin, init_len) == 0) {
            if (init_len > initial_len) {
                initial_ipa = PINYIN_INITIALS[i].ipa;
                initial_len = init_len;
            }
        }
    }
    
    // Match final (remaining part)
    const char* final_part = lower + initial_len;
    for (size_t i = 0; i < PINYIN_FINALS_COUNT; i++) {
        if (strcmp(final_part, PINYIN_FINALS[i].pinyin) == 0) {
            final_ipa = PINYIN_FINALS[i].ipa;
            break;
        }
    }
    
    if (!final_ipa) {
        // No final found, maybe it's just the initial (like "r" in "er4")
        if (initial_len == len) {
            snprintf(ipa_out, max_len, "%s%s", initial_ipa, TONE_MARKERS[tone]);
            return 0;
        }
        return -1;  // Invalid pinyin
    }
    
    // Combine initial + final + tone
    snprintf(ipa_out, max_len, "%s%s%s", initial_ipa, final_ipa, TONE_MARKERS[tone]);
    return 0;
}

#endif // ETHERVOX_PINYIN_TO_IPA_H
