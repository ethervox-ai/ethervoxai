/**
 * @file stress_reduction.h
 * @brief IPA stress reduction for natural connected speech
 */

#ifndef STRESS_REDUCTION_H
#define STRESS_REDUCTION_H

#include "ethervox/error.h"
#include <stddef.h>

/**
 * Context for stress reduction decisions
 */
typedef enum {
    STRESS_CONTEXT_ISOLATED,         // Single word (no reduction)
    STRESS_CONTEXT_SENTENCE_INITIAL, // First word in sentence
    STRESS_CONTEXT_SENTENCE_MEDIAL,  // Middle of sentence
    STRESS_CONTEXT_SENTENCE_FINAL,   // Last word in sentence
    STRESS_CONTEXT_QUESTION          // In a question (different stress patterns)
} stress_reduction_context_t;

/**
 * Apply stress reduction rules to IPA string
 * 
 * @param word Original word (for function word detection)
 * @param ipa IPA string (modified in-place)
 * @param max_len Maximum length of IPA buffer
 * @param context Sentence context for the word
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t apply_stress_reduction(
    const char* word,
    char* ipa,
    size_t max_len,
    stress_reduction_context_t context
);

// Needed for strcasecmp portability
int strcasecmp(const char* s1, const char* s2);

#endif // STRESS_REDUCTION_H
