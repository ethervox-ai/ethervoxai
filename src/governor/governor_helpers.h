/**
 * @file governor_helpers.h
 * @brief Clean, testable helper functions for streaming governor
 *
 * This header provides reusable utilities that can be tested independently:
 * - Buffer management
 * - Token processing
 * - KV cache operations
 * - Sequence detection
 * - Error handling
 *
 * Design principles:
 * - Small, focused functions (single responsibility)
 * - Pure functions where possible (no side effects)
 * - Clear error codes
 * - Easy to unit test
 * - Well-documented contracts
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_GOVERNOR_HELPERS_H
#define ETHERVOX_GOVERNOR_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ethervox/governor.h"

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
#include <llama.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Buffer Management - Dynamically growing buffers
// ============================================================================

/**
 * Dynamic string buffer that grows as needed
 */
typedef struct {
    char* data;
    size_t length;      // Current content length
    size_t capacity;    // Allocated capacity
    bool owns_memory;   // Whether we allocated the memory
} string_buffer_t;

/**
 * Initialize a string buffer
 * @param buffer Buffer to initialize
 * @param initial_capacity Initial capacity (0 = use default)
 * @return true on success, false on allocation failure
 */
bool string_buffer_init(string_buffer_t* buffer, size_t initial_capacity);

/**
 * Append string to buffer, growing if necessary
 * @param buffer Buffer to append to
 * @param str String to append
 * @return true on success, false on allocation failure
 */
bool string_buffer_append(string_buffer_t* buffer, const char* str);

/**
 * Append single character to buffer
 */
bool string_buffer_append_char(string_buffer_t* buffer, char c);

/**
 * Clear buffer content (keeps allocated memory)
 */
void string_buffer_clear(string_buffer_t* buffer);

/**
 * Free buffer memory
 */
void string_buffer_free(string_buffer_t* buffer);

/**
 * Transfer ownership of buffer contents
 * @param buffer Buffer to transfer from
 * @return Pointer to contents (caller owns memory), or NULL on error
 */
char* string_buffer_transfer(string_buffer_t* buffer);

// ============================================================================
// Token Processing - Clean token manipulation
// ============================================================================

/**
 * Token metadata
 */
typedef struct {
    const char* text;       // Token text (borrowed pointer)
    size_t length;          // Length in bytes
    int token_id;          // llama token ID
    bool is_special;       // Is this a special token?
} token_info_t;

/**
 * Convert llama token to string safely
 * @param ctx llama context
 * @param token Token to convert
 * @param buffer Output buffer (must be at least 256 bytes)
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or -1 on error
 */
int token_to_string_safe(
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
    struct llama_context* ctx,
    llama_token token,
#else
    void* ctx,
    int token,
#endif
    char* buffer,
    size_t buffer_size
);

/**
 * Clean token text (remove special markers that leaked through)
 * @param token_text Input token text
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return true if cleaning succeeded
 */
bool clean_token_text(const char* token_text, char* output, size_t output_size);

// ============================================================================
// KV Cache Management - Safe KV cache operations
// ============================================================================

/**
 * KV cache state
 */
typedef struct {
    int current_pos;        // Current position in cache
    int capacity;           // Maximum capacity
    int system_prompt_end;  // End of system prompt
    bool is_valid;          // Is the cache in a valid state?
} kv_cache_state_t;

/**
 * Get current KV cache state
 */
kv_cache_state_t kv_cache_get_state(
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
    struct llama_context* ctx,
#else
    void* ctx,
#endif
    int current_pos,
    int system_prompt_end
);

/**
 * Check if we can safely add N tokens to KV cache
 * @param state Current cache state
 * @param n_tokens Number of tokens to add
 * @return true if there's room
 */
bool kv_cache_has_room(const kv_cache_state_t* state, int n_tokens);

/**
 * Result of a KV cache decode operation
 */
typedef enum {
    KV_DECODE_SUCCESS = 0,
    KV_DECODE_ERROR_FULL = -1,
    KV_DECODE_ERROR_INVALID = -2,
    KV_DECODE_ERROR_LLAMA = -3
} kv_decode_result_t;

/**
 * Decode tokens into KV cache safely
 * @param ctx llama context
 * @param tokens Array of tokens
 * @param n_tokens Number of tokens
 * @param start_pos Starting position in KV cache
 * @param compute_logits Whether to compute logits for last token
 * @return KV_DECODE_SUCCESS or error code
 */
kv_decode_result_t kv_cache_decode_tokens(
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
    struct llama_context* ctx,
    const llama_token* tokens,
#else
    void* ctx,
    const int* tokens,
#endif
    int n_tokens,
    int start_pos,
    bool compute_logits
);

// ============================================================================
// Sequence Detection - Detect stop sequences and tool calls
// ============================================================================

/**
 * Sequence match result
 */
