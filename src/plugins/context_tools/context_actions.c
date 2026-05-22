/**
#include "ethervox/error.h"
 * @file context_actions.c
 * @brief Implementation of context management actions
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/context_tools.h"
#include "ethervox/config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <llama.h>
#define LLAMA_HEADER_AVAILABLE 1
#else
#define LLAMA_HEADER_AVAILABLE 0
#endif

#define CTX_LOG(...) ETHERVOX_LOGI(__VA_ARGS__)
#define CTX_ERROR(...) ETHERVOX_LOGE(__VA_ARGS__)

// Access to internal governor structure fields
// We'll use a simplified casting approach
typedef struct {
    ethervox_governor_config_t config;
    ethervox_tool_registry_t* tool_registry;
    
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
    struct llama_model* llm_model;
    struct llama_context* llm_ctx;
    int32_t system_prompt_token_count;
    int32_t current_kv_pos;
    char* model_path;
    void* tool_result_prefix_tokens;
    int tool_result_prefix_len;
    void* tool_result_suffix_tokens;
    int tool_result_suffix_len;
#else
    void* llm_model;
    void* llm_ctx;
    int32_t system_prompt_token_count;
    int32_t current_kv_pos;
    char* model_path;
#endif
    
    uint32_t last_iteration_count;
    bool initialized;
    bool llm_loaded;
    
    context_manager_state_t context_manager;
    conversation_history_t conversation_history;
    uint32_t turn_counter;
} governor_internal_t;

// ============================================================================
// Action: Shift Window
// ============================================================================

ethervox_result_t context_action_shift_window(
    ethervox_governor_t* governor,
    uint32_t keep_last_n_turns,
    context_action_result_t* result
) {
    if (!governor || !result) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    memset(result, 0, sizeof(context_action_result_t));
    
#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
    snprintf(result->error_msg, sizeof(result->error_msg), 
             "llama.cpp not available - cannot manage context");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
#else
    
    governor_internal_t* gov = (governor_internal_t*)governor;
    conversation_history_t* history = &gov->conversation_history;
    uint32_t total_turns = history->turn_count;
    
    // Validate parameters
    if (keep_last_n_turns >= total_turns) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Cannot shift: only %u turns exist, requested to keep %u",
                 total_turns, keep_last_n_turns);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t turns_to_drop = total_turns - keep_last_n_turns;
    if (turns_to_drop == 0) {
        result->success = true;
        return ETHERVOX_SUCCESS;  // Nothing to do
    }
    
    CTX_LOG("[Context] Shifting window: dropping %u turns, keeping %u",
            turns_to_drop, keep_last_n_turns);
    
    // Find the end position of the last turn to drop
    int32_t drop_end = history->turns[turns_to_drop - 1].kv_end;
    int32_t drop_start = history->turns[0].kv_start;
    int tokens_to_free = drop_end - drop_start;
    
    // Get KV cache memory handle and remove old tokens
    llama_memory_t mem = llama_get_memory(gov->llm_ctx);
    if (!mem) {
        CTX_ERROR("[Context] Failed to get memory handle");
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Failed to get KV cache memory handle");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Remove the specified range from KV cache (sequence 0 is the main conversation)
    // llama_memory_seq_rm(memory, seq_id, p0, p1) removes tokens from position p0 to p1
    llama_memory_seq_rm(mem, 0, drop_start, drop_end);
    CTX_LOG("[Context] Removed KV cache range: %d to %d (%d tokens)",
            drop_start, drop_end, tokens_to_free);
    
    // Note: No need to manually shift - llama.cpp handles compaction automatically
    // The remaining tokens will be accessed with their updated positions
    
    // Update turn tracking - shift remaining turns down
    for (uint32_t i = 0; i < keep_last_n_turns; i++) {
        history->turns[i] = history->turns[i + turns_to_drop];
        // Update KV positions
        history->turns[i].kv_start -= tokens_to_free;
        history->turns[i].kv_end -= tokens_to_free;
    }
    
    // Update counts
    history->turn_count = keep_last_n_turns;
    gov->current_kv_pos -= tokens_to_free;
    
    // Update context manager state
    gov->context_manager.last_gc_position = gov->current_kv_pos;
    
    // Fill result
    result->tokens_freed = tokens_to_free;
    result->turns_removed = turns_to_drop;
    result->summary_memory_id = 0;  // No summary created
    result->success = true;
    
    CTX_LOG("[Context] Shift complete: freed %d tokens from KV cache, current pos now %d",
            tokens_to_free, gov->current_kv_pos);
    
    return ETHERVOX_SUCCESS;
#endif
}

// ============================================================================
// Action: Summarize Old
// ============================================================================

// Forward declaration
static char* generate_summary_simple(
    conversation_history_t* history,
    uint32_t start_turn,
    uint32_t end_turn,
    const char* detail_level
);

/**
 * Generate summary using LLM (intelligent summarization)
 */
