// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
#ifndef ETHERVOX_GRAMMAR_H
#define ETHERVOX_GRAMMAR_H

#include "ethervox/error.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file grammar.h
 * @brief Grammar-constrained decoding using GBNF (GGML Backus-Naur Form)
 * 
 * Provides JSON Schema → GBNF conversion for constraining LLM output to
 * valid structured formats (tool call arguments, JSON objects, etc.).
 * 
 * Design constraints:
 * - First-party implementation: no llama.cpp common/ or nlohmann/json
 * - Uses only bundled cJSON for JSON parsing
 * - Supports a closed subset of JSON Schema (tool-call schemas only)
 * - Grammar objects store GBNF source; samplers are constructed per-model
 * 
 * @see C2.3a task packet for implementation details
 */

/**
 * @brief Opaque grammar handle
 * 
 * Stores GBNF grammar source and metadata. Does NOT store a llama_sampler*
 * because grammars are vocab-independent and can be used across multiple models.
 */
typedef struct ethervox_grammar ethervox_grammar_t;

/**
 * @brief Create a grammar from GBNF source text
 * 
 * Compiles a GBNF grammar from a string. The grammar can then be used with
 * generation via ethervox_generate() or similar.
 * 
 * @param gbnf_source GBNF grammar source (will be copied)
 * @param root_rule Name of the root rule (usually "root")
 * @param out_grammar Output pointer for created grammar
 * @return ETHERVOX_SUCCESS or error code
 * 
 * @note Caller must free with ethervox_grammar_free()
 * @note Grammar compilation errors return ETHERVOX_ERROR_INVALID_ARGUMENT
 */
ethervox_result_t ethervox_grammar_compile(
    const char* gbnf_source,
    const char* root_rule,
    ethervox_grammar_t** out_grammar
);

/**
 * @brief Create a grammar from a JSON Schema
 * 
 * Converts a JSON Schema (subset: types boolean/integer/number/string/object/array,
 * constraints: enum/maxLength/pattern/maxItems/required/oneOf) to GBNF.
 * 
 * Supported schema features:
 * - Scalar types: boolean, integer, number, string
 * - String constraints: enum, maxLength, pattern (basic character classes only)
 * - Objects: properties, required, additionalProperties: false
 * - Arrays: items, maxItems
 * - Unions: oneOf (for tool-call variant selection)
 * 
 * @param json_schema JSON Schema as a JSON string
 * @param out_grammar Output pointer for created grammar
 * @return ETHERVOX_SUCCESS or error code
 * 
 * @note Caller must free with ethervox_grammar_free()
 * @note Malformed JSON or unsupported schema → ETHERVOX_ERROR_INVALID_ARGUMENT
 * 
 * @example
 * const char* schema = "{\"type\": \"object\", \"properties\": "
 *                      "{\"name\": {\"type\": \"string\"}}, \"required\": [\"name\"]}";
 * ethervox_grammar_t* grammar = NULL;
 * if (ethervox_grammar_from_json_schema(schema, &grammar) == ETHERVOX_SUCCESS) {
 *     // Use grammar...
 *     ethervox_grammar_free(grammar);
 * }
 */
ethervox_result_t ethervox_grammar_from_json_schema(
    const char* json_schema,
    ethervox_grammar_t** out_grammar
);

/**
 * @brief Free a grammar object
 * 
 * @param grammar Grammar to free (NULL-safe)
 */
void ethervox_grammar_free(ethervox_grammar_t* grammar);

/**
 * @brief Get the GBNF source of a compiled grammar
 * 
 * @param grammar Grammar object
 * @return GBNF source string (owned by grammar, do not free)
 * @return NULL if grammar is NULL
 */
const char* ethervox_grammar_get_source(const ethervox_grammar_t* grammar);

/**
 * @brief Get the root rule name of a grammar
 * 
 * @param grammar Grammar object
 * @return Root rule name (owned by grammar, do not free)
 * @return NULL if grammar is NULL
 */
const char* ethervox_grammar_get_root(const ethervox_grammar_t* grammar);

/**
 * @brief Check if grammar is in lazy mode
 * 
 * @param grammar Grammar object
 * @return true if lazy mode enabled, false otherwise
 */
bool ethervox_grammar_is_lazy(const ethervox_grammar_t* grammar);

/**
 * @brief Get trigger words for lazy grammar
 * 
 * @param grammar Grammar object
 * @param out_count Output: number of trigger words (can be NULL)
 * @return Pointer to trigger words array (owned by grammar, do not free)
 * @return NULL if grammar is NULL or not in lazy mode
 */
const char* const* ethervox_grammar_get_trigger_words(
    const ethervox_grammar_t* grammar,
    size_t* out_count
);

/**
 * @brief Enable lazy grammar mode with trigger words
 * 
 * Lazy grammars allow the model to produce free text (explanation, thinking)
 * followed by structured output. The grammar only engages after one of the
 * trigger words/patterns is detected in the output.
 * 
 * @param grammar Grammar object
 * @param trigger_words NULL-terminated array of trigger strings (will be copied)
 * @param trigger_word_count Number of trigger words in the array
 * @return ETHERVOX_SUCCESS or error code
 * 
 * @example
 * const char* triggers[] = {"```json"};
 * ethervox_grammar_set_lazy_mode(grammar, triggers, 1);
 * 
 * @note Pass NULL and 0 to disable lazy mode
 * @note Trigger words are copied; caller retains ownership of input array
 */
ethervox_result_t ethervox_grammar_set_lazy_mode(
    ethervox_grammar_t* grammar,
    const char** trigger_words,
    size_t trigger_word_count
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_GRAMMAR_H
