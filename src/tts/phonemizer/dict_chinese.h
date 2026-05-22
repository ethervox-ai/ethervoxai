/**
 * @file dict_chinese.h
 * @brief Unicode Unihan dictionary lookup for Chinese
 */

#ifndef ETHERVOX_DICT_CHINESE_H
#define ETHERVOX_DICT_CHINESE_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * Opaque Chinese dictionary handle
 */
typedef struct chinese_dict dict_chinese_t;

/**
 * Load Unihan dictionary
 * @param path Path to Unihan_Readings.txt file
 * @return Dictionary handle or NULL on error
 */
dict_chinese_t* dict_chinese_load(const char* path);

/**
 * Lookup Chinese character pronunciation (pinyin with tone marks)
 * @param dict Dictionary handle
 * @param character Single Chinese character (UTF-8)
 * @param pinyin_out Output buffer for pinyin
 * @param max_len Maximum output length
 * @return ETHERVOX_SUCCESS on success, ETHERVOX_ERROR_NOT_FOUND if not found
 * 
 * Example: "你" → "nǐ" (with Unicode tone marks)
 */
ethervox_result_t dict_chinese_lookup(dict_chinese_t* dict, const char* character, char* pinyin_out, size_t max_len);

/**
 * Free dictionary resources
 */
void dict_chinese_free(dict_chinese_t* dict);

#endif // ETHERVOX_DICT_CHINESE_H