static char* generate_summary_llm(
    ethervox_governor_t* governor,
    conversation_history_t* history,
    uint32_t start_turn,
    uint32_t end_turn,
    const char* detail_level
) {
#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
    // Fall back to simple summary if no LLM available
    return generate_summary_simple(history, start_turn, end_turn, detail_level);
#else
    governor_internal_t* gov = (governor_internal_t*)governor;
    
    // Check if LLM is loaded
    if (!gov->llm_ctx || !gov->llm_model) {
        return generate_summary_simple(history, start_turn, end_turn, detail_level);
    }
    
    // Build conversation text from turns
    size_t conv_buffer_size = 4096;
    char* conversation_text = malloc(conv_buffer_size);
    if (!conversation_text) {
        return generate_summary_simple(history, start_turn, end_turn, detail_level);
    }
    
    size_t pos = 0;
    for (uint32_t i = start_turn; i <= end_turn && i < history->turn_count; i++) {
        conversation_turn_t* turn = &history->turns[i];
        const char* speaker = turn->is_user ? "User" : "Assistant";
        
        size_t remaining = conv_buffer_size - pos - 1;
        if (remaining < 200) break;
        
        pos += snprintf(conversation_text + pos, remaining,
                       "%s: %s\n", speaker, turn->preview);
    }
    conversation_text[pos] = '\0';
    
    // Determine summary length based on detail level
    int max_tokens = 50;  // brief
    if (strcmp(detail_level, "moderate") == 0) {
        max_tokens = 100;
    } else if (strcmp(detail_level, "detailed") == 0) {
        max_tokens = 200;
    }
    
    // Build summarization prompt
    char prompt[5120];
    snprintf(prompt, sizeof(prompt),
             "<|im_start|>system\n"
             "You are a conversation summarizer. Create a concise summary of the following conversation. "
             "Focus on key topics, decisions, and outcomes. Keep it under %d tokens.<|im_end|>\n"
             "<|im_start|>user\n"
             "Summarize this conversation:\n\n%s<|im_end|>\n"
             "<|im_start|>assistant\n",
             max_tokens, conversation_text);
    
    free(conversation_text);
    
    // Tokenize prompt
    const struct llama_vocab* vocab = llama_model_get_vocab(gov->llm_model);
    int n_prompt_tokens = -llama_tokenize(vocab, prompt, strlen(prompt), NULL, 0, false, false);
    if (n_prompt_tokens <= 0) {
        return generate_summary_simple(history, start_turn, end_turn, detail_level);
    }
    
    llama_token* prompt_tokens = malloc(n_prompt_tokens * sizeof(llama_token));
    if (!prompt_tokens) {
        return generate_summary_simple(history, start_turn, end_turn, detail_level);
    }
    
    llama_tokenize(vocab, prompt, strlen(prompt), prompt_tokens, n_prompt_tokens, false, false);
    
    // Create temporary context for summarization (don't pollute main KV cache)
    // We'll use sequence ID 1 for this temporary summarization
    llama_batch batch = llama_batch_init(n_prompt_tokens, 0, 1);
    batch.n_tokens = n_prompt_tokens;
    for (int i = 0; i < n_prompt_tokens; i++) {
        batch.token[i] = prompt_tokens[i];
        batch.pos[i] = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 1;  // Use sequence 1 for summarization
        batch.logits[i] = false;
    }
    batch.logits[n_prompt_tokens - 1] = true;
    
    // Decode prompt
    if (llama_decode(gov->llm_ctx, batch) != 0) {
        llama_batch_free(batch);
        free(prompt_tokens);
        return generate_summary_simple(history, start_turn, end_turn, detail_level);
    }
    
    llama_batch_free(batch);
    free(prompt_tokens);
    
    // Generate summary
    char* summary_buffer = malloc(2048);
    if (!summary_buffer) {
        // Clean up sequence 1
        llama_memory_t mem = llama_get_memory(gov->llm_ctx);
        llama_memory_seq_rm(mem, 1, 0, -1);
        return generate_summary_simple(history, start_turn, end_turn, detail_level);
    }
    summary_buffer[0] = '\0';
    
    // Create sampler for summary generation
    struct llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.3f));  // Low temp for focused summary
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(0));
    
    int current_pos = n_prompt_tokens;
    int generated = 0;
    
    while (generated < max_tokens) {
        llama_token next_token = llama_sampler_sample(sampler, gov->llm_ctx, -1);
        
        // Check for end of generation
        if (llama_vocab_is_eog(vocab, next_token)) break;
        
        // Decode token to text
        char token_text[128];
        int n_chars = llama_token_to_piece(vocab, next_token, token_text, sizeof(token_text), 0, false);
        if (n_chars > 0 && n_chars < (int)sizeof(token_text)) {
            token_text[n_chars] = '\0';
            strncat(summary_buffer, token_text, 2047 - strlen(summary_buffer));
        }
        
        // Check for stop sequences
        if (strstr(summary_buffer, "<|im_end|>") || strstr(summary_buffer, "<|im")) {
            break;
        }
        
        // Feed token back for next prediction (sequence 1)
        llama_batch next_batch = llama_batch_init(1, 0, 1);
        next_batch.n_tokens = 1;
        next_batch.token[0] = next_token;
        next_batch.pos[0] = current_pos;
        next_batch.n_seq_id[0] = 1;
        next_batch.seq_id[0][0] = 1;
        next_batch.logits[0] = true;
        
        if (llama_decode(gov->llm_ctx, next_batch) != 0) {
            llama_batch_free(next_batch);
            break;
        }
        
        llama_batch_free(next_batch);
        current_pos++;
        generated++;
    }
    
    llama_sampler_free(sampler);
    
    // Clean up sequence 1 from KV cache
    llama_memory_t mem = llama_get_memory(gov->llm_ctx);
    llama_memory_seq_rm(mem, 1, 0, -1);
    
    // Clean up any stop tokens from summary
    char* stop_marker = strstr(summary_buffer, "<|im");
    if (stop_marker) *stop_marker = '\0';
    
    // If summary is empty or too short, fall back
    if (strlen(summary_buffer) < 10) {
        free(summary_buffer);
        return generate_summary_simple(history, start_turn, end_turn, detail_level);
    }
    
    return summary_buffer;
