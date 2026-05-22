/**
 * @file text_normalizer.c
 * @brief Text normalization for TTS (numbers, times, abbreviations)
 *
 * Converts numbers, times, and symbols into speakable text before phonemization.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Number-to-word conversion
static const char* ones[] = {"", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
static const char* teens[] = {"ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
static const char* tens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};

/**
 * Convert 0-99 to words
 */
static void number_to_words_0_99(int num, char* output, size_t output_size) {
    if (num == 0) {
        snprintf(output, output_size, "zero");
    } else if (num < 10) {
        snprintf(output, output_size, "%s", ones[num]);
    } else if (num < 20) {
        snprintf(output, output_size, "%s", teens[num - 10]);
    } else {
        int ten = num / 10;
        int one = num % 10;
        if (one == 0) {
            snprintf(output, output_size, "%s", tens[ten]);
        } else {
            snprintf(output, output_size, "%s-%s", tens[ten], ones[one]);
        }
    }
}

/**
 * Normalize time format (HH:MM) to speakable text
 * Examples:
 *   07:46 → "seven forty-six"
 *   12:00 → "twelve o'clock"
 *   03:05 → "three oh five"
 */
static bool normalize_time(const char* input, char* output, size_t output_size, size_t* chars_consumed) {
    // Check for pattern: digit(s):digit(s)
    const char* colon = strchr(input, ':');
    if (!colon || colon - input > 2 || colon - input < 1) {
        return false;
    }
    
    // Parse hour
    int hour = 0;
    for (const char* p = input; p < colon && isdigit(*p); p++) {
        hour = hour * 10 + (*p - '0');
    }
    
    // Validate hour
    if (hour < 0 || hour > 23) {
        return false;
    }
    
    // Parse minute
    const char* minute_start = colon + 1;
    if (!isdigit(minute_start[0]) || !isdigit(minute_start[1])) {
        return false;
    }
    
    int minute = (minute_start[0] - '0') * 10 + (minute_start[1] - '0');
    if (minute < 0 || minute > 59) {
        return false;
    }
    
    // Convert hour to words
    char hour_words[64];
    if (hour == 0) {
        strcpy(hour_words, "midnight");
    } else if (hour <= 12) {
        number_to_words_0_99(hour, hour_words, sizeof(hour_words));
    } else {
        number_to_words_0_99(hour - 12, hour_words, sizeof(hour_words));
    }
    
    // Convert minute to words
    char minute_words[64];
    if (minute == 0) {
        snprintf(output, output_size, "%s o'clock", hour_words);
    } else if (minute < 10) {
        snprintf(output, output_size, "%s oh %s", hour_words, ones[minute]);
    } else {
        number_to_words_0_99(minute, minute_words, sizeof(minute_words));
        snprintf(output, output_size, "%s %s", hour_words, minute_words);
    }
    
    *chars_consumed = (minute_start + 2) - input;
    return true;
}

/**
 * Normalize standalone numbers to words
 * Examples:
 *   5 → "five"
 *   23 → "twenty-three"
 *   100 → "one hundred"
 */
static bool normalize_number(const char* input, char* output, size_t output_size, size_t* chars_consumed) {
    if (!isdigit(*input)) {
        return false;
    }
    
    // Parse the number
    int num = 0;
    const char* p = input;
    while (isdigit(*p)) {
        num = num * 10 + (*p - '0');
        p++;
    }
    
    *chars_consumed = p - input;
    
    // Convert to words (simple implementation for 0-999)
    if (num == 0) {
        snprintf(output, output_size, "zero");
    } else if (num < 100) {
        number_to_words_0_99(num, output, output_size);
    } else if (num < 1000) {
        int hundreds = num / 100;
        int remainder = num % 100;
        
        if (remainder == 0) {
            snprintf(output, output_size, "%s hundred", ones[hundreds]);
        } else {
            char remainder_words[64];
            number_to_words_0_99(remainder, remainder_words, sizeof(remainder_words));
            snprintf(output, output_size, "%s hundred %s", ones[hundreds], remainder_words);
        }
    } else {
        // For numbers >= 1000, just spell out digits
        char* out_ptr = output;
        size_t remaining = output_size;
        for (const char* digit = input; digit < p && remaining > 10; digit++) {
            int len = snprintf(out_ptr, remaining, "%s ", ones[*digit - '0']);
            out_ptr += len;
            remaining -= len;
        }
        // Remove trailing space
        if (out_ptr > output && *(out_ptr - 1) == ' ') {
            *(out_ptr - 1) = '\0';
        }
    }
    
    return true;
}

/**
 * Main text normalization function
 * Converts numbers, times, and symbols to speakable text
 *
 * @param input Input text with numbers/times
 * @param output Buffer for normalized text
 * @param output_size Size of output buffer
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_tts_normalize_text(const char* input, char* output, size_t output_size) {
    ETHERVOX_CHECK_PTR(input);
    ETHERVOX_CHECK_PTR(output);
    
    if (output_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const char* in_ptr = input;
    char* out_ptr = output;
    size_t remaining = output_size - 1; // Reserve space for null terminator
    
    while (*in_ptr && remaining > 0) {
        char temp[256];
        size_t consumed = 0;
        
        // Try time normalization (HH:MM)
        if (isdigit(*in_ptr) && normalize_time(in_ptr, temp, sizeof(temp), &consumed)) {
            size_t len = strlen(temp);
            if (len < remaining) {
                memcpy(out_ptr, temp, len);
                out_ptr += len;
                remaining -= len;
                in_ptr += consumed;
                continue;
            }
        }
        
        // Try number normalization
        if (isdigit(*in_ptr) && normalize_number(in_ptr, temp, sizeof(temp), &consumed)) {
            size_t len = strlen(temp);
            if (len < remaining) {
                memcpy(out_ptr, temp, len);
                out_ptr += len;
                remaining -= len;
                in_ptr += consumed;
                continue;
            }
        }
        
        // Handle special characters
        if (*in_ptr == '(' || *in_ptr == ')' || *in_ptr == '[' || *in_ptr == ']') {
            // Skip parentheses/brackets (already handled in context)
            in_ptr++;
            continue;
        }
        
        // Copy regular character
        *out_ptr++ = *in_ptr++;
        remaining--;
    }
    
    *out_ptr = '\0';
    return ETHERVOX_SUCCESS;
}
