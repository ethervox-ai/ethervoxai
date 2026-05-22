/**
 * @file dict_chinese.c
 * @brief Unicode Unihan dictionary implementation
 * 
 * Hash table for Chinese character→Pinyin lookup.
 * Format: U+4F60\tkMandarin\tnǐ
 * 
 * License: Unicode License v3 (permissive, similar to MIT)
 * Source: https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip
 */

#include "dict_chinese.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_TABLE_SIZE 65536   // 2^16 buckets for ~44K characters
#define MAX_CHAR_LENGTH 8       // Max UTF-8 bytes for one character
#define MAX_PINYIN_LENGTH 32    // Max pinyin length per character

typedef struct dict_entry {
    char* character;    // Single Chinese character (UTF-8)
    char* pinyin;       // Pinyin with Unicode tone marks (e.g., "nǐ")
    struct dict_entry* next;
} dict_entry_t;

struct chinese_dict {
    dict_entry_t** table;
    size_t size;
    size_t entry_count;
};

/**
 * FNV-1a hash for UTF-8 strings
 */
static unsigned long hash_fnv1a(const char* str) {
    unsigned long hash = 2166136261UL;
    const unsigned char* p = (const unsigned char*)str;
    while (*p) {
        hash ^= *p++;
        hash *= 16777619UL;
    }
    return hash;
}

/**
 * Convert Unicode tone marks to numeric tones (Unihan format → CC-CEDICT format)
 * Example: "nǐ" → "ni3", "hǎo" → "hao3"
 * 
 * Tone marks by Unicode encoding:
 * Tone 1 (ā): 0xC481, 0xC493, 0xC4AB, 0xC58D, 0xC5AB, 0xC7AB
 * Tone 2 (á): 0xC3A1, 0xC3A9, 0xC3AD, 0xC3B3, 0xC3BA, 0xC798
 * Tone 3 (ǎ): 0xC78E, 0xC79A, 0xC790, 0xC792, 0xC794, 0xC79A
 * Tone 4 (à): 0xC3A0, 0xC3A8, 0xC3AC, 0xC3B2, 0xC3B9, 0xC79C
 */
static void convert_tone_marks_to_numbers(const char* input, char* output, size_t max_len) {
    size_t out_idx = 0;
    const unsigned char* p = (const unsigned char*)input;
    int tone = 5;  // Default neutral tone
    
    while (*p && out_idx < max_len - 2) {
        int matched = 0;
        
        // Two-byte UTF-8 sequences for pinyin tone marks
        if (p[0] >= 0xC0 && p[1]) {
            unsigned short code = (p[0] << 8) | p[1];
            
            // Tone 1: ā ē ī ō ū ǖ
            if (code == 0xC481) { output[out_idx++] = 'a'; tone = 1; matched = 2; }
            else if (code == 0xC493) { output[out_idx++] = 'e'; tone = 1; matched = 2; }
            else if (code == 0xC4AB) { output[out_idx++] = 'i'; tone = 1; matched = 2; }
            else if (code == 0xC58D) { output[out_idx++] = 'o'; tone = 1; matched = 2; }
            else if (code == 0xC5AB) { output[out_idx++] = 'u'; tone = 1; matched = 2; }
            else if (code == 0xC7AB) { output[out_idx++] = 'v'; tone = 1; matched = 2; }
            
            // Tone 2: á é í ó ú ǘ
            else if (code == 0xC3A1) { output[out_idx++] = 'a'; tone = 2; matched = 2; }
            else if (code == 0xC3A9) { output[out_idx++] = 'e'; tone = 2; matched = 2; }
            else if (code == 0xC3AD) { output[out_idx++] = 'i'; tone = 2; matched = 2; }
            else if (code == 0xC3B3) { output[out_idx++] = 'o'; tone = 2; matched = 2; }
            else if (code == 0xC3BA) { output[out_idx++] = 'u'; tone = 2; matched = 2; }
            else if (code == 0xC798) { output[out_idx++] = 'v'; tone = 2; matched = 2; }
            
            // Tone 3: ǎ ě ǐ ǒ ǔ ǚ
            else if (code == 0xC78E) { output[out_idx++] = 'a'; tone = 3; matched = 2; }
            else if (code == 0xC79A) { output[out_idx++] = 'e'; tone = 3; matched = 2; }
            else if (code == 0xC790) { output[out_idx++] = 'i'; tone = 3; matched = 2; }
            else if (code == 0xC792) { output[out_idx++] = 'o'; tone = 3; matched = 2; }
            else if (code == 0xC794) { output[out_idx++] = 'u'; tone = 3; matched = 2; }
            else if (code == 0xC79A) { output[out_idx++] = 'v'; tone = 3; matched = 2; }
            
            // Tone 4: à è ì ò ù ǜ
            else if (code == 0xC3A0) { output[out_idx++] = 'a'; tone = 4; matched = 2; }
            else if (code == 0xC3A8) { output[out_idx++] = 'e'; tone = 4; matched = 2; }
            else if (code == 0xC3AC) { output[out_idx++] = 'i'; tone = 4; matched = 2; }
            else if (code == 0xC3B2) { output[out_idx++] = 'o'; tone = 4; matched = 2; }
            else if (code == 0xC3B9) { output[out_idx++] = 'u'; tone = 4; matched = 2; }
            else if (code == 0xC79C) { output[out_idx++] = 'v'; tone = 4; matched = 2; }
            
            // Neutral ü
            else if (code == 0xC3BC) { output[out_idx++] = 'v'; matched = 2; }
        }
        
        // If no tone mark matched, copy ASCII character
        if (!matched) {
            output[out_idx++] = *p++;
        } else {
            p += matched;
        }
    }
    
    // Append tone number
    if (tone >= 1 && tone <= 4) {
        output[out_idx++] = '0' + tone;
    }
    output[out_idx] = '\0';
}

