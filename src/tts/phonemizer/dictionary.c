/**
 * @file dictionary.c
 * @brief Hash table-based pronunciation dictionary
 * 
 * Implements fast lookup using djb2 hash with chaining.
 * CMU Dict format: WORD  P1 P2 P3 ... (space/tab delimited)
 */

#include "dictionary.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define HASH_TABLE_SIZE 65536  // 2^16 buckets for ~134K entries
#define MAX_WORD_LENGTH 64
#define MAX_PRONUNCIATION_LENGTH 256

typedef struct dict_entry {
    char* word;
    char* pronunciation;  // ARPAbet space-separated
    struct dict_entry* next;  // Chaining for collisions
} dict_entry_t;

struct pronunciation_dict {
    dict_entry_t** table;
    size_t size;
    size_t entry_count;
};

/**
 * djb2 hash function
 */
static unsigned long hash_djb2(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        c = toupper(c);  // Case-insensitive
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

/**
 * Normalize word (uppercase, remove (n) variants)
 */
static void normalize_word(const char* input, char* output, size_t max_len) {
    size_t i = 0, j = 0;
    while (input[i] && j < max_len - 1) {
        if (input[i] == '(') {
            // Skip variant marker like (2)
            while (input[i] && input[i] != ')') i++;
            if (input[i] == ')') i++;
            break;
        }
        output[j++] = toupper(input[i++]);
    }
    output[j] = '\0';
}

dict_t* dict_load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        ETHERVOX_LOG_ERROR("Failed to open dictionary: %s\n", path);
        return NULL;
    }
    
    dict_t* dict = calloc(1, sizeof(dict_t));
    if (!dict) {
        fclose(f);
        return NULL;
    }
    
    dict->size = HASH_TABLE_SIZE;
    dict->table = calloc(dict->size, sizeof(dict_entry_t*));
    if (!dict->table) {
        free(dict);
        fclose(f);
        return NULL;
    }
    
    char line[512];
    char word[MAX_WORD_LENGTH];
    char pronunciation[MAX_PRONUNCIATION_LENGTH];
    size_t loaded = 0;
    
    while (fgets(line, sizeof(line), f)) {
        // Skip comments
        if (line[0] == ';' || line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        // Parse: WORD  P1 P2 P3
        char* space = strchr(line, ' ');
        if (!space) {
            space = strchr(line, '\t');
        }
        if (!space) continue;
        
        // Extract word
        size_t word_len = space - line;
        if (word_len >= MAX_WORD_LENGTH) continue;
        strncpy(word, line, word_len);
        word[word_len] = '\0';
        
        // Normalize word (uppercase, remove variants)
        char normalized[MAX_WORD_LENGTH];
        normalize_word(word, normalized, MAX_WORD_LENGTH);
        
        // Extract pronunciation (skip whitespace)
        const char* pron_start = space;
        while (*pron_start == ' ' || *pron_start == '\t') pron_start++;
        
        // Copy pronunciation, remove trailing newline
        strncpy(pronunciation, pron_start, MAX_PRONUNCIATION_LENGTH - 1);
        pronunciation[MAX_PRONUNCIATION_LENGTH - 1] = '\0';
        char* newline = strchr(pronunciation, '\n');
        if (newline) *newline = '\0';
        char* cr = strchr(pronunciation, '\r');
        if (cr) *cr = '\0';
        
        if (strlen(pronunciation) == 0) continue;
        
        // Hash and insert
        unsigned long hash = hash_djb2(normalized);
        size_t idx = hash % dict->size;
        
        dict_entry_t* entry = malloc(sizeof(dict_entry_t));
        if (!entry) continue;
        
        entry->word = strdup(normalized);
        entry->pronunciation = strdup(pronunciation);
        entry->next = dict->table[idx];
        dict->table[idx] = entry;
        
        loaded++;
    }
    
    fclose(f);
    dict->entry_count = loaded;
    
    ETHERVOX_LOG_INFO("Loaded %zu dictionary entries\n", loaded);
    return dict;
}

ethervox_result_t dict_lookup(dict_t* dict, const char* word, char* arpabet_out, size_t max_len) {
    ETHERVOX_CHECK_PTR(dict);
    ETHERVOX_CHECK_PTR(word);
    ETHERVOX_CHECK_PTR(arpabet_out);
    
    if (max_len == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Normalize input word
    char normalized[MAX_WORD_LENGTH];
    normalize_word(word, normalized, MAX_WORD_LENGTH);
    
    // Hash and search chain
    unsigned long hash = hash_djb2(normalized);
    size_t idx = hash % dict->size;
    
    dict_entry_t* entry = dict->table[idx];
    while (entry) {
        if (strcmp(entry->word, normalized) == 0) {
            strncpy(arpabet_out, entry->pronunciation, max_len - 1);
            arpabet_out[max_len - 1] = '\0';
            return ETHERVOX_SUCCESS;
        }
        entry = entry->next;
    }
    
    return ETHERVOX_ERROR_NOT_FOUND; // Not found
}

void dict_free(dict_t* dict) {
    if (!dict) return;
    
    for (size_t i = 0; i < dict->size; i++) {
        dict_entry_t* entry = dict->table[i];
        while (entry) {
            dict_entry_t* next = entry->next;
            free(entry->word);
            free(entry->pronunciation);
            free(entry);
            entry = next;
        }
    }
    
    free(dict->table);
    free(dict);
}
