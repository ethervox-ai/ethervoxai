/**
 * @file event_stream.c
 * @brief Event stream implementation with UTF-8 validation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/event_stream.h"

#include <stdlib.h>
#include <string.h>

// Internal logging (can be disabled for release builds)
#ifdef ETHERVOX_EVENT_STREAM_DEBUG
#include "ethervox/logging.h"
#define EVENT_LOG(...) ETHERVOX_LOG_INFO(__VA_ARGS__)
#else
#define EVENT_LOG(...) ((void)0)
#endif

// ============================================================================
// UTF-8 Validation (ported from iOS bridge)
// ============================================================================

bool ethervox_validate_utf8(const char* str, size_t* out_valid_len) {
    if (!str) {
        if (out_valid_len) {
            *out_valid_len = 0;
        }
        return true;  // NULL is trivially valid
    }
    
    if (!out_valid_len) {
        return false;  // Need output parameter
    }
    
    size_t len = strlen(str);
    *out_valid_len = 0;
    bool is_valid = true;
    
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)str[i];
        int expected_bytes = 0;
        
        // Determine expected byte count from first byte
        if (c <= 0x7F) {
            expected_bytes = 1;  // ASCII (0xxxxxxx)
        } else if ((c & 0xE0) == 0xC0) {
            expected_bytes = 2;  // 2-byte sequence (110xxxxx)
        } else if ((c & 0xF0) == 0xE0) {
            expected_bytes = 3;  // 3-byte sequence (1110xxxx)
        } else if ((c & 0xF8) == 0xF0) {
            expected_bytes = 4;  // 4-byte sequence (11110xxx) - emojis!
        } else {
            // Invalid start byte (10xxxxxx or 11111xxx)
            EVENT_LOG("Invalid UTF-8 start byte 0x%02X at position %zu", c, i);
            is_valid = false;
            break;
        }
        
        // Check if we have all bytes for this character
        if (i + expected_bytes > len) {
            EVENT_LOG("Incomplete UTF-8 sequence at end: need %d bytes, have %zu",
                     expected_bytes, len - i);
            is_valid = false;
            break;
        }
        
        // Verify continuation bytes (must be 10xxxxxx)
        for (int j = 1; j < expected_bytes; j++) {
            unsigned char cont = (unsigned char)str[i + j];
            if ((cont & 0xC0) != 0x80) {
                EVENT_LOG("Invalid continuation byte 0x%02X at position %zu",
                         cont, i + j);
                is_valid = false;
                break;
            }
        }
        
        if (!is_valid) {
            break;
        }
        
        i += expected_bytes;
        *out_valid_len = i;
    }
    
    return is_valid;
}

char* ethervox_create_safe_utf8(const char* str, size_t* out_len) {
    if (!str) {
        if (out_len) {
            *out_len = 0;
        }
        return NULL;
    }
    
    size_t valid_len = 0;
    bool is_valid = ethervox_validate_utf8(str, &valid_len);
    
    if (out_len) {
        *out_len = valid_len;
    }
    
    if (is_valid) {
        // Entire string is valid - return a copy
        char* result = (char*)malloc(valid_len + 1);
        if (!result) {
            return NULL;
        }
        memcpy(result, str, valid_len);
        result[valid_len] = '\0';
        return result;
    } else {
        if (valid_len == 0) {
            EVENT_LOG("Completely invalid UTF-8 string");
            return NULL;
        }
        
        // Partial string is valid - return truncated copy
        EVENT_LOG("Truncated UTF-8 string from %zu to %zu bytes",
                 strlen(str), valid_len);
        
        char* result = (char*)malloc(valid_len + 1);
        if (!result) {
            return NULL;
        }
        memcpy(result, str, valid_len);
        result[valid_len] = '\0';
        return result;
    }
}
