/**
 * @file pronunciation_overrides.c
 * @brief Implementation of user-trainable pronunciation overrides
 */

#include "pronunciation_overrides.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_OVERRIDES 10000
#define PROMOTION_THRESHOLD_USAGE 50
#define PROMOTION_THRESHOLD_CONFIDENCE 0.85f
#define CORE_EXPORT_THRESHOLD_USAGE 100

struct pronunciation_override_store {
    pronunciation_override_t* overrides;
    int count;
    int capacity;
    char personal_path[512];
    char community_path[512];
};

/**
 * Ensure directory exists
 */
static int ensure_directory(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef _WIN32
#include <direct.h>  // for _mkdir on Windows
        if (_mkdir(path) != 0 && errno != EEXIST) {
#else
        if (mkdir(path, 0755) != 0 && errno != EEXIST) {
#endif
            return -1;
        }
    }
    return 0;
}

/**
 * Normalize word to lowercase for case-insensitive lookup
 */
static void normalize_word(const char* input, char* output, size_t output_size) {
    size_t i;
    for (i = 0; i < output_size - 1 && input[i]; i++) {
        output[i] = tolower(input[i]);
    }
    output[i] = '\0';
}

/**
 * Parse single override from JSON object
 */
static int parse_override_json(cJSON* item, pronunciation_override_t* override, bool is_community) {
    if (!item || !override) return -1;
    
    const char* word = item->string;
    if (!word) return -1;
    
    normalize_word(word, override->word, sizeof(override->word));
    
    cJSON* phonemes = cJSON_GetObjectItem(item, "phonemes");
    cJSON* ipa = cJSON_GetObjectItem(item, "ipa");
    cJSON* usage_count = cJSON_GetObjectItem(item, "usage_count");
    cJSON* confidence = cJSON_GetObjectItem(item, "confidence");
    cJSON* speaker_id = cJSON_GetObjectItem(item, "trained_speaker_id");
    cJSON* created = cJSON_GetObjectItem(item, "created");
    cJSON* last_used = cJSON_GetObjectItem(item, "last_used");
    
    if (!phonemes || !cJSON_IsString(phonemes)) return -1;
    
    strncpy(override->phonemes, phonemes->valuestring, sizeof(override->phonemes) - 1);
    
    if (ipa && cJSON_IsString(ipa)) {
        strncpy(override->ipa, ipa->valuestring, sizeof(override->ipa) - 1);
    }
    
    override->usage_count = usage_count && cJSON_IsNumber(usage_count) ? 
                           usage_count->valueint : 1;
    override->confidence = confidence && cJSON_IsNumber(confidence) ? 
                          (float)confidence->valuedouble : 0.5f;
    override->trained_speaker_id = speaker_id && cJSON_IsNumber(speaker_id) ? 
                                   speaker_id->valueint : 0;
    override->created = created && cJSON_IsNumber(created) ? 
                       (time_t)created->valueint : time(NULL);
    override->last_used = last_used && cJSON_IsNumber(last_used) ? 
                         (time_t)last_used->valueint : time(NULL);
    override->is_community = is_community;
    
    return 0;
}

/**
 * Load overrides from JSON file
 */
static int load_overrides_file(const char* path, pronunciation_override_store_t* store, bool is_community) {
    FILE* f = fopen(path, "r");
    if (!f) {
        // File doesn't exist yet - not an error
        return 0;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 10 * 1024 * 1024) {  // Max 10MB
        fclose(f);
        return -1;
    }
    
    char* json_str = malloc(size + 1);
    if (!json_str) {
        fclose(f);
        return -1;
    }
    
    fread(json_str, 1, size, f);
    json_str[size] = '\0';
    fclose(f);
    
    cJSON* root = cJSON_Parse(json_str);
    free(json_str);
    
    if (!root) {
        ETHERVOX_LOG_DEBUG("[PronOverrides] Failed to parse JSON: %s\n", path);
        return -1;
    }
    
    // Iterate through all items in the JSON object
    cJSON* item = root->child;
    int loaded = 0;
    
    while (item && store->count < store->capacity) {
        pronunciation_override_t override = {0};
        if (parse_override_json(item, &override, is_community) == 0) {
            store->overrides[store->count++] = override;
            loaded++;
        }
        item = item->next;
    }
    
    cJSON_Delete(root);
    
    ETHERVOX_LOG_DEBUG("[PronOverrides] Loaded %d overrides from %s\n", loaded, path);
    return loaded;
}