typedef enum {
    SEQ_NO_MATCH = 0,           // Definitely not a match
    SEQ_PARTIAL_MATCH = 1,      // Could be a match (need more tokens)
    SEQ_COMPLETE_MATCH = 2      // Complete match
} sequence_match_t;

/**
 * Check if buffer could be (or is) a stop sequence
 * @param buffer Current buffer content
 * @param stop_sequences Array of stop sequences (NULL-terminated)
 * @return Match result
 */
sequence_match_t check_stop_sequence(const char* buffer, const char** stop_sequences);

/**
 * Check if buffer contains a tool call
 * @param buffer Current buffer content
 * @param is_json_format Whether using JSON-in-XML format
 * @return Match result
 */
sequence_match_t check_tool_call(const char* buffer, bool is_json_format);

/**
 * Extract tool call name from tool call string
 * @param tool_call Complete tool call string
 * @param name_out Output buffer for name
 * @param name_size Size of output buffer
 * @return true if name extracted successfully
 */
bool extract_tool_name(const char* tool_call, char* name_out, size_t name_size);

// ============================================================================
// Prompt Building - Testable prompt construction
// ============================================================================

/**
 * Prompt builder for clean prompt construction
 */
typedef struct {
    string_buffer_t buffer;
    const char* user_start;
    const char* user_end;
    const char* assistant_start;
    const char* assistant_end;
    const char* system_start;
    const char* system_end;
} prompt_builder_t;

/**
 * Initialize prompt builder with chat template
 */
bool prompt_builder_init(prompt_builder_t* builder, const void* template);

/**
 * Add user message
 */
bool prompt_builder_add_user_message(prompt_builder_t* builder, const char* message);

/**
 * Add assistant message
 */
bool prompt_builder_add_assistant_message(prompt_builder_t* builder, const char* message);

/**
 * Add system message
 */
bool prompt_builder_add_system_message(prompt_builder_t* builder, const char* message);

/**
 * Add tool result
 */
bool prompt_builder_add_tool_result(prompt_builder_t* builder, const char* tool_result);

/**
 * Get final prompt (transfers ownership)
 */
char* prompt_builder_finalize(prompt_builder_t* builder);

/**
 * Free prompt builder
 */
void prompt_builder_free(prompt_builder_t* builder);

// ============================================================================
// Error Handling - Consistent error reporting
// ============================================================================

/**
 * Error context for detailed error information
 */
typedef struct {
    int code;                   // Error code
    char message[512];          // Error message
    const char* function;       // Function where error occurred
    int line;                   // Line number
    bool is_fatal;              // Whether error is fatal
} error_context_t;

/**
 * Set error context
 */
void error_context_set(
    error_context_t* ctx,
    int code,
    const char* message,
    const char* function,
    int line,
    bool is_fatal
);

/**
 * Clear error context
 */
void error_context_clear(error_context_t* ctx);

/**
 * Check if error is set
 */
bool error_context_has_error(const error_context_t* ctx);

/**
 * Get error message
 */
const char* error_context_get_message(const error_context_t* ctx);

// Macro for setting errors with automatic function/line info
#define SET_ERROR(ctx, code, msg, fatal) \
    error_context_set(ctx, code, msg, __func__, __LINE__, fatal)

// ============================================================================
// Statistics - Performance metrics
// ============================================================================

/**
 * Generation statistics
 */
typedef struct {
    int tokens_generated;
    int tokens_streamed;
    int tool_calls_executed;
    double first_token_time;    // Seconds since start
    double total_generation_time;
    double tool_execution_time;
    int kv_cache_usage;         // Percentage
} generation_stats_t;

/**
 * Initialize statistics
 */
void stats_init(generation_stats_t* stats);

/**
 * Record first token
 */
void stats_record_first_token(generation_stats_t* stats, double time);

/**
 * Record token generated
 */
void stats_record_token(generation_stats_t* stats);

/**
 * Record tool execution
 */
void stats_record_tool_execution(generation_stats_t* stats, double duration);

/**
 * Get tokens per second
 */
double stats_get_tokens_per_second(const generation_stats_t* stats);

/**
 * Print statistics (for debugging)
 */
void stats_print(const generation_stats_t* stats);

// ============================================================================
// Memory Management - Safe allocation/deallocation
// ============================================================================

/**
 * Allocate memory with error checking
 * @param size Size to allocate
 * @param error_ctx Error context to set on failure
 * @return Pointer to allocated memory, or NULL on failure
 */
void* safe_malloc(size_t size, error_context_t* error_ctx);

/**
 * Allocate and zero memory
 */
void* safe_calloc(size_t count, size_t size, error_context_t* error_ctx);

/**
 * Reallocate memory safely
 */
void* safe_realloc(void* ptr, size_t new_size, error_context_t* error_ctx);

/**
 * Free memory safely (NULL-safe)
 */
void safe_free(void** ptr);

/**
 * Duplicate string safely
 */
char* safe_strdup(const char* str, error_context_t* error_ctx);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_GOVERNOR_HELPERS_H