dict_chinese_t* dict_chinese_load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        ETHERVOX_LOG_ERROR("Failed to open Unihan: %s\n", path);
        return NULL;
    }
    
    dict_chinese_t* dict = calloc(1, sizeof(dict_chinese_t));
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
    
    char line[1024];
    char character[MAX_CHAR_LENGTH];
    char pinyin[MAX_PINYIN_LENGTH];
    size_t loaded = 0;
    
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Parse format: U+4F60\tkMandarin\tnǐ
        // Only process kMandarin lines
        if (!strstr(line, "kMandarin")) continue;
        
        // Find first tab (after U+XXXX)
        char* tab1 = strchr(line, '\t');
        if (!tab1) continue;
        
        // Parse Unicode code point (U+XXXX)
        unsigned int codepoint;
        if (sscanf(line, "U+%X", &codepoint) != 1) continue;
        
        // Convert codepoint to UTF-8
        int char_len = 0;
        if (codepoint < 0x80) {
            character[char_len++] = (char)codepoint;
        } else if (codepoint < 0x800) {
            character[char_len++] = (char)(0xC0 | (codepoint >> 6));
            character[char_len++] = (char)(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            character[char_len++] = (char)(0xE0 | (codepoint >> 12));
            character[char_len++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            character[char_len++] = (char)(0x80 | (codepoint & 0x3F));
        } else {
            character[char_len++] = (char)(0xF0 | (codepoint >> 18));
            character[char_len++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
            character[char_len++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            character[char_len++] = (char)(0x80 | (codepoint & 0x3F));
        }
        character[char_len] = '\0';
        
        // Find second tab (before pinyin)
        char* tab2 = strchr(tab1 + 1, '\t');
        if (!tab2) continue;
        
        // Extract pinyin (after second tab, trim newline)
        char* pinyin_start = tab2 + 1;
        char* pinyin_end = strchr(pinyin_start, '\n');
        if (!pinyin_end) pinyin_end = pinyin_start + strlen(pinyin_start);
        
        size_t pinyin_len = pinyin_end - pinyin_start;
        if (pinyin_len >= MAX_PINYIN_LENGTH) continue;
        
        // Copy Unicode pinyin first
        char unicode_pinyin[MAX_PINYIN_LENGTH];
        strncpy(unicode_pinyin, pinyin_start, pinyin_len);
        unicode_pinyin[pinyin_len] = '\0';
        
        // Trim trailing whitespace from Unicode version
        while (pinyin_len > 0 && (unicode_pinyin[pinyin_len-1] == ' ' || unicode_pinyin[pinyin_len-1] == '\r')) {
            unicode_pinyin[--pinyin_len] = '\0';
        }
        
        // Convert Unicode tone marks to numeric tones (nǐ → ni3)
        convert_tone_marks_to_numbers(unicode_pinyin, pinyin, MAX_PINYIN_LENGTH);
        
        // Hash and insert
        unsigned long hash = hash_fnv1a(character);
        size_t idx = hash % dict->size;
        
        dict_entry_t* entry = malloc(sizeof(dict_entry_t));
        if (!entry) continue;
        
        entry->character = strdup(character);
        entry->pinyin = strdup(pinyin);
        entry->next = dict->table[idx];
        dict->table[idx] = entry;
        
        loaded++;
    }
    
    fclose(f);
    dict->entry_count = loaded;
    
    printf("Loaded %zu Chinese character pronunciations (Unihan)\n", loaded);
    return dict;
}

ethervox_result_t dict_chinese_lookup(dict_chinese_t* dict, const char* character, char* pinyin_out, size_t max_len) {
    ETHERVOX_CHECK_PTR(dict);
    ETHERVOX_CHECK_PTR(character);
    ETHERVOX_CHECK_PTR(pinyin_out);
    
    if (max_len == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    unsigned long hash = hash_fnv1a(character);
    size_t idx = hash % dict->size;
    
    dict_entry_t* entry = dict->table[idx];
    while (entry) {
        if (strcmp(entry->character, character) == 0) {
            strncpy(pinyin_out, entry->pinyin, max_len - 1);
            pinyin_out[max_len - 1] = '\0';
            return ETHERVOX_SUCCESS;
        }
        entry = entry->next;
    }
    
    return ETHERVOX_ERROR_NOT_FOUND;  // Not found
}

void dict_chinese_free(dict_chinese_t* dict) {
    if (!dict) return;
    
    for (size_t i = 0; i < dict->size; i++) {
        dict_entry_t* entry = dict->table[i];
        while (entry) {
            dict_entry_t* next = entry->next;
            free(entry->character);
            free(entry->pinyin);
            free(entry);
            entry = next;
        }
    }
    
    free(dict->table);
    free(dict);
}