#endif
}

/**
 * Generate simple text-based summary (fallback)
 */
static char* generate_summary_simple(
    conversation_history_t* history,
    uint32_t start_turn,
    uint32_t end_turn,
    const char* detail_level
) {
    // Determine max summary length based on detail level
    size_t max_summary_len = 200;  // brief
    if (strcmp(detail_level, "moderate") == 0) {
        max_summary_len = 500;
    } else if (strcmp(detail_level, "detailed") == 0) {
        max_summary_len = 1000;
    }
    
    // Allocate summary buffer
    char* summary = malloc(max_summary_len + 1);
    if (!summary) return NULL;
    
    size_t pos = 0;
    pos += snprintf(summary + pos, max_summary_len - pos,
                   "Summary of conversation (turns %u-%u):\n",
                   start_turn, end_turn);
    
    // Extract key information from each turn
    for (uint32_t i = start_turn; i <= end_turn && i < history->turn_count; i++) {
        conversation_turn_t* turn = &history->turns[i];
        const char* speaker = turn->is_user ? "User" : "Assistant";
        
        // Add turn preview (already truncated to 128 chars)
        size_t remaining = max_summary_len - pos;
        if (remaining < 50) break;  // Not enough space
        
        pos += snprintf(summary + pos, remaining,
                       "- %s: %s\n", speaker, turn->preview);
    }
    
    summary[max_summary_len] = '\0';
    return summary;
}

