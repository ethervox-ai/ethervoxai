/**
 * @file pronunciation_overrides.h
 * @brief User-trainable pronunciation override system
 * 
 * Allows users to correct mispronunciations by providing audio examples.
 * Overrides are stored with usage statistics and can be promoted to
 * community standards over time.
 * 
 * Storage hierarchy:
 * 1. Personal overrides: ~/.ethervox/pronunciation_overrides.json
 * 2. Community overrides: ~/.ethervox/community_overrides.json (auto-promoted)
 * 3. Core phonemizer: overrides_learned.c (merged in releases)
 */

#ifndef PRONUNCIATION_OVERRIDES_H
#define PRONUNCIATION_OVERRIDES_H

#include "ethervox/error.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_WORD_LENGTH 128
#define MAX_PHONEME_LENGTH 256
#define MAX_IPA_LENGTH 256

typedef struct {
    char word[MAX_WORD_LENGTH];
    char phonemes[MAX_PHONEME_LENGTH];  // ARPAbet (e.g., "IH N T AH G R EY T")
    char ipa[MAX_IPA_LENGTH];           // IPA (e.g., "ˈɪntəɡreɪt")
    uint32_t usage_count;
    float confidence;                    // 0.0-1.0, from audio similarity
    int trained_speaker_id;              // Which speaker was used for training
    time_t created;
    time_t last_used;
    bool is_community;                   // Promoted to community overrides
} pronunciation_override_t;

typedef struct pronunciation_override_store pronunciation_override_store_t;

/**
 * Load pronunciation overrides from disk
 * Loads both personal and community overrides
 * 
 * @return Override store or NULL on error
 */
pronunciation_override_store_t* pronunciation_overrides_load(void);

/**
 * Look up pronunciation override for a word
 * 
 * @param store Override store
 * @param word Word to look up (case-insensitive)
 * @param out_override Output override (caller allocates)
 * @return ETHERVOX_SUCCESS if found, ETHERVOX_ERROR_NOT_FOUND if not found
 */
ethervox_result_t pronunciation_overrides_lookup(
    pronunciation_override_store_t* store,
    const char* word,
    pronunciation_override_t* out_override
);

/**
 * Add or update pronunciation override
 * Increments usage_count if override already exists
 * 
 * @param store Override store
 * @param override Override to add/update
 * @return 0 on success, -1 on error
 */
int pronunciation_overrides_add(
    pronunciation_override_store_t* store,
    const pronunciation_override_t* override
);

/**
 * Record usage of an override (increments counter, updates timestamp)
 * 
 * @param store Override store
 * @param word Word that was used
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t pronunciation_overrides_record_usage(
    pronunciation_override_store_t* store,
    const char* word
);

/**
 * Save overrides to disk
 * Writes both personal and community override files
 * 
 * @param store Override store
 * @return 0 on success, -1 on error
 */
int pronunciation_overrides_save(pronunciation_override_store_t* store);

/**
 * Promote high-confidence overrides to community file
 * Criteria: usage_count >= 50, confidence >= 0.85
 * 
 * @param store Override store
 * @return Number of overrides promoted, or -1 on error
 */
int pronunciation_overrides_promote(pronunciation_override_store_t* store);

/**
 * Export stable community overrides for core phonemizer integration
 * Generates C code for overrides_learned.c
 * 
 * @param store Override store
 * @param output_path Path to write C code
 * @return Number of overrides exported, or -1 on error
 */
int pronunciation_overrides_export_to_core(
    pronunciation_override_store_t* store,
    const char* output_path
);

/**
 * Get statistics about override store
 * 
 * @param store Override store
 * @param total_overrides Output: total number of overrides
 * @param community_overrides Output: number of community overrides
 * @param avg_confidence Output: average confidence score
 */
void pronunciation_overrides_get_stats(
    pronunciation_override_store_t* store,
    int* total_overrides,
    int* community_overrides,
    float* avg_confidence
);

/**
 * Reset/clear all pronunciation overrides
 * Deletes both personal and community override files
 * 
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t pronunciation_overrides_reset(void);

/**
 * Free override store
 */
void pronunciation_overrides_free(pronunciation_override_store_t* store);

#endif // PRONUNCIATION_OVERRIDES_H
