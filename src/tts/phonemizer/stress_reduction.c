/**
 * @file stress_reduction.c
 * @brief IPA stress reduction for natural connected speech
 * 
 * Implements rules for reducing stress marks on function words to produce
 * more natural-sounding phonemization that matches Piper's training data.
 * 
 * In natural English speech, only content words (nouns, verbs, adjectives, etc.)
 * carry primary stress. Function words (articles, prepositions, pronouns, etc.)
 * are typically unstressed or have reduced vowels.
 */

#include "stress_reduction.h"
#include "ethervox/error.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Function words that should be FULLY unstressed (stress removed entirely)
static const char* FULLY_UNSTRESSED_WORDS[] = {
    // Articles
    "a", "an", "the",
    
    // Most common prepositions
    "of", "to", "for", "with", "on", "at", "from", "by",
    "in", "as",
    
    // Pronouns (except question words)
    "i", "you", "he", "she", "it", "we", "they", "me", "him", "her",
    "us", "them", "my", "your", "his", "its", "our", "their",
    
    // Conjunctions
    "and", "but", "or", "nor", "so", "yet",
    
    // Auxiliary verbs
    "am", "is", "are", "was", "were", "be", "been", "being",
    "have", "has", "had", "do", "does", "did",
    "will", "would", "shall", "should", "may", "might", 
    "must", "can", "could",
    
    NULL  // Sentinel
};

// Function words that should have SECONDARY stress (not primary)
// These carry some prosodic weight but not full stress
static const char* SECONDARY_STRESS_WORDS[] = {
    // Locative/demonstrative adverbs (can carry some stress)
    // NOTE: "there" removed - keeps primary stress at phrase boundaries
    "here", "where", "when", "how",
    "then", "now",
    
    // Quantifiers and degree adverbs
    "very", "too", "so", "just", "even", "still",
    "also", "only", "more", "most", "some", "any",
    "all", "both", "each", "few", "many", "much",
    
    // Other prepositions that can carry stress
    "about", "into", "like", "through", "after", "over", 
    "between", "out", "against", "during", "without", 
    "before", "under", "around", "among", "up", "down", "off",
    
    // Question words (carry stress in questions)
    "what", "which", "who", "whom", "whose", "why",
    
    // Other function words with some prosodic weight
    "this", "that", "these", "those",
    "if", "than", "other", "such", "own", "same",
    "not", "no", "yes",
    
    NULL  // Sentinel
};

/**
 * Check if a word should be fully unstressed
 */
static bool is_fully_unstressed_word(const char* word) {
    if (!word || !*word) return false;
    
    char lower[64];
    size_t len = strlen(word);
    if (len >= sizeof(lower)) return false;
    
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)word[i]);
    }
    lower[len] = '\0';
    
    for (int i = 0; FULLY_UNSTRESSED_WORDS[i] != NULL; i++) {
        if (strcmp(lower, FULLY_UNSTRESSED_WORDS[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

/**
 * Check if a word should have secondary stress instead of primary
 */
static bool is_secondary_stress_word(const char* word) {
    if (!word || !*word) return false;
    
    char lower[64];
    size_t len = strlen(word);
    if (len >= sizeof(lower)) return false;
    
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)word[i]);
    }
    lower[len] = '\0';
    
    for (int i = 0; SECONDARY_STRESS_WORDS[i] != NULL; i++) {
        if (strcmp(lower, SECONDARY_STRESS_WORDS[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

/**
 * Remove primary stress marks (ˈ) from IPA string
 * Keeps secondary stress (ˌ) for more natural variation
 */
static void remove_primary_stress(char* ipa) {
    if (!ipa) return;
    
    char* src = ipa;
    char* dst = ipa;
    
    // UTF-8 encoding of ˈ (U+02C8) is: 0xCB 0x88
    while (*src) {
        // Check for UTF-8 sequence of primary stress mark
        if ((unsigned char)src[0] == 0xCB && 
            (unsigned char)src[1] == 0x88) {
            // Skip the primary stress mark (2 bytes in UTF-8)
            src += 2;
        } else {
            // Copy character (might be multi-byte UTF-8)
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * Convert primary stress (ˈ) to secondary stress (ˌ) in IPA string
 * For words that should have reduced but not eliminated stress
 */
static void primary_to_secondary_stress(char* ipa) {
    if (!ipa) return;
    
    // UTF-8 encoding of ˈ (U+02C8) is: 0xCB 0x88
    // UTF-8 encoding of ˌ (U+02CC) is: 0xCB 0x8C
    char* p = ipa;
    while (*p) {
        if ((unsigned char)p[0] == 0xCB && 
            (unsigned char)p[1] == 0x88) {
            // Replace primary stress with secondary stress
            p[1] = 0x8C;
            p += 2;
        } else {
            p++;
        }
    }
}

ethervox_result_t apply_stress_reduction(
    const char* word,
    char* ipa,
    size_t max_len,
    stress_reduction_context_t context
) {
    ETHERVOX_CHECK_PTR(word);
    ETHERVOX_CHECK_PTR(ipa);
    
    if (max_len == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if word should be fully unstressed or have secondary stress
    bool should_remove_stress = false;
    bool should_reduce_to_secondary = false;
    
    switch (context) {
        case STRESS_CONTEXT_SENTENCE_MEDIAL:
        case STRESS_CONTEXT_SENTENCE_FINAL:
            // In middle/end of sentence, apply full stress reduction hierarchy
            if (is_fully_unstressed_word(word)) {
                should_remove_stress = true;
            } else if (is_secondary_stress_word(word)) {
                should_reduce_to_secondary = true;
            }
            break;
            
        case STRESS_CONTEXT_SENTENCE_INITIAL:
            // At sentence start, be more conservative
            // Only fully unstressed words lose stress, others keep primary
            if (is_fully_unstressed_word(word)) {
                // Exception: "I", "We", "The" keep stress at sentence start
                if (strcasecmp(word, "i") != 0 && 
                    strcasecmp(word, "we") != 0 &&
                    strcasecmp(word, "the") != 0) {
                    should_remove_stress = true;
                }
            } else if (is_secondary_stress_word(word)) {
                // Exception: Question words (how, what, where, etc.) ALWAYS get
                // secondary stress even at sentence start, since they signal questions
                // even without punctuation. Matches espeak's behavior.
                should_reduce_to_secondary = true;
            }
            break;
            
        case STRESS_CONTEXT_QUESTION:
            // In questions, apply normal reduction but adjust for question words
            if (is_fully_unstressed_word(word)) {
                should_remove_stress = true;
            } else if (is_secondary_stress_word(word)) {
                // Question words at the START of a question get secondary stress
                // (This matches espeak's behavior: "how are you?" → hˌaʊ)
                // But keep primary if it's a content-bearing question word in other positions
                should_reduce_to_secondary = true;
            }
            break;
            
        case STRESS_CONTEXT_ISOLATED:
        default:
            // Don't reduce stress for isolated words
            break;
    }
    
    if (should_remove_stress) {
        remove_primary_stress(ipa);
    } else if (should_reduce_to_secondary) {
        primary_to_secondary_stress(ipa);
    }
    
    return ETHERVOX_SUCCESS;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
