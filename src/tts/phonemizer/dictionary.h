/**
 * @file dictionary.h
 * @brief Pronunciation dictionary lookup (CMU Dict)
 */

#ifndef ETHERVOX_DICTIONARY_H
#define ETHERVOX_DICTIONARY_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * Opaque dictionary handle
 */
typedef struct pronunciation_dict dict_t;

/**
 * Load dictionary from file
 * @param path Path to CMU Dict file
 * @return Dictionary handle or NULL on error
 */
dict_t* dict_load(const char* path);

/**
 * Lookup word pronunciation (ARPAbet)
 * @param dict Dictionary handle
 * @param word Word to lookup (case-insensitive)
 * @param arpabet_out Output buffer for ARPAbet (space-separated)
 * @param max_len Maximum output length
 * @return ETHERVOX_SUCCESS on success, ETHERVOX_ERROR_NOT_FOUND if not found
 * 
 * Example: "HELLO" → "HH AH0 L OW1"
 */
ethervox_result_t dict_lookup(dict_t* dict, const char* word, char* arpabet_out, size_t max_len);

/**
 * Free dictionary resources
 */
void dict_free(dict_t* dict);

#endif // ETHERVOX_DICTIONARY_H
