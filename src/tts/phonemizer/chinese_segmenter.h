/**
 * @file chinese_segmenter.h
 * @brief Chinese text segmentation (word boundary detection)
 * 
 * Uses maximum matching algorithm with dictionary.
 */

#ifndef ETHERVOX_CHINESE_SEGMENTER_H
#define ETHERVOX_CHINESE_SEGMENTER_H

#include <stddef.h>
#include "dict_chinese.h"

/**
 * Segment Chinese text into words using maximum matching
 * 
 * @param dict Dictionary for word validation
 * @param text Input Chinese text (UTF-8)
 * @param words Output array of word strings (caller must free)
 * @param max_words Maximum number of words
 * @return Number of words segmented, or -1 on error
 * 
 * Algorithm: Forward maximum matching
 * - Try longest possible match first (up to 8 characters)
 * - Fall back to single character if no match found
 */
int segment_chinese_text(dict_chinese_t* dict,
                        const char* text,
                        char** words,
                        size_t max_words);

#endif // ETHERVOX_CHINESE_SEGMENTER_H