pronunciation_override_store_t* pronunciation_overrides_load(void) {
    pronunciation_override_store_t* store = calloc(1, sizeof(pronunciation_override_store_t));
    if (!store) return NULL;
    
    store->capacity = MAX_OVERRIDES;
    store->overrides = calloc(store->capacity, sizeof(pronunciation_override_t));
    if (!store->overrides) {
        free(store);
        return NULL;
    }
    
    // Get paths
    const char* home = getenv("HOME");
    if (!home) home = ".";
    
    snprintf(store->personal_path, sizeof(store->personal_path), 
             "%s/.ethervox/pronunciation_overrides.json", home);
    snprintf(store->community_path, sizeof(store->community_path),
             "%s/.ethervox/community_overrides.json", home);
    
    // Ensure directory exists
    char ethervox_dir[512];
    snprintf(ethervox_dir, sizeof(ethervox_dir), "%s/.ethervox", home);
    ensure_directory(ethervox_dir);
    
    // Load community overrides first (lower priority)
    load_overrides_file(store->community_path, store, true);
    
    // Load personal overrides (higher priority, can override community)
    load_overrides_file(store->personal_path, store, false);
    
    return store;
}

ethervox_result_t pronunciation_overrides_lookup(
    pronunciation_override_store_t* store,
    const char* word,
    pronunciation_override_t* out_override
) {
    ETHERVOX_CHECK_PTR(store);
    ETHERVOX_CHECK_PTR(word);
    ETHERVOX_CHECK_PTR(out_override);
    
    char normalized[MAX_WORD_LENGTH];
    normalize_word(word, normalized, sizeof(normalized));
    
    ETHERVOX_LOG_DEBUG("[PronOverrides] Looking up '%s' (normalized: '%s') in %d overrides\n",
            word, normalized, store->count);
    
    // Search in reverse (personal overrides loaded last, higher priority)
    for (int i = store->count - 1; i >= 0; i--) {
        if (strcmp(store->overrides[i].word, normalized) == 0) {
            ETHERVOX_LOG_DEBUG("[PronOverrides] 🎯 Found match at index %d: ipa='%s' confidence=%.3f\n",
                    i, store->overrides[i].ipa, store->overrides[i].confidence);
            *out_override = store->overrides[i];
            return ETHERVOX_SUCCESS;
        }
    }
    
    ETHERVOX_LOG_DEBUG("[PronOverrides] ❌ No match found for '%s'\n", normalized);
    
    return ETHERVOX_ERROR_NOT_FOUND;
}

ethervox_result_t pronunciation_overrides_add(
    pronunciation_override_store_t* store,
    const pronunciation_override_t* override
) {
    ETHERVOX_CHECK_PTR(store);
    ETHERVOX_CHECK_PTR(override);
    
    // Check if override already exists
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->overrides[i].word, override->word) == 0 &&
            !store->overrides[i].is_community) {  // Only update personal overrides
            // Update existing override
            store->overrides[i].usage_count++;
            store->overrides[i].last_used = time(NULL);
            
            // Update phonemes if confidence is higher
            if (override->confidence > store->overrides[i].confidence) {
                strncpy(store->overrides[i].phonemes, override->phonemes, 
                       sizeof(store->overrides[i].phonemes) - 1);
                strncpy(store->overrides[i].ipa, override->ipa,
                       sizeof(store->overrides[i].ipa) - 1);
                store->overrides[i].confidence = override->confidence;
                store->overrides[i].trained_speaker_id = override->trained_speaker_id;
            }
            
            return ETHERVOX_SUCCESS;
        }
    }
    
    // Add new override
    if (store->count >= store->capacity) {
        ETHERVOX_LOG_DEBUG("[PronOverrides] Store full, cannot add more overrides");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    store->overrides[store->count++] = *override;
    store->overrides[store->count - 1].created = time(NULL);
    store->overrides[store->count - 1].last_used = time(NULL);
    store->overrides[store->count - 1].is_community = false;
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t pronunciation_overrides_record_usage(
    pronunciation_override_store_t* store,
    const char* word
) {
    ETHERVOX_CHECK_PTR(store);
    ETHERVOX_CHECK_PTR(word);
    
    char normalized[MAX_WORD_LENGTH];
    normalize_word(word, normalized, sizeof(normalized));
    
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->overrides[i].word, normalized) == 0) {
            store->overrides[i].usage_count++;
            store->overrides[i].last_used = time(NULL);
            return ETHERVOX_SUCCESS;
        }
    }
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}

