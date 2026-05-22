/**
 * @file chinese_segmenter.c
 * @brief Forward maximum matching segmentation
 */

#include "chinese_segmenter.h"
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>

#define MAX_WORD_LEN_CHARS 8  // Max Chinese word length in characters
#define MAX_WORD_LEN_BYTES 32 // Max bytes (8 chars * 4 bytes per UTF-8 char)

/**
 * Get length of UTF-8 character at position
 */
static int utf8_char_len(const char* s) {
    unsigned char c = (unsigned char)s[0];
    if ((c & 0x80) == 0) return 1;      // ASCII
    if ((c & 0xE0) == 0xC0) return 2;   // 2-byte
    if ((c & 0xF0) == 0xE0) return 3;   // 3-byte (most Chinese)
    if ((c & 0xF8) == 0xF0) return 4;   // 4-byte
    return 1; // Invalid, treat as single byte
}

/**
 * Count UTF-8 characters in string
 */
static size_t utf8_strlen(const char* s) {
    size_t count = 0;
    while (*s) {
        s += utf8_char_len(s);
        count++;
    }
    return count;
}

/**
 * Extract N UTF-8 characters into buffer
 */
static int extract_utf8_chars(const char* src, size_t n_chars, char* dest, size_t dest_size) {
    size_t bytes = 0;
    size_t chars = 0;
    
    while (chars < n_chars && src[bytes] && bytes < dest_size - 1) {
        int char_len = utf8_char_len(src + bytes);
        if (bytes + char_len >= dest_size) break;
        
        for (int i = 0; i < char_len; i++) {
            dest[bytes] = src[bytes];
            bytes++;
        }
        chars++;
    }
    
    dest[bytes] = '\0';
    return bytes;
}

ethervox_result_t segment_chinese_text(dict_chinese_t* dict,
                        const char* text,
                        char** words,
                        size_t max_words) {
    if (!dict || !text || !words) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    size_t word_count = 0;
    const char* p = text;
    char candidate[MAX_WORD_LEN_BYTES];
    char dummy_pinyin[512];
    
    while (*p && word_count < max_words) {
        // Skip whitespace and ASCII punctuation
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
            *p == ',' || *p == '.' || *p == '!' || *p == '?' ||
            *p == ';' || *p == ':') {
            p++;
            continue;
        }
        
        // Try maximum matching (8 chars down to 1)
        int matched_bytes = 0;
        
        for (int try_len = MAX_WORD_LEN_CHARS; try_len >= 1; try_len--) {
            int bytes = extract_utf8_chars(p, try_len, candidate, MAX_WORD_LEN_BYTES);
            if (bytes == 0) break;
            
            // Check if this is in dictionary
            if (dict_chinese_lookup(dict, candidate, dummy_pinyin, sizeof(dummy_pinyin)) == 0) {
                // Found match
                words[word_count] = malloc(bytes + 1);
                if (words[word_count]) {
                    strcpy(words[word_count], candidate);
                    word_count++;
                }
                matched_bytes = bytes;
                break;
            }
        }
        
        if (matched_bytes == 0) {
            // No match found, take single character
            int char_len = utf8_char_len(p);
            words[word_count] = malloc(char_len + 1);
            if (words[word_count]) {
                strncpy(words[word_count], p, char_len);
                words[word_count][char_len] = '\0';
                word_count++;
            }
            matched_bytes = char_len;
        }
        
        p += matched_bytes;
    }
    
    return word_count;
}
