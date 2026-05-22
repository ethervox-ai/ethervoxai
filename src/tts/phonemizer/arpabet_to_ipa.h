/**
 * @file arpabet_to_ipa.h
 * @brief ARPAbet to IPA phoneme conversion table
 * 
 * Conversion from CMU Dictionary ARPAbet notation to IPA.
 * Stress markers: 0=unstressed, 1=primary, 2=secondary
 */

#ifndef ETHERVOX_ARPABET_TO_IPA_H
#define ETHERVOX_ARPABET_TO_IPA_H

#include <string.h>

typedef struct {
    const char* arpabet;
    const char* ipa;
} arpabet_ipa_mapping_t;

// Vowels (with stress variants)
static const arpabet_ipa_mapping_t ARPABET_VOWELS[] = {
    // Monophthongs
    {"AA0", "ɑ"},   {"AA1", "ˈɑ"},   {"AA2", "ˌɑ"},
    {"AE0", "æ"},   {"AE1", "ˈæ"},   {"AE2", "ˌæ"},
    {"AH0", "ə"},   {"AH1", "ˈʌ"},   {"AH2", "ˌʌ"},
    {"AO0", "ɔ"},   {"AO1", "ˈɔ"},   {"AO2", "ˌɔ"},
    {"AW0", "aʊ"},  {"AW1", "ˈaʊ"},  {"AW2", "ˌaʊ"},
    {"AY0", "aɪ"},  {"AY1", "ˈaɪ"},  {"AY2", "ˌaɪ"},
    {"EH0", "ɛ"},   {"EH1", "ˈɛ"},   {"EH2", "ˌɛ"},
    {"ER0", "ɚ"},   {"ER1", "ˈɝ"},   {"ER2", "ˌɝ"},
    {"EY0", "eɪ"},  {"EY1", "ˈeɪ"},  {"EY2", "ˌeɪ"},
    {"IH0", "ɪ"},   {"IH1", "ˈɪ"},   {"IH2", "ˌɪ"},
    {"IY0", "i"},   {"IY1", "ˈi"},   {"IY2", "ˌi"},
    {"OW0", "oʊ"},  {"OW1", "ˈoʊ"},  {"OW2", "ˌoʊ"},
    {"OY0", "ɔɪ"},  {"OY1", "ˈɔɪ"},  {"OY2", "ˌɔɪ"},
    {"UH0", "ʊ"},   {"UH1", "ˈʊ"},   {"UH2", "ˌʊ"},
    {"UW0", "u"},   {"UW1", "ˈu"},   {"UW2", "ˌu"},
};

// Consonants (no stress)
static const arpabet_ipa_mapping_t ARPABET_CONSONANTS[] = {
    {"B", "b"},
    {"CH", "tʃ"},
    {"D", "d"},
    {"DH", "ð"},
    {"F", "f"},
    {"G", "ɡ"},
    {"HH", "h"},
    {"JH", "dʒ"},
    {"K", "k"},
    {"L", "l"},
    {"M", "m"},
    {"N", "n"},
    {"NG", "ŋ"},
    {"P", "p"},
    {"R", "ɹ"},
    {"S", "s"},
    {"SH", "ʃ"},
    {"T", "t"},
    {"TH", "θ"},
    {"V", "v"},
    {"W", "w"},
    {"Y", "j"},
    {"Z", "z"},
    {"ZH", "ʒ"},
};

#define ARPABET_VOWEL_COUNT (sizeof(ARPABET_VOWELS) / sizeof(ARPABET_VOWELS[0]))
#define ARPABET_CONSONANT_COUNT (sizeof(ARPABET_CONSONANTS) / sizeof(ARPABET_CONSONANTS[0]))

/**
 * Convert ARPAbet phoneme to IPA
 * @return IPA string or NULL if not found
 */
static inline const char* arpabet_to_ipa(const char* arpabet) {
    // Try vowels first (more entries due to stress variants)
    for (size_t i = 0; i < ARPABET_VOWEL_COUNT; i++) {
        if (strcmp(arpabet, ARPABET_VOWELS[i].arpabet) == 0) {
            return ARPABET_VOWELS[i].ipa;
        }
    }
    
    // Try consonants
    for (size_t i = 0; i < ARPABET_CONSONANT_COUNT; i++) {
        if (strcmp(arpabet, ARPABET_CONSONANTS[i].arpabet) == 0) {
            return ARPABET_CONSONANTS[i].ipa;
        }
    }
    
    return NULL; // Not found
}

#endif // ETHERVOX_ARPABET_TO_IPA_H