/**
 * Convert override to JSON object
 */
static cJSON* override_to_json(const pronunciation_override_t* override) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;
    
    cJSON_AddStringToObject(obj, "phonemes", override->phonemes);
    if (override->ipa[0]) {
        cJSON_AddStringToObject(obj, "ipa", override->ipa);
    }
    cJSON_AddNumberToObject(obj, "usage_count", override->usage_count);
    cJSON_AddNumberToObject(obj, "confidence", override->confidence);
    cJSON_AddNumberToObject(obj, "trained_speaker_id", override->trained_speaker_id);
    cJSON_AddNumberToObject(obj, "created", (double)override->created);
    cJSON_AddNumberToObject(obj, "last_used", (double)override->last_used);
    
    return obj;
}

ethervox_result_t pronunciation_overrides_save(pronunciation_override_store_t* store) {
    ETHERVOX_CHECK_PTR(store);
    
    cJSON* personal_root = cJSON_CreateObject();
    cJSON* community_root = cJSON_CreateObject();
    
    if (!personal_root || !community_root) {
        if (personal_root) cJSON_Delete(personal_root);
        if (community_root) cJSON_Delete(community_root);
        return ETHERVOX_ERROR_FILE_WRITE;
    }
    
    // Separate personal and community overrides
    for (int i = 0; i < store->count; i++) {
        pronunciation_override_t* override = &store->overrides[i];
        cJSON* obj = override_to_json(override);
        if (!obj) continue;
        
        if (override->is_community) {
            cJSON_AddItemToObject(community_root, override->word, obj);
        } else {
            cJSON_AddItemToObject(personal_root, override->word, obj);
        }
    }
    
    // Write personal overrides
    char* personal_json = cJSON_Print(personal_root);
    if (personal_json) {
        FILE* f = fopen(store->personal_path, "w");
        if (f) {
            fprintf(f, "%s\n", personal_json);
            fclose(f);
        }
        free(personal_json);
    }
    
    // Write community overrides
    char* community_json = cJSON_Print(community_root);
    if (community_json) {
        FILE* f = fopen(store->community_path, "w");
        if (f) {
            fprintf(f, "%s\n", community_json);
            fclose(f);
        }
        free(community_json);
    }
    
    cJSON_Delete(personal_root);
    cJSON_Delete(community_root);
    
    ETHERVOX_LOG_DEBUG("[PronOverrides] Saved to %s and %s\n", 
           store->personal_path, store->community_path);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t pronunciation_overrides_promote(pronunciation_override_store_t* store) {
    ETHERVOX_CHECK_PTR(store);
    
    int promoted = 0;
    
    for (int i = 0; i < store->count; i++) {
        pronunciation_override_t* override = &store->overrides[i];
        
        if (!override->is_community &&
            override->usage_count >= PROMOTION_THRESHOLD_USAGE &&
            override->confidence >= PROMOTION_THRESHOLD_CONFIDENCE) {
            
            override->is_community = true;
            promoted++;
            
            printf("[PronOverrides] Promoted '%s' to community (usage=%u, confidence=%.2f)\n",
                   override->word, override->usage_count, override->confidence);
        }
    }
    
    if (promoted > 0) {
        pronunciation_overrides_save(store);
    }
    
    return promoted;
}

ethervox_result_t pronunciation_overrides_export_to_core(
    pronunciation_override_store_t* store,
    const char* output_path
) {
    ETHERVOX_CHECK_PTR(store);
    ETHERVOX_CHECK_PTR(output_path);
    
    FILE* f = fopen(output_path, "w");
    if (!f) return ETHERVOX_ERROR_FILE_WRITE;
    
    fprintf(f, "/**\n");
    fprintf(f, " * @file overrides_learned.c\n");
    fprintf(f, " * @brief Auto-generated pronunciation overrides from community feedback\n");
    fprintf(f, " * \n");
    fprintf(f, " * This file contains high-confidence pronunciation corrections\n");
    fprintf(f, " * learned from user feedback. Generated automatically by\n");
    fprintf(f, " * pronunciation_overrides_export_to_core().\n");
    fprintf(f, " */\n\n");
    fprintf(f, "#include \"dictionary.h\"\n\n");
    fprintf(f, "// Community-learned pronunciation overrides\n");
    fprintf(f, "// Format: { word, phonemes, usage_count }\n");
    fprintf(f, "static const struct {\n");
    fprintf(f, "    const char* word;\n");
    fprintf(f, "    const char* phonemes;\n");
    fprintf(f, "    int usage_count;\n");
    fprintf(f, "} learned_overrides[] = {\n");
    
    int exported = 0;
    for (int i = 0; i < store->count; i++) {
        pronunciation_override_t* override = &store->overrides[i];
        
        if (override->is_community && 
            override->usage_count >= CORE_EXPORT_THRESHOLD_USAGE) {
            fprintf(f, "    { \"%s\", \"%s\", %u },\n",
                   override->word, override->phonemes, override->usage_count);
            exported++;
        }
    }
    
    fprintf(f, "};\n\n");
    fprintf(f, "int get_learned_overrides_count(void) {\n");
    fprintf(f, "    return %d;\n", exported);
    fprintf(f, "}\n");
    
    fclose(f);
    
    ETHERVOX_LOG_DEBUG("[PronOverrides] Exported %d overrides to %s\n", exported, output_path);
    return exported;
}

void pronunciation_overrides_get_stats(
    pronunciation_override_store_t* store,
    int* total_overrides,
    int* community_overrides,
    float* avg_confidence
) {
    if (!store) return;
    
    int total = 0;
    int community = 0;
    float confidence_sum = 0.0f;
    
    for (int i = 0; i < store->count; i++) {
        total++;
        if (store->overrides[i].is_community) community++;
        confidence_sum += store->overrides[i].confidence;
    }
    
    if (total_overrides) *total_overrides = total;
    if (community_overrides) *community_overrides = community;
    if (avg_confidence) *avg_confidence = total > 0 ? confidence_sum / total : 0.0f;
}

ethervox_result_t pronunciation_overrides_reset(void) {
    const char* home = getenv("HOME");
    if (!home) home = ".";
    
    char personal_path[512];
    char community_path[512];
    
    snprintf(personal_path, sizeof(personal_path), 
             "%s/.ethervox/pronunciation_overrides.json", home);
    snprintf(community_path, sizeof(community_path),
             "%s/.ethervox/community_overrides.json", home);
    
    int result = 0;
    
    // Delete personal overrides file
    if (remove(personal_path) == 0) {
        printf("[OK] Deleted personal pronunciation overrides\n");
    } else if (errno != ENOENT) {
        fprintf(stderr, "⚠️  Failed to delete %s: %s\n", personal_path, strerror(errno));
        result = -1;
    }
    
    // Delete community overrides file
    if (remove(community_path) == 0) {
        printf("[OK] Deleted community pronunciation overrides\n");
    } else if (errno != ENOENT) {
        fprintf(stderr, "⚠️  Failed to delete %s: %s\n", community_path, strerror(errno));
        result = -1;
    }
    
    if (result == 0) {
        printf("✅ Pronunciation override system reset successfully\n");
    }
    
    return result;
}

void pronunciation_overrides_free(pronunciation_override_store_t* store) {
    if (!store) return;
    
    free(store->overrides);
    free(store);
}