ethervox_result_t context_action_summarize_old(
    ethervox_governor_t* governor,
    ethervox_memory_store_t* memory_store,
    uint32_t keep_last_n_turns,
    const char* detail_level,
    context_action_result_t* result
) {
    if (!governor || !result) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    memset(result, 0, sizeof(context_action_result_t));
    
#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
    snprintf(result->error_msg, sizeof(result->error_msg), 
             "llama.cpp not available - cannot manage context");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
#else
    
    governor_internal_t* gov = (governor_internal_t*)governor;
    conversation_history_t* history = &gov->conversation_history;
    uint32_t total_turns = history->turn_count;
    
    // Validate parameters
    if (keep_last_n_turns >= total_turns) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Cannot summarize: only %u turns exist, requested to keep %u",
                 total_turns, keep_last_n_turns);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    uint32_t turns_to_summarize = total_turns - keep_last_n_turns;
    if (turns_to_summarize == 0) {
        result->success = true;
        return ETHERVOX_SUCCESS;  // Nothing to do
    }
    
    CTX_LOG("[Context] Summarizing old turns: %u turns -> %s summary, keeping last %u",
            turns_to_summarize, detail_level, keep_last_n_turns);
    
    // Generate summary of the turns we're about to drop (use LLM if available)
    char* summary = generate_summary_llm(governor, history, 0, turns_to_summarize - 1, detail_level);
    if (!summary) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Failed to generate summary");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Store summary in memory system (if available)
    uint64_t summary_id = 0;
    if (memory_store) {
        const char* tags[] = {"context_summary", "auto_generated", "conversation"};
        
        ethervox_result_t store_ret = ethervox_memory_store_add(
            memory_store,
            summary,
            tags,
            3,          // tag count
            0.95f,      // high importance
            false,      // not user message
            &summary_id
        );
        
        if (ethervox_is_success(store_ret)) {
            CTX_LOG("[Context] Stored summary in memory with ID %llu", 
                    (unsigned long long)summary_id);
        } else {
            CTX_LOG("[Context] Warning: Failed to store summary in memory");
        }
    } else {
        CTX_LOG("[Context] Warning: No memory store available, summary will be lost");
    }
    
    free(summary);
    
    // Now perform the actual shift (same as shift_window)
    int32_t drop_end = history->turns[turns_to_summarize - 1].kv_end;
    int32_t drop_start = history->turns[0].kv_start;
    int tokens_to_free = drop_end - drop_start;
    
    // Remove old tokens from KV cache
    llama_memory_t mem = llama_get_memory(gov->llm_ctx);
    if (mem) {
        llama_memory_seq_rm(mem, 0, drop_start, drop_end);
        CTX_LOG("[Context] Removed KV cache range: %d to %d (%d tokens)",
                drop_start, drop_end, tokens_to_free);
    } else {
        CTX_ERROR("[Context] WARNING: Failed to get memory handle, only updating tracking");
    }
    
    // Update turn tracking
    for (uint32_t i = 0; i < keep_last_n_turns; i++) {
        history->turns[i] = history->turns[i + turns_to_summarize];
        history->turns[i].kv_start -= tokens_to_free;
        history->turns[i].kv_end -= tokens_to_free;
    }
    
    // Update counts
    history->turn_count = keep_last_n_turns;
    gov->current_kv_pos -= tokens_to_free;
    gov->context_manager.last_gc_position = gov->current_kv_pos;
    
    // Fill result
    result->tokens_freed = tokens_to_free;
    result->turns_removed = turns_to_summarize;
    result->summary_memory_id = summary_id;
    result->success = true;
    
    CTX_LOG("[Context] Summarize complete: freed %d tokens from KV cache, stored summary ID %llu",
            tokens_to_free, (unsigned long long)summary_id);
    
    return ETHERVOX_SUCCESS;
#endif
}

// ============================================================================
// Action: Prune Unimportant (Stub)
// ============================================================================

ethervox_result_t context_action_prune_unimportant(
    ethervox_governor_t* governor,
    float importance_threshold,
    context_action_result_t* result
) {
    if (!governor || !result) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    memset(result, 0, sizeof(context_action_result_t));
    
    snprintf(result->error_msg, sizeof(result->error_msg),
             "Prune action not yet implemented");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
}
