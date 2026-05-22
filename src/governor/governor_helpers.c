/**
 * @file governor_helpers.c
 * @brief Implementation of clean, testable helper functions
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "governor_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Default buffer capacity
#define DEFAULT_STRING_BUFFER_CAPACITY 4096

// ============================================================================
// Buffer Management Implementation
// ============================================================================

bool string_buffer_init(string_buffer_t* buffer, size_t initial_capacity) {
    if (!buffer) return false;
    
    if (initial_capacity == 0) {
        initial_capacity = DEFAULT_STRING_BUFFER_CAPACITY;
    }
    
    buffer->data = (char*)calloc(initial_capacity, 1);
    if (!buffer->data) {
        return false;
    }
    
    buffer->length = 0;
    buffer->capacity = initial_capacity;
    buffer->owns_memory = true;
    
    return true;
}

bool string_buffer_append(string_buffer_t* buffer, const char* str) {
    if (!buffer || !str) return false;
    
    size_t str_len = strlen(str);
    if (str_len == 0) return true;
    
    // Check if we need to grow
    if (buffer->length + str_len + 1 > buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        while (new_capacity < buffer->length + str_len + 1) {
            new_capacity *= 2;
        }
        
        char* new_data = (char*)realloc(buffer->data, new_capacity);
        if (!new_data) {
            return false;
        }
        
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    
    // Append
    memcpy(buffer->data + buffer->length, str, str_len);
    buffer->length += str_len;
    buffer->data[buffer->length] = '\0';
    
    return true;
}

bool string_buffer_append_char(string_buffer_t* buffer, char c) {
    char str[2] = {c, '\0'};
    return string_buffer_append(buffer, str);
}

void string_buffer_clear(string_buffer_t* buffer) {
    if (buffer && buffer->data) {
        buffer->data[0] = '\0';
        buffer->length = 0;
    }
}

void string_buffer_free(string_buffer_t* buffer) {
    if (buffer && buffer->owns_memory) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->length = 0;
        buffer->capacity = 0;
    }
}

char* string_buffer_transfer(string_buffer_t* buffer) {
    if (!buffer || !buffer->data) return NULL;
    
    char* result = buffer->data;
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
    buffer->owns_memory = false;
    
    return result;
}

// ============================================================================
// Token Processing Implementation
// ============================================================================

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
) {
    if (!ctx || !buffer || buffer_size < 2) {
        return -1;
    }
    
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
    struct llama_model* model = llama_get_model(ctx);
    if (!model) {
        return -1;
    }
    
    const struct llama_vocab* vocab = llama_model_get_vocab(model);
    if (!vocab) {
        return -1;
    }
    
    int n = llama_token_to_piece(vocab, token, buffer, buffer_size - 1, 0, false);
    
    if (n < 0 || n >= (int)buffer_size) {
        return -1;
    }
    
    buffer[n] = '\0';
    return n;
#else
    buffer[0] = '\0';
    return 0;
#endif
}

bool clean_token_text(const char* token_text, char* output, size_t output_size) {
    if (!token_text || !output || output_size < 2) {
        return false;
    }
    
    // List of special markers to filter out
    const char* markers[] = {
        "<|im_start|>",
        "<|im_end|>",
        "<|endoftext|>",
        "<|assistant|>",
        "<|user|>",
        "<|system|>",
        "</s>",
        NULL
    };
    
    // Check if token is a special marker
    for (int i = 0; markers[i] != NULL; i++) {
        if (strcmp(token_text, markers[i]) == 0) {
            output[0] = '\0';  // Empty string
            return true;
        }
    }
    
    // Not a special marker - copy as-is
    strncpy(output, token_text, output_size - 1);
    output[output_size - 1] = '\0';
    
    return true;
}

// ============================================================================
// KV Cache Management Implementation
// ============================================================================

kv_cache_state_t kv_cache_get_state(
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
    struct llama_context* ctx,
#else
    void* ctx,
#endif
    int current_pos,
    int system_prompt_end
) {
    kv_cache_state_t state = {0};
    
    if (!ctx) {
        state.is_valid = false;
        return state;
    }
    
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
    state.current_pos = current_pos;
    state.capacity = llama_n_ctx(ctx);
    state.system_prompt_end = system_prompt_end;
    state.is_valid = true;
#else
    state.is_valid = false;
#endif
    
    return state;
}

bool kv_cache_has_room(const kv_cache_state_t* state, int n_tokens) {
    if (!state || !state->is_valid) {
        return false;
    }
    
    return (state->current_pos + n_tokens) <= state->capacity;
}

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
) {
    if (!ctx || !tokens || n_tokens <= 0) {
        return KV_DECODE_ERROR_INVALID;
    }
    
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE)
    // Check capacity
    int n_ctx = llama_n_ctx(ctx);
    if (start_pos + n_tokens > n_ctx) {
        return KV_DECODE_ERROR_FULL;
    }
    
    // Create batch
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    batch.n_tokens = n_tokens;
    
    for (int i = 0; i < n_tokens; i++) {
        batch.token[i] = tokens[i];
        batch.pos[i] = start_pos + i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = false;
    }
    
    // Compute logits for last token if requested
    if (compute_logits && n_tokens > 0) {
        batch.logits[n_tokens - 1] = true;
    }
    
    // Decode
    int result = llama_decode(ctx, batch);
    llama_batch_free(batch);
    
    return (result == 0) ? KV_DECODE_SUCCESS : KV_DECODE_ERROR_LLAMA;
#else
    return KV_DECODE_ERROR_INVALID;
#endif
}

// ============================================================================
// Sequence Detection Implementation
// ============================================================================

sequence_match_t check_stop_sequence(const char* buffer, const char** stop_sequences) {
    if (!buffer || !stop_sequences) {
        return SEQ_NO_MATCH;
    }
    
    size_t buf_len = strlen(buffer);
    if (buf_len == 0) {
        return SEQ_NO_MATCH;
    }
    
    // Check each stop sequence
    for (int i = 0; stop_sequences[i] != NULL; i++) {
        const char* stop_seq = stop_sequences[i];
        size_t seq_len = strlen(stop_seq);
        
        // Complete match?
        if (strcmp(buffer, stop_seq) == 0) {
            return SEQ_COMPLETE_MATCH;
        }
        
        // Partial match (buffer is prefix of sequence)?
        if (buf_len < seq_len && strncmp(buffer, stop_seq, buf_len) == 0) {
            return SEQ_PARTIAL_MATCH;
        }
    }
    
    return SEQ_NO_MATCH;
}

sequence_match_t check_tool_call(const char* buffer, bool is_json_format) {
    if (!buffer) {
        return SEQ_NO_MATCH;
    }
    
    // Check for tool call start
    if (strstr(buffer, "<tool") != buffer) {
        return SEQ_NO_MATCH;
    }
    
    // Check for complete tool call
    if (is_json_format) {
        // JSON format: <tool_call>\n{...}\n</tool_call>
        if (strstr(buffer, "</tool_call>")) {
            return SEQ_COMPLETE_MATCH;
        }
    } else {
        // XML attribute format: <tool_call name="..." />
        if (strstr(buffer, "/>")) {
            return SEQ_COMPLETE_MATCH;
        }
    }
    
    // Partial match (might still be accumulating)
    return SEQ_PARTIAL_MATCH;
}

bool extract_tool_name(const char* tool_call, char* name_out, size_t name_size) {
    if (!tool_call || !name_out || name_size < 2) {
        return false;
    }
    
    // Look for name="..." or "name":"..."
    const char* name_start = strstr(tool_call, "name");
    if (!name_start) {
        return false;
    }
    
    // Skip to quote
    const char* quote_start = strchr(name_start, '"');
    if (!quote_start) {
        return false;
    }
    quote_start++; // Skip opening quote
    
    const char* quote_end = strchr(quote_start, '"');
    if (!quote_end) {
        return false;
    }
    
    // Extract name
    size_t name_len = quote_end - quote_start;
    if (name_len >= name_size) {
        name_len = name_size - 1;
    }
    
    strncpy(name_out, quote_start, name_len);
    name_out[name_len] = '\0';
    
    return true;
}

// ============================================================================
// Prompt Building Implementation
// ============================================================================

// Forward declare to avoid circular dependency
typedef struct {
    const char* user_start;
    const char* user_end;
    const char* assistant_start;
    const char* assistant_end;
    const char* system_start;
    const char* system_end;
} chat_template_fields_t;

bool prompt_builder_init(prompt_builder_t* builder, const void* template_ptr) {
    if (!builder || !template_ptr) {
        return false;
    }
    
    const chat_template_fields_t* template = (const chat_template_fields_t*)template_ptr;
    
    if (!string_buffer_init(&builder->buffer, 8192)) {
        return false;
    }
    
    builder->user_start = template->user_start;
    builder->user_end = template->user_end;
    builder->assistant_start = template->assistant_start;
    builder->assistant_end = template->assistant_end;
    builder->system_start = template->system_start;
    builder->system_end = template->system_end;
    
    return true;
}

bool prompt_builder_add_user_message(prompt_builder_t* builder, const char* message) {
    if (!builder || !message) return false;
    
    return string_buffer_append(&builder->buffer, builder->user_start) &&
           string_buffer_append(&builder->buffer, message) &&
           string_buffer_append(&builder->buffer, builder->user_end);
}

bool prompt_builder_add_assistant_message(prompt_builder_t* builder, const char* message) {
    if (!builder || !message) return false;
    
    return string_buffer_append(&builder->buffer, builder->assistant_start) &&
           string_buffer_append(&builder->buffer, message) &&
           string_buffer_append(&builder->buffer, builder->assistant_end);
}

bool prompt_builder_add_system_message(prompt_builder_t* builder, const char* message) {
    if (!builder || !message) return false;
    
    return string_buffer_append(&builder->buffer, builder->system_start) &&
           string_buffer_append(&builder->buffer, message) &&
           string_buffer_append(&builder->buffer, builder->system_end);
}

bool prompt_builder_add_tool_result(prompt_builder_t* builder, const char* tool_result) {
    if (!builder || !tool_result) return false;
    
    return string_buffer_append(&builder->buffer, "<tool_result>") &&
           string_buffer_append(&builder->buffer, tool_result) &&
           string_buffer_append(&builder->buffer, "</tool_result>");
}

char* prompt_builder_finalize(prompt_builder_t* builder) {
    if (!builder) return NULL;
    return string_buffer_transfer(&builder->buffer);
}

void prompt_builder_free(prompt_builder_t* builder) {
    if (builder) {
        string_buffer_free(&builder->buffer);
    }
}

// ============================================================================
// Error Handling Implementation
// ============================================================================

void error_context_set(
    error_context_t* ctx,
    int code,
    const char* message,
    const char* function,
    int line,
    bool is_fatal
) {
    if (!ctx) return;
    
    ctx->code = code;
    ctx->is_fatal = is_fatal;
    
    snprintf(ctx->message, sizeof(ctx->message), "[%s:%d] %s",
             function ? function : "unknown",
             line,
             message ? message : "Unknown error");
}

void error_context_clear(error_context_t* ctx) {
    if (ctx) {
        ctx->code = 0;
        ctx->message[0] = '\0';
        ctx->function = NULL;
        ctx->line = 0;
        ctx->is_fatal = false;
    }
}

bool error_context_has_error(const error_context_t* ctx) {
    return ctx && ctx->code != 0;
}

const char* error_context_get_message(const error_context_t* ctx) {
    return ctx ? ctx->message : "No error context";
}

// ============================================================================
// Statistics Implementation
// ============================================================================

void stats_init(generation_stats_t* stats) {
    if (stats) {
        memset(stats, 0, sizeof(generation_stats_t));
    }
}

void stats_record_first_token(generation_stats_t* stats, double time) {
    if (stats) {
        stats->first_token_time = time;
    }
}

void stats_record_token(generation_stats_t* stats) {
    if (stats) {
        stats->tokens_generated++;
        stats->tokens_streamed++;
    }
}

void stats_record_tool_execution(generation_stats_t* stats, double duration) {
    if (stats) {
        stats->tool_calls_executed++;
        stats->tool_execution_time += duration;
    }
}

double stats_get_tokens_per_second(const generation_stats_t* stats) {
    if (!stats || stats->total_generation_time <= 0.0) {
        return 0.0;
    }
    return stats->tokens_generated / stats->total_generation_time;
}

void stats_print(const generation_stats_t* stats) {
    if (!stats) return;
    
    printf("=== Generation Statistics ===\n");
    printf("Tokens generated:    %d\n", stats->tokens_generated);
    printf("Tokens streamed:     %d\n", stats->tokens_streamed);
    printf("Tool calls executed: %d\n", stats->tool_calls_executed);
    printf("First token time:    %.3f ms\n", stats->first_token_time * 1000.0);
    printf("Total generation:    %.3f s\n", stats->total_generation_time);
    printf("Tool execution time: %.3f s\n", stats->tool_execution_time);
    printf("Tokens per second:   %.1f\n", stats_get_tokens_per_second(stats));
    printf("KV cache usage:      %d%%\n", stats->kv_cache_usage);
    printf("============================\n");
}

// ============================================================================
// Memory Management Implementation
// ============================================================================

void* safe_malloc(size_t size, error_context_t* error_ctx) {
    void* ptr = malloc(size);
    if (!ptr && error_ctx) {
        SET_ERROR(error_ctx, -1, "Memory allocation failed", true);
    }
    return ptr;
}

void* safe_calloc(size_t count, size_t size, error_context_t* error_ctx) {
    void* ptr = calloc(count, size);
    if (!ptr && error_ctx) {
        SET_ERROR(error_ctx, -1, "Memory allocation failed", true);
    }
    return ptr;
}

void* safe_realloc(void* ptr, size_t new_size, error_context_t* error_ctx) {
    void* new_ptr = realloc(ptr, new_size);
    if (!new_ptr && error_ctx) {
        SET_ERROR(error_ctx, -1, "Memory reallocation failed", true);
    }
    return new_ptr;
}

void safe_free(void** ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

char* safe_strdup(const char* str, error_context_t* error_ctx) {
    if (!str) return NULL;
    
    char* dup = strdup(str);
    if (!dup && error_ctx) {
        SET_ERROR(error_ctx, -1, "String duplication failed", true);
    }
    return dup;
}
