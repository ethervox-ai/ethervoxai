/**
 * @file governor.c
 * @brief Governor orchestration system implementation
 *
 * The Governor is the central reasoning engine that coordinates between
 * LLM inference and tool execution. It maintains a conversation loop
 * until a complete response is generated or limits are reached.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/governor.h"
#include "governor_helpers.h"  // Clean, testable helper functions

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ethervox/chat_template.h"
#include "ethervox/config.h"
#include "ethervox/device_profile.h"  // Adaptive hardware configuration
#include "ethervox/error.h"
#include "ethervox/kv_cache_persistence.h"  // KV cache save/load for fast startup
#include "ethervox/tool_manifest.h"   // Manifest system for optimized prompts

#ifdef _WIN32
#include <malloc.h>  // For alloca on Windows
#endif

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#include <ggml.h>
#include <llama.h>
#define LLAMA_HEADER_AVAILABLE 1
#else
#define LLAMA_HEADER_AVAILABLE 0
#endif

#define GOV_LOG(...) ETHERVOX_LOGI(__VA_ARGS__)
#define GOV_ERROR(...) ETHERVOX_LOGE(__VA_ARGS__)

// External function to get configurable startup batch size (from Android/iOS JNI layer)
// Falls back to config.h default if not available (desktop/ESP32 builds)
extern int ethervox_get_startup_batch_size(void) __attribute__((weak));

// Provide default implementation for non-Android platforms
// This will be overridden by the Android-specific version when building for Android
__attribute__((weak))
int ethervox_get_startup_batch_size(void) {
  return ETHERVOX_GOVERNOR_STARTUP_BATCH_SIZE;
}

// Global flag to track llama backend initialization (should only happen once per process)
static bool g_llama_backend_initialized = false;

// Global flag to detect model corruption from llama.cpp error messages
static bool g_model_corruption_detected = false;

// Forward declarations for helper functions (defined after struct definition)
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
static bool parse_memory_search_text(const char* json_result, char* text_out, size_t text_out_size);
static int load_tokens_to_kv_cache(struct ethervox_governor* governor, const llama_token* tokens,
                                   int n_tokens, const char* log_prefix);
static int load_conversation_summary(struct ethervox_governor* governor,
                                     const char* tag_filter_json, const char* context_prefix,
                                     const char* log_prefix);
#endif

// GGML log callback to capture llama.cpp errors
static void governor_ggml_log_callback(enum ggml_log_level level, const char* text,
                                       void* user_data) {
  (void)user_data;
  // Remove trailing newline if present
  size_t len = strlen(text);
  char* clean_text = (char*)alloca(len + 1);
  // Use safe string copy (from helpers)
  if (len < len + 1) {
    memcpy(clean_text, text, len);
    clean_text[len] = '\0';
  }
  if (len > 0 && clean_text[len - 1] == '\n') {
    clean_text[len - 1] = '\0';
  }

  // Detect corruption in error messages
  if (level == GGML_LOG_LEVEL_ERROR) {
    if (strstr(clean_text, "corrupted") != NULL || strstr(clean_text, "incomplete") != NULL ||
        strstr(clean_text, "not within the file bounds") != NULL) {
      g_model_corruption_detected = true;
      GOV_ERROR("[llama.cpp] CORRUPTION DETECTED: %s", clean_text);
    } else {
      GOV_ERROR("[llama.cpp] %s", clean_text);
    }
  } else if (level == GGML_LOG_LEVEL_WARN) {
    GOV_LOG("[llama.cpp WARN] %s", clean_text);
  } else {
    GOV_LOG("[llama.cpp] %s", clean_text);
  }
}

// Progress callback for model loading
static bool governor_load_progress_callback(float progress, void* user_data) {
  // user_data contains our custom callback wrapper
  if (user_data) {
    typedef struct {
      ethervox_load_progress_callback callback;
      void* callback_user_data;
    } callback_context_t;

    callback_context_t* ctx = (callback_context_t*)user_data;
    if (ctx->callback) {
      char msg[128];
      snprintf(msg, sizeof(msg), "Loading model: %d%%", (int)(progress * 100.0f));
      ctx->callback("loading_model", progress, msg, ctx->callback_user_data);
    }
  }

  GOV_LOG("[Governor] Model load progress: %d%%", (int)(progress * 100.0f));
  return true;  // Continue loading
}

/**
 * Internal state for the Governor
 */
struct ethervox_governor {
  ethervox_governor_config_t config;
  ethervox_tool_registry_t* tool_registry;
  tool_manifest_registry_t* manifest_registry;  // Manifest system (optimized prompts)

#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  struct llama_model* llm_model;
  struct llama_context* llm_ctx;
  llama_pos system_prompt_token_count;
  llama_pos current_kv_pos;  // Track current position in KV cache
  char* model_path;

  // Saved system prompt for recovery after nuclear clear
  llama_token* system_prompt_tokens;
  int system_prompt_tokens_len;

  // Pre-tokenized static wrappers for speed optimization (from chat_template)
  llama_token* tool_result_prefix_tokens;  // chat_template->tool_result_start
  int tool_result_prefix_len;
  llama_token* tool_result_suffix_tokens;  // chat_template->tool_result_end
  int tool_result_suffix_len;
#else
  void* llm_model;  // Placeholder
  void* llm_ctx;
  int32_t system_prompt_token_count;
  int32_t current_kv_pos;  // Track current position in KV cache
  char* model_path;
#endif

  // KV Cache Debugging - Track all tokens for full cache dumps
  llama_token* kv_cache_tokens;  // All tokens currently in KV cache
  int kv_cache_tokens_capacity;  // Allocated capacity
  int kv_cache_tokens_count;     // Actual count
  bool kv_cache_tracking_enabled; // Enable/disable tracking (memory intensive)

  // State tracking
  uint32_t last_iteration_count;
  bool initialized;
  bool llm_loaded;
  bool tool_execution_enabled;        // Allow disabling tool execution (for optimization)
  bool system_prompt_lost;            // Track if nuclear clear wiped the system prompt
  bool tools_available;               // Track if tools are enabled (false in minimal mode)
  volatile bool interrupt_requested;  // Set by interrupt callback to abort generation

  // Context management
  context_manager_state_t context_manager;
  conversation_history_t conversation_history;
  uint32_t turn_counter;

  // Chat template for formatting
  const chat_template_t* chat_template;
};

// ============================================================================
// KV Cache Debugging Functions
// ============================================================================

#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE

/**
 * Track tokens added to KV cache for debugging
 */
static void track_kv_cache_tokens(struct ethervox_governor* governor, const llama_token* tokens, int n_tokens) {
  if (!governor || !governor->kv_cache_tracking_enabled || !tokens || n_tokens <= 0) {
    return;
  }

  // Ensure capacity
  int required_capacity = governor->kv_cache_tokens_count + n_tokens;
  if (required_capacity > governor->kv_cache_tokens_capacity) {
    int new_capacity = required_capacity * 2;
    error_context_t track_error = {0};
    llama_token* new_buffer = (llama_token*)safe_realloc(
        governor->kv_cache_tokens, 
        new_capacity * sizeof(llama_token),
        &track_error);
    if (!new_buffer) {
      GOV_ERROR("Failed to expand KV cache tracking buffer: %s", 
                error_context_get_message(&track_error));
      return;
    }
    governor->kv_cache_tokens = new_buffer;
    governor->kv_cache_tokens_capacity = new_capacity;
  }

  // Append tokens
  memcpy(governor->kv_cache_tokens + governor->kv_cache_tokens_count, 
         tokens, 
         n_tokens * sizeof(llama_token));
  governor->kv_cache_tokens_count += n_tokens;
}

/**
 * Dump entire KV cache contents to log (decoded to text)
 */
static void dump_kv_cache_contents(struct ethervox_governor* governor) {
  if (!governor || !governor->kv_cache_tracking_enabled || !governor->kv_cache_tokens) {
    GOV_LOG("KV cache tracking not enabled or no tokens tracked");
    return;
  }

  GOV_LOG("=== KV CACHE DUMP (%d tokens) ===", governor->kv_cache_tokens_count);
  
  const struct llama_vocab* vocab = llama_model_get_vocab(governor->llm_model);
  char full_text[65536] = {0};
  int text_pos = 0;
  
  // Decode all tokens
  for (int i = 0; i < governor->kv_cache_tokens_count && text_pos < sizeof(full_text) - 128; i++) {
    char token_str[128];
    int token_len = llama_token_to_piece(vocab, governor->kv_cache_tokens[i], 
                                          token_str, sizeof(token_str), 0, false);
    if (token_len > 0 && token_len < sizeof(token_str)) {
      token_str[token_len] = '\0';
      int remaining = sizeof(full_text) - text_pos - 1;
      if (token_len < remaining) {
        memcpy(full_text + text_pos, token_str, token_len);
        text_pos += token_len;
      }
    }
  }
  full_text[text_pos] = '\0';
  
  // Log in chunks (logcat has ~4KB line limit)
  GOV_LOG("Total decoded length: %d chars", text_pos);
  const int chunk_size = 1000;
  for (int i = 0; i < text_pos; i += chunk_size) {
    int remaining = text_pos - i;
    int current_chunk = remaining < chunk_size ? remaining : chunk_size;
    
    char chunk[chunk_size + 1];
    memcpy(chunk, full_text + i, current_chunk);
    chunk[current_chunk] = '\0';
    
    GOV_LOG("[KV_CACHE][%d-%d]: %s", i, i + current_chunk - 1, chunk);
  }
  
  GOV_LOG("=== END KV CACHE DUMP ===");
}

/**
 * Get KV cache contents as string (for UI display)
 * Returns malloc'd string that caller must free, or NULL if tracking disabled
 */
static char* get_kv_cache_contents_string(struct ethervox_governor* governor) {
  if (!governor || !governor->kv_cache_tracking_enabled || !governor->kv_cache_tokens) {
    return NULL;
  }
  
  if (!governor->llm_model) {
    return NULL;
  }
  
  const struct llama_vocab* vocab = llama_model_get_vocab(governor->llm_model);
  
  // Allocate large buffer for full cache contents (2MB should be enough)
  size_t buffer_size = 2 * 1024 * 1024;
  char* full_text = (char*)malloc(buffer_size);
  if (!full_text) {
    return NULL;
  }
  
  int text_pos = 0;
  
  // Decode all tokens
  for (int i = 0; i < governor->kv_cache_tokens_count && text_pos < buffer_size - 256; i++) {
    char token_str[256];
    int token_len = llama_token_to_piece(vocab, governor->kv_cache_tokens[i], 
                                          token_str, sizeof(token_str), 0, false);
    if (token_len > 0 && token_len < sizeof(token_str)) {
      token_str[token_len] = '\0';
      int remaining = buffer_size - text_pos - 1;
      if (token_len < remaining) {
        memcpy(full_text + text_pos, token_str, token_len);
        text_pos += token_len;
      }
    }
  }
  full_text[text_pos] = '\0';
  
  return full_text;
}

/**
 * Reset KV cache tracking after clear
 */
static void reset_kv_cache_tracking(struct ethervox_governor* governor, int start_pos) {
  if (!governor || !governor->kv_cache_tracking_enabled) {
    return;
  }
  
  if (start_pos == 0) {
    // Full clear
    governor->kv_cache_tokens_count = 0;
    GOV_LOG("[KV_TRACK] Full reset - cleared all tracked tokens");
  } else if (start_pos < governor->kv_cache_tokens_count) {
    // Partial clear - keep tokens before start_pos
    governor->kv_cache_tokens_count = start_pos;
    GOV_LOG("[KV_TRACK] Partial reset - kept %d tokens", start_pos);
  }
}

#endif  // ETHERVOX_WITH_LLAMA

// ============================================================================
// Helper Function Implementations (Summary Loading)
// ============================================================================

#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE

/**
 * Parse JSON memory search result to extract the "text" field
 * Format: {"results":[{"text":"...", "relevance":0.95}], "count":1}
 */
static bool parse_memory_search_text(const char* json_result, char* text_out,
                                     size_t text_out_size) {
  if (!json_result || !text_out || text_out_size == 0) {
    return false;
  }

  text_out[0] = '\0';

  char* text_start = strstr(json_result, "\"text\":\"");
  if (!text_start) {
    return false;
  }

  text_start += 8;  // Skip past "text":"
  char* text_end = strstr(text_start, "\",\"relevance\"");
  if (!text_end) {
    text_end = strstr(text_start, "\"}");
  }

  if (text_end) {
    size_t len = text_end - text_start;
    if (len < text_out_size) {
      strncpy(text_out, text_start, len);
      text_out[len] = '\0';
      return true;
    }
  }

  return false;
}

/**
 * Load tokenized text into KV cache in chunks
 * Returns number of tokens loaded, or -1 on error
 */
static int load_tokens_to_kv_cache(struct ethervox_governor* governor, const llama_token* tokens,
                                   int n_tokens, const char* log_prefix) {
  if (!governor || !tokens || n_tokens <= 0) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  int chunk_size = 512;
  for (int i = 0; i < n_tokens; i += chunk_size) {
    int chunk_len = (i + chunk_size > n_tokens) ? (n_tokens - i) : chunk_size;

    llama_batch batch = llama_batch_init(chunk_len, 0, 1);
    batch.n_tokens = chunk_len;
    for (int j = 0; j < chunk_len; j++) {
      batch.token[j] = tokens[i + j];
      batch.pos[j] = governor->current_kv_pos + i + j;  // Account for outer loop position
      batch.n_seq_id[j] = 1;
      batch.seq_id[j][0] = 0;
      batch.logits[j] = false;
    }

    int decode_result = llama_decode(governor->llm_ctx, batch);
    
    if (decode_result == 0) {
      // Track tokens successfully added to KV cache
      track_kv_cache_tokens(governor, &tokens[i], chunk_len);
    }
    
    llama_batch_free(batch);

    if (decode_result != 0) {
      GOV_ERROR("%s: Failed to decode chunk %d/%d (error %d, tokens %d-%d/%d)", log_prefix,
                i / chunk_size + 1, (n_tokens + chunk_size - 1) / chunk_size, decode_result, i,
                i + chunk_len - 1, n_tokens);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
  }

  // Update position after all chunks processed
  governor->current_kv_pos += n_tokens;
  return n_tokens;
}

/**
 * Search memory for conversation summary and load into KV cache
 * Returns 0 on success (even if no summary found), -1 on error
 */
static int load_conversation_summary(struct ethervox_governor* governor,
                                     const char* tag_filter_json, const char* context_prefix,
                                     const char* log_prefix) {
  if (!governor || !tag_filter_json || !context_prefix || !log_prefix) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Find memory_search tool
  ethervox_tool_t* memory_search_tool = NULL;
  for (uint32_t i = 0; i < governor->tool_registry->tool_count; i++) {
    if (strcmp(governor->tool_registry->tools[i].name, "memory_search") == 0) {
      memory_search_tool = &governor->tool_registry->tools[i];
      break;
    }
  }

  if (!memory_search_tool) {
    GOV_LOG("%s: memory_search tool not available", log_prefix);
    return ETHERVOX_SUCCESS;  // Not an error, just unavailable
  }

  // Build search args
  char search_args[512];
  snprintf(search_args, sizeof(search_args),
           "{\"query\":null,"
           "\"tag_filter\":%s,"
           "\"limit\":1}",
           tag_filter_json);

  char* search_result = NULL;
  char* search_error = NULL;

  int search_status = memory_search_tool->execute(search_args, &search_result, &search_error);

  if (search_status != 0 || !search_result) {
    GOV_LOG("%s: No previous conversation summary found", log_prefix);
    free(search_result);
    free(search_error);
    return ETHERVOX_SUCCESS;  // Not an error
  }

  // Parse JSON to extract text
  char summary_text[4096];
  if (!parse_memory_search_text(search_result, summary_text, sizeof(summary_text))) {
    GOV_LOG("%s: No valid summary found in memory (empty or malformed)", log_prefix);
    free(search_result);
    free(search_error);
    return ETHERVOX_SUCCESS;
  }

  free(search_result);
  free(search_error);

  // Strip metadata header if present (e.g., "[Manual Cache Clear - Context Summary]\\n\\n")
  char* actual_summary = summary_text;
  if (summary_text[0] == '[') {
    // Look for end of header (]\n\n or ]\\ n\\ n for escaped)
    char* header_end = strstr(summary_text, "]\\n\\n");
    if (header_end) {
      actual_summary = header_end + 5;  // Skip "]\n\n" (with escapes: 5 chars)
    } else {
      header_end = strstr(summary_text, "]\n\n");
      if (header_end) {
        actual_summary = header_end + 3;  // Skip "]\n\n"
      }
    }
  }

  // Additional validation: check summary length is reasonable
  size_t summary_len = strlen(actual_summary);
  if (summary_len < 10) {
    GOV_ERROR("%s: Summary too short (%zu chars), likely corrupt", log_prefix, summary_len);
    return ETHERVOX_SUCCESS;
  }
  if (summary_len > 3500) {
    GOV_LOG("%s: Warning - summary unusually long (%zu chars)", log_prefix, summary_len);
  }

  GOV_LOG("%s: Found conversation summary (%zu chars)", log_prefix, summary_len);

  // Build context restoration prompt
  char context_restore_prompt[8192];
  snprintf(context_restore_prompt, sizeof(context_restore_prompt), "%s\n%s\n\n%s", context_prefix,
           actual_summary,  // Use stripped summary without metadata header
           strcmp(log_prefix, "STARTUP") == 0
               ? "Ready to continue our conversation."
               : "Continue conversation naturally based on this context.");

  // Tokenize
  // Use safe_malloc for error-checked allocation
  error_context_t alloc_error = {0};
  llama_token* restore_tokens = (llama_token*)safe_malloc(2048 * sizeof(llama_token), &alloc_error);
  if (!restore_tokens) {
    GOV_ERROR("Failed to allocate restore_tokens: %s", error_context_get_message(&alloc_error));
    return false;
  }
  if (!restore_tokens) {
    GOV_ERROR("%s: Failed to allocate token buffer", log_prefix);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  const struct llama_vocab* vocab = llama_model_get_vocab(governor->llm_model);
  int n_restore = llama_tokenize(vocab, context_restore_prompt, strlen(context_restore_prompt),
                                 restore_tokens, 2048,
                                 true,  // add_special
                                 false  // parse_special
  );

  if (n_restore <= 0) {
    GOV_ERROR("%s: Failed to tokenize summary (returned %d)", log_prefix, n_restore);
    free(restore_tokens);
    return ETHERVOX_SUCCESS;  // Not a fatal error
  }

  if (n_restore >= 2048) {
    GOV_ERROR("%s: Summary tokenized to maximum buffer size (%d), likely truncated", log_prefix,
              n_restore);
    free(restore_tokens);
    return ETHERVOX_SUCCESS;
  }

  // Sanity check: reasonable token count for a summary
  if (n_restore < 5) {
    GOV_ERROR("%s: Summary produced too few tokens (%d), likely corrupt", log_prefix, n_restore);
    free(restore_tokens);
    return ETHERVOX_SUCCESS;
  }

  GOV_LOG("%s: Tokenized summary into %d tokens", log_prefix, n_restore);

  // Load into KV cache
  int loaded = load_tokens_to_kv_cache(governor, restore_tokens, n_restore, log_prefix);
  free(restore_tokens);

  if (loaded > 0) {
    GOV_LOG("%s: Loaded %d context tokens into KV cache", log_prefix, loaded);
    GOV_LOG("%s: Context restoration complete", log_prefix);
  }

  return loaded > 0 ? 0 : -1;
}

#endif  // ETHERVOX_WITH_LLAMA

// ============================================================================
// End Helper Function Implementations
// ============================================================================

/**
 * Extract tool calls from LLM response
 * Looks for <tool_call name="..." attr="..." /> tags
 *
 * Returns: Number of tool calls found, fills tool_calls array
 */
static int extract_tool_calls(const char* response, char** tool_calls, int max_calls) {
  if (!response || !tool_calls)
    return ETHERVOX_SUCCESS;

  int count = 0;
  const char* pos = response;

  while (count < max_calls) {
    // Find next tool call
    const char* start = strstr(pos, "<tool_call");
    if (!start)
      break;

    const char* end = strstr(start, "/>");
    if (!end)
      break;

    end += 2;  // Include the />

    // Extract full tag
    size_t tag_len = end - start;
    error_context_t tool_error = {0};
    tool_calls[count] = (char*)safe_malloc(tag_len + 1, &tool_error);
    if (!tool_calls[count]) {
      GOV_ERROR("Failed to allocate tool_call: %s", error_context_get_message(&tool_error));
      continue;
    }
    if (!tool_calls[count])
      break;

    strncpy(tool_calls[count], start, tag_len);
    tool_calls[count][tag_len] = '\0';

    count++;
    pos = end;
  }

  return count;
}

/**
 * Extract JSON-format tool calls from LLM response (Granite 4.0 format)
 * Looks for <tool_call>\n{"name": "...", "arguments": {...}}\n</tool_call>
 *
 * Returns: Number of tool calls found, fills tool_calls array with JSON strings
 */
static int extract_tool_calls_json(const char* response, char** tool_calls, int max_calls) {
  if (!response || !tool_calls)
    return ETHERVOX_SUCCESS;

  int count = 0;
  const char* pos = response;

  while (count < max_calls) {
    // Find opening tag
    const char* start = strstr(pos, "<tool_call>");
    if (!start)
      break;

    const char* json_start = start + 11;  // Skip "<tool_call>"

    // Find closing tag (optional - handle incomplete tool calls gracefully)
    const char* end = strstr(json_start, "</tool_call>");
    const char* json_end;
    
    if (end) {
      // Normal case: closing tag found
      json_end = end;
    } else {
      // Fallback: No closing tag - try to extract complete JSON object
      // This handles cases where model stops generating before closing tag
      GOV_LOG("WARNING: Tool call without closing tag, attempting to extract JSON");
      
      // Find the end of the JSON object by counting braces
      const char* p = json_start;
      
      // Skip whitespace to find opening brace
      while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
        p++;
      }
      
      if (*p != '{') {
        GOV_ERROR("Tool call JSON doesn't start with '{', skipping");
        break;
      }
      
      int brace_count = 0;
      bool in_string = false;
      bool escaped = false;
      
      while (*p) {
        if (escaped) {
          escaped = false;
          p++;
          continue;
        }
        
        if (*p == '\\') {
          escaped = true;
          p++;
          continue;
        }
        
        if (*p == '"' && !escaped) {
          in_string = !in_string;
        }
        
        if (!in_string) {
          if (*p == '{') {
            brace_count++;
          } else if (*p == '}') {
            brace_count--;
            if (brace_count == 0) {
              // Found complete JSON object
              json_end = p + 1;
              end = json_end;  // Set end so we can continue parsing
              GOV_LOG("Extracted incomplete tool call (no closing tag)");
              break;
            }
          }
        }
        p++;
      }
      
      if (brace_count != 0) {
        GOV_ERROR("Incomplete JSON in tool call, skipping");
        break;
      }
    }

    // Skip whitespace at start of JSON
    while (json_start < json_end && (*json_start == ' ' || *json_start == '\n' || *json_start == '\r' ||
                                *json_start == '\t')) {
      json_start++;
    }

    // Find actual JSON end (skip trailing whitespace)
    while (json_end > json_start && (*(json_end - 1) == ' ' || *(json_end - 1) == '\n' ||
                                     *(json_end - 1) == '\r' || *(json_end - 1) == '\t')) {
      json_end--;
    }

    // Extract JSON content
    size_t json_len = json_end - json_start;
    if (json_len > 0) {
      error_context_t tool_error = {0};
      tool_calls[count] = (char*)safe_malloc(json_len + 1, &tool_error);
      if (!tool_calls[count]) {
        GOV_ERROR("Failed to allocate JSON tool_call: %s", error_context_get_message(&tool_error));
        continue;
      }
      if (!tool_calls[count])
        break;

      strncpy(tool_calls[count], json_start, json_len);
      tool_calls[count][json_len] = '\0';

      count++;
    }

    // Move past the tool call (either closing tag or end of JSON)
    if (strstr(end, "</tool_call>") == end) {
      pos = end + 12;  // Move past "</tool_call>"
    } else {
      pos = end;  // Move past JSON end
    }
  }

  return count;
}

// ============================================================================
// Context Management Helper Functions
// ============================================================================

/**
 * Check context health status based on current KV cache usage
 */
static context_health_t check_context_health(ethervox_governor_t* gov) {
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  int n_ctx = llama_n_ctx(gov->llm_ctx);
  int current_pos = gov->current_kv_pos;
  float usage = (float)current_pos / n_ctx;

  if (usage < 0.60f)
    return CTX_HEALTH_OK;
  if (usage < 0.80f)
    return CTX_HEALTH_WARNING;
  if (usage < 0.95f)
    return CTX_HEALTH_CRITICAL;
  return CTX_HEALTH_OVERFLOW;
#else
  return CTX_HEALTH_OK;  // No monitoring without llama.cpp
#endif
}

/**
 * Initialize conversation history
 */
static int init_conversation_history(conversation_history_t* history, uint32_t initial_capacity) {
  error_context_t hist_error = {0};
  history->turns = (conversation_turn_t*)safe_malloc(initial_capacity * sizeof(conversation_turn_t), &hist_error);
  if (!history->turns) {
    GOV_ERROR("Failed to allocate conversation history: %s", error_context_get_message(&hist_error));
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }

  history->turn_count = 0;
  history->capacity = initial_capacity;
  return ETHERVOX_SUCCESS;
}

/**
 * Append a turn to conversation history
 */
static int append_turn(conversation_history_t* history, const conversation_turn_t* turn) {
  error_context_t hist_error = {0};
  // Resize if needed
  if (history->turn_count >= history->capacity) {
    uint32_t new_capacity = history->capacity * 2;
    conversation_turn_t* new_turns =
        (conversation_turn_t*)safe_realloc(history->turns, new_capacity * sizeof(conversation_turn_t), &hist_error);
    if (!new_turns && new_capacity > 0) {
      GOV_ERROR("Failed to expand conversation history: %s", error_context_get_message(&hist_error));
      return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }

    history->turns = new_turns;
    history->capacity = new_capacity;
  }

  history->turns[history->turn_count++] = *turn;
  return ETHERVOX_SUCCESS;
}

/**
 * Cleanup conversation history
 */
static void cleanup_conversation_history(conversation_history_t* history) {
  if (history->turns) {
    free(history->turns);
    history->turns = NULL;
  }
  history->turn_count = 0;
  history->capacity = 0;
}

/**
 * Parse XML attribute from tool call tag
 * Example: name="calculator_compute" -> returns "calculator_compute"
 */
static char* parse_attribute(const char* tag, const char* attr_name) {
  if (!tag || !attr_name)
    return NULL;

  // Look for attr_name="value"
  char search[64];
  snprintf(search, sizeof(search), "%s=\"", attr_name);

  const char* start = strstr(tag, search);
  if (!start)
    return NULL;

  start += strlen(search);

  const char* end = strchr(start, '"');
  if (!end)
    return NULL;

  size_t len = end - start;
  error_context_t val_error = {0};
  char* value = (char*)safe_malloc(len + 1, &val_error);
  if (!value) {
    GOV_ERROR("Failed to allocate value: %s", error_context_get_message(&val_error));
    return NULL;
  }
  if (!value)
    return NULL;

  strncpy(value, start, len);
  value[len] = '\0';

  return value;
}

/**
 * Execute a single tool call (JSON format for Granite 4.0)
 * Input: JSON string like {"name": "calculator_compute", "arguments": {"expression": "17*23"}}
 * Extracts name and arguments, calls tool
 */
static int execute_tool_call_json(const char* tool_call_json, ethervox_tool_registry_t* registry,
                                  char** result, char** error) {
  if (!tool_call_json || !registry || !result || !error) {
    if (error)
      *error = strdup("Invalid parameters passed to execute_tool_call_json");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Simple JSON parsing - find "name" field
  const char* name_start = strstr(tool_call_json, "\"name\"");
  if (!name_start) {
    *error = strdup("Missing 'name' field in JSON tool call");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Find the value after "name":
  const char* name_value_start = strchr(name_start, ':');
  if (!name_value_start) {
    *error = strdup("Malformed 'name' field in JSON tool call");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  name_value_start++;

  // Skip whitespace and opening quote
  while (*name_value_start == ' ' || *name_value_start == '\t' || *name_value_start == '\n') {
    name_value_start++;
  }
  if (*name_value_start == '"')
    name_value_start++;

  // Find end quote
  const char* name_value_end = strchr(name_value_start, '"');
  if (!name_value_end) {
    *error = strdup("Malformed 'name' value in JSON tool call");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Extract tool name
  size_t name_len = name_value_end - name_value_start;
  char* tool_name = malloc(name_len + 1);
  if (!tool_name) {
    *error = strdup("Memory allocation failed for tool name");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  strncpy(tool_name, name_value_start, name_len);
  tool_name[name_len] = '\0';

  // DEBUG: List all registered tools
  GOV_LOG("Looking for tool '%s' in registry with %u tools:", tool_name, registry->tool_count);
  for (uint32_t i = 0; i < registry->tool_count; i++) {
    GOV_LOG("  [%u] %s", i, registry->tools[i].name);
  }

  // Find tool in registry
  const ethervox_tool_t* tool = ethervox_tool_registry_find(registry, tool_name);
  if (!tool) {
    char err_msg[256];
    snprintf(err_msg, sizeof(err_msg), "Unknown tool: %s", tool_name);
    *error = strdup(err_msg);
    free(tool_name);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Find "arguments" field
  const char* args_start = strstr(tool_call_json, "\"arguments\"");
  char json_input[65536] = "{}";  // Default empty object

  if (args_start) {
    const char* args_value_start = strchr(args_start, ':');
    if (args_value_start) {
      args_value_start++;

      // Skip whitespace
      while (*args_value_start == ' ' || *args_value_start == '\t' || *args_value_start == '\n') {
        args_value_start++;
      }

      // Handle two formats:
      // 1. Object: "arguments": {"location": "..."}  (correct Granite format)
      // 2. String: "arguments": "{\"location\": \"...\"}" (model sometimes generates this)
      
      if (*args_value_start == '"') {
        // Format 2: Arguments as JSON string - extract the string content
        args_value_start++; // Skip opening quote
        
        // Find the closing quote (handle escaped quotes)
        const char* p = args_value_start;
        const char* args_end = NULL;
        while (*p) {
          if (*p == '\\' && *(p + 1) == '"') {
            p += 2;  // Skip escaped quote
            continue;
          }
          if (*p == '"') {
            args_end = p;
            break;
          }
          p++;
        }
        
        if (args_end) {
          // Copy the string content and unescape it
          size_t args_len = args_end - args_value_start;
          if (args_len < sizeof(json_input)) {
            size_t dst_idx = 0;
            const char* src = args_value_start;
            while (src < args_end && dst_idx < sizeof(json_input) - 1) {
              if (*src == '\\' && src + 1 < args_end) {
                // Handle escape sequences
                src++;
                switch (*src) {
                  case 'n':  json_input[dst_idx++] = '\n'; break;
                  case 't':  json_input[dst_idx++] = '\t'; break;
                  case 'r':  json_input[dst_idx++] = '\r'; break;
                  case '"':  json_input[dst_idx++] = '"';  break;
                  case '\\': json_input[dst_idx++] = '\\'; break;
                  default:   json_input[dst_idx++] = *src; break;
                }
                src++;
              } else {
                json_input[dst_idx++] = *src++;
              }
            }
            json_input[dst_idx] = '\0';
            GOV_LOG("Extracted arguments from JSON string format: %s", json_input);
          }
        }
      } else if (*args_value_start == '{') {
        // Format 1: Arguments as JSON object (correct format)
        int brace_count = 0;
        const char* p = args_value_start;
        const char* args_end = NULL;

        // Find matching closing brace
        while (*p) {
          if (*p == '{')
            brace_count++;
          else if (*p == '}') {
            brace_count--;
            if (brace_count == 0) {
              args_end = p + 1;
              break;
            }
          }
          p++;
        }

        if (args_end) {
          size_t args_len = args_end - args_value_start;
          if (args_len < sizeof(json_input)) {
            strncpy(json_input, args_value_start, args_len);
            json_input[args_len] = '\0';
            GOV_LOG("Extracted arguments from JSON object format: %s", json_input);
          }
        }
      }
    }
  }

  GOV_LOG("Executing tool '%s' with JSON: %s", tool->name, json_input);

  free(tool_name);

  // Safety checks before execution
  if (!tool->execute) {
    *error = strdup("Tool has NULL execute pointer");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (!result || !error) {
    GOV_ERROR("Invalid result or error pointers passed to execute_tool_call_json");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Resource checks for audio tools (speak/listen)
  // Note: The tools themselves handle graceful degradation, but we can provide
  // better error messages here if execution context indicates resources unavailable
  if (strcmp(tool->name, "speak") == 0 || strcmp(tool->name, "listen") == 0) {
    GOV_LOG("Audio tool '%s' requested - resource availability will be checked by tool callback",
            tool->name);
  }

  // Execute the tool
  int exec_result = tool->execute(json_input, result, error);

  if (exec_result != 0) {
    if (!*error) {
      *error = strdup("Tool execution failed (no error message provided)");
    }
    GOV_ERROR("Tool '%s' failed: %s", tool->name, *error);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (!*result) {
    *result = strdup("(no output)");
  }

  GOV_LOG("Tool '%s' succeeded: %s", tool->name, *result);
  return ETHERVOX_SUCCESS;
}

/**
 * Execute a single tool call (XML attribute format)
 * Parses the XML tag, extracts attributes, builds JSON, calls tool
 */
static int execute_tool_call(const char* tool_call_xml, ethervox_tool_registry_t* registry,
                             char** result, char** error) {
  // Extract tool name
  char* tool_name = parse_attribute(tool_call_xml, "name");
  if (!tool_name) {
    *error = strdup("Missing 'name' attribute in tool call");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Find tool in registry
  const ethervox_tool_t* tool = ethervox_tool_registry_find(registry, tool_name);
  if (!tool) {
    char err_msg[256];
    snprintf(err_msg, sizeof(err_msg), "Unknown tool: %s", tool_name);
    *error = strdup(err_msg);
    free(tool_name);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Build JSON input from XML attributes
  // For now, support simple attribute -> JSON key mapping
  // TODO: More sophisticated JSON building for complex tools

  // Increased buffer to 64KB to accommodate large content parameters (e.g., file_write with large
  // text)
  char json_input[65536] = "{";
  bool first = true;

  // Common attributes to extract
  const char* attrs[] = {
      // Calculator/compute
      "expression", "value", "percentage", "operation", "from", "to", "amount", "duration_seconds",
      "label", "hour", "minute", "decimal_places",
      // File tools
      "directory", "file_path", "pattern", "recursive", "path", "enable", "description",
      // Memory tools
      "text", "new_text", "content", "tags", "query", "limit", "window_size", "format",
      "importance", "min_importance", "max_age_hours", "is_user", "memory_id", "memory_ids",
      "filepath", "older_than_seconds", "importance_threshold", "tag_filter", "focus_topic",
      "correction",
      // Startup prompt tools
      "prompt_text", NULL};

  for (int i = 0; attrs[i] != NULL; i++) {
    char* attr_value = parse_attribute(tool_call_xml, attrs[i]);
    if (attr_value) {
      // Security fix: Check buffer space before appending separator
      size_t current_len = strlen(json_input);
      size_t remaining = sizeof(json_input) - current_len - 1;
      
      if (!first && remaining > 3) {
        strncat(json_input, ", ", remaining);
        current_len = strlen(json_input);  // Update after append
        remaining = sizeof(json_input) - current_len - 1;
      }

      // Check if value is numeric (must have at least one digit)
      // Exception: memory_id, file_path, filepath, tags should always be strings
      bool force_string =
          (strcmp(attrs[i], "memory_id") == 0 || strcmp(attrs[i], "file_path") == 0 ||
           strcmp(attrs[i], "filepath") == 0 || strcmp(attrs[i], "path") == 0 ||
           strcmp(attrs[i], "tags") == 0 || strcmp(attrs[i], "query") == 0 ||
           strcmp(attrs[i], "text") == 0 || strcmp(attrs[i], "new_text") == 0 ||
           strcmp(attrs[i], "content") == 0 || strcmp(attrs[i], "directory") == 0 ||
           strcmp(attrs[i], "pattern") == 0 || strcmp(attrs[i], "format") == 0 ||
           strcmp(attrs[i], "label") == 0 || strcmp(attrs[i], "description") == 0 ||
           strcmp(attrs[i], "prompt_text") == 0 || strcmp(attrs[i], "tag_filter") == 0 ||
           strcmp(attrs[i], "focus_topic") == 0 || strcmp(attrs[i], "correction") == 0);

      bool is_numeric = false;
      if (!force_string) {
        bool has_digit = false;
        const char* p = attr_value;

        if (*p == '-' || *p == '+')
          p++;

        while (*p) {
          if (*p >= '0' && *p <= '9') {
            has_digit = true;
          } else if (*p != '.') {
            has_digit = false;
            break;
          }
          p++;
        }

        is_numeric = has_digit && (p > attr_value);
      }

      // Append field directly to json_input (reuse current_len and remaining)
      current_len = strlen(json_input);
      remaining = sizeof(json_input) - current_len - 1;

      if (is_numeric) {
        snprintf(json_input + current_len, remaining, "\"%s\": %s", attrs[i], attr_value);
      } else {
        snprintf(json_input + current_len, remaining, "\"%s\": \"%s\"", attrs[i], attr_value);
      }

      first = false;
      free(attr_value);
    }
  }

  // Security fix: Use strncat to safely append closing brace
  size_t final_len = strlen(json_input);
  size_t final_remaining = sizeof(json_input) - final_len - 1;
  if (final_remaining > 1) {
    strncat(json_input, "}", final_remaining);
  } else {
    GOV_ERROR("JSON buffer overflow prevented - input too long");
    free(tool_name);
    *error = strdup("Tool arguments too large");
    return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
  }

  GOV_LOG("Built JSON for tool '%s': %s", tool->name, json_input);

  free(tool_name);

  // Safety checks before execution
  if (!tool->execute) {
    *error = strdup("Tool has NULL execute pointer");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (!result || !error) {
    GOV_ERROR("Invalid result or error pointers passed to execute_tool_call");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Resource checks for audio tools (speak/listen)
  // Note: The tools themselves handle graceful degradation, but we can provide
  // better error messages here if execution context indicates resources unavailable
  if (strcmp(tool->name, "speak") == 0 || strcmp(tool->name, "listen") == 0) {
    GOV_LOG("Audio tool '%s' requested - resource availability will be checked by tool callback",
            tool->name);
  }

  // Execute tool
  int ret = tool->execute(json_input, result, error);
  if (ret != 0) {
    GOV_ERROR("Tool '%s' execution failed with code %d, error: %s", tool->name, ret,
              error && *error ? *error : "unknown");
  } else {
    GOV_LOG("Tool '%s' executed successfully, result length: %zu", tool->name,
            result && *result ? strlen(*result) : 0);
  }
  return ret;
}

ethervox_result_t ethervox_governor_load_model(ethervox_governor_t* governor,
                                               const char* model_path,
                                               const char* cache_dir,
                                               ethervox_load_progress_callback progress_callback,
                                               void* user_data) {
  if (!governor || !model_path)
    return ETHERVOX_ERROR_INVALID_ARGUMENT;

#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  GOV_ERROR("llama.cpp not available - cannot load model");
  return ETHERVOX_ERROR_INVALID_ARGUMENT;
#else

  // If a model is already loaded, unload it first
  if (governor->llm_loaded) {
    GOV_LOG("[Governor] Unloading existing model before loading new one");

    // Free pre-tokenized wrappers
    if (governor->tool_result_prefix_tokens) {
      free(governor->tool_result_prefix_tokens);
      governor->tool_result_prefix_tokens = NULL;
      governor->tool_result_prefix_len = 0;
    }
    if (governor->tool_result_suffix_tokens) {
      free(governor->tool_result_suffix_tokens);
      governor->tool_result_suffix_tokens = NULL;
      governor->tool_result_suffix_len = 0;
    }

    // Free context and model
    if (governor->llm_ctx) {
      llama_free(governor->llm_ctx);
      governor->llm_ctx = NULL;
    }
    if (governor->llm_model) {
      llama_model_free(governor->llm_model);
      governor->llm_model = NULL;
    }
    if (governor->model_path) {
      free(governor->model_path);
      governor->model_path = NULL;
    }

    governor->llm_loaded = false;
    governor->system_prompt_token_count = 0;
    governor->current_kv_pos = 0;
  }

  // Clear corruption flag before attempting load
  g_model_corruption_detected = false;

  GOV_LOG("[Governor] Loading model: %s", model_path);
  
  // Initialize device profiling for adaptive configuration
  ethervox_device_profile_init();

  // Auto-detect chat template from model path
  governor->chat_template = chat_template_get(CHAT_TEMPLATE_AUTO, model_path);
  if (!governor->chat_template) {
    GOV_LOG("ERROR: Failed to get chat template");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  GOV_LOG("[Governor] chat_template assigned: %p (type: %d)", (void*)governor->chat_template, governor->chat_template->type);

  // Initialize llama.cpp backend (only once per process)
  if (!g_llama_backend_initialized) {
    GOV_LOG("[Governor] Initializing llama.cpp backend");
    // Set up log callback to capture llama.cpp errors
    ggml_log_set(governor_ggml_log_callback, NULL);

    // Enable verbose llama.cpp logging
    llama_log_set(governor_ggml_log_callback, NULL);

    llama_backend_init();

    // CRITICAL: Load ggml backends (CPU, etc.)
    GOV_LOG("[Governor] Loading ggml backends...");
    ggml_backend_load_all();
    int backend_count = ggml_backend_reg_count();
    GOV_LOG("[Governor] Loaded %d ggml backends", backend_count);

    if (backend_count == 0) {
      GOV_ERROR("[Governor] FATAL: No backends loaded! Cannot load models.");
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    g_llama_backend_initialized = true;
  } else {
    GOV_LOG("[Governor] llama.cpp backend already initialized, skipping");
  }

  // Model params - Use runtime config (with config.h fallbacks)
  struct llama_model_params model_params = llama_model_default_params();
  model_params.n_gpu_layers = governor->config.gpu_layers;
  model_params.use_mmap = ETHERVOX_GOVERNOR_USE_MMAP;
  model_params.use_mlock = false;  // Don't lock memory (let OS manage)

  // Set up callback context for model loading progress
  typedef struct {
    ethervox_load_progress_callback callback;
    void* callback_user_data;
  } callback_context_t;

  callback_context_t callback_ctx = {.callback = progress_callback,
                                     .callback_user_data = user_data};

  model_params.progress_callback = governor_load_progress_callback;
  model_params.progress_callback_user_data = progress_callback ? &callback_ctx : NULL;

  GOV_LOG("[Governor] Model params: n_gpu_layers=%d, use_mmap=%d", model_params.n_gpu_layers,
          model_params.use_mmap);

  // Verify file exists and is readable before trying to load
  FILE* test_file = fopen(model_path, "rb");
  if (!test_file) {
    GOV_ERROR("Cannot open model file: %s (errno: %d - %s)", model_path, errno, strerror(errno));
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Get file size
  fseek(test_file, 0, SEEK_END);
  long file_size = ftell(test_file);
  fseek(test_file, 0, SEEK_SET);
  fclose(test_file);

  GOV_LOG("[Governor] Model file accessible: %ld bytes", file_size);

  // Double-check all parameters before loading
  GOV_LOG("[Governor] Final check before load:");
  GOV_LOG("  - model_path: %s", model_path);
  GOV_LOG("  - n_gpu_layers: %d", model_params.n_gpu_layers);
  GOV_LOG("  - use_mmap: %d", model_params.use_mmap);
  GOV_LOG("  - use_mlock: %d", model_params.use_mlock);
  GOV_LOG("  - vocab_only: %d", model_params.vocab_only);
  GOV_LOG("  - check_tensors: %d", model_params.check_tensors);

  // Load model
  // Additional debug: Check file one more time before load
  FILE* f_test = fopen(model_path, "rb");
  if (!f_test) {
    GOV_ERROR("[Governor] CRITICAL: Cannot open file just before llama load: %s", strerror(errno));
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  fclose(f_test);
  GOV_LOG("[Governor] File test just before load: SUCCESS");

  // Check backend count one more time
  int backend_count_preload = ggml_backend_reg_count();
  GOV_LOG("[Governor] Backend count just before load: %d", backend_count_preload);

  if (backend_count_preload == 0) {
    GOV_ERROR("[Governor] FATAL: Backends disappeared before load!");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  GOV_LOG("[Governor] About to call llama_model_load_from_file...");
  GOV_LOG("[Governor] Path: %s", model_path);
  GOV_LOG("[Governor] Model params address: %p", (void*)&model_params);

  governor->llm_model = llama_model_load_from_file(model_path, model_params);

  GOV_LOG("[Governor] llama_model_load_from_file returned: %p", (void*)governor->llm_model);

  if (!governor->llm_model) {
    // Check if backends still there after failed load
    int backend_count_postfail = ggml_backend_reg_count();
    GOV_ERROR("[Governor] Load failed, backend count after: %d", backend_count_postfail);
  }

  if (!governor->llm_model) {
    GOV_ERROR("Failed to load model from %s", model_path);
    GOV_ERROR("Model params used: n_gpu_layers=%d, use_mmap=%d, use_mlock=%d",
              model_params.n_gpu_layers, model_params.use_mmap, model_params.use_mlock);

    // Try to understand why it failed
    GOV_ERROR("Possible reasons:");
    GOV_ERROR("  1. Incompatible GGUF version");
    GOV_ERROR("  2. Corrupted model file");
    GOV_ERROR("  3. Insufficient memory");
    GOV_ERROR("  4. Missing backend support");

    // Return -2 if corruption was detected, -1 otherwise
    if (g_model_corruption_detected) {
      GOV_ERROR("[Governor] Model corruption detected - returning error code -2");
      return -2;
    }
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  GOV_LOG("[Governor] Model loaded successfully");

  // Set privacy mode based on config (secret mode)
  extern void ethervox_memory_set_privacy_mode(bool disable_logging);
  ethervox_memory_set_privacy_mode(governor->config.disable_memory_logging);

  // Context params - Use runtime config with adaptive hardware detection
  struct llama_context_params ctx_params = llama_context_default_params();

  // DEBUG: Log the context size that's about to be used
  GOV_LOG("[Governor] Setting n_ctx from config.context_size = %u", governor->config.context_size);
  ctx_params.n_ctx = governor->config.context_size;
  GOV_LOG("[Governor] ctx_params.n_ctx set to = %d", ctx_params.n_ctx);

  // Adaptive batch size based on device tier
  int optimal_batch_size = ethervox_device_profile_get_optimal_batch_size();
  ctx_params.n_batch = optimal_batch_size;
  GOV_LOG("[Governor] Using adaptive batch size: %d (was %d)", optimal_batch_size, ETHERVOX_GOVERNOR_BATCH_SIZE);
  
  // CRITICAL: Set n_ubatch for generation (micro-batch size)
  // Lower values = faster generation on mobile (less memory bandwidth)
  // For 3B models on 4GB RAM: 64 is optimal (512 default is too high)
  ctx_params.n_ubatch = 64;
  GOV_LOG("[Governor] Using n_ubatch: %d (generation micro-batch)", ctx_params.n_ubatch);
  
  // Adaptive thread count based on CPU cores
  int optimal_threads = ethervox_device_profile_get_optimal_threads();
  ctx_params.n_threads = optimal_threads;
  ctx_params.n_threads_batch = optimal_threads;
  GOV_LOG("[Governor] Using adaptive threads: %d (CPU cores: %d)", 
          optimal_threads, ethervox_device_profile_get_cpu_cores());
  
  // Adaptive flash attention based on device capability
  bool use_flash = ethervox_device_profile_should_use_flash_attention();
  ctx_params.flash_attn_type = use_flash ? 1 : 0;  // 1=ENABLED, 0=DISABLED
  GOV_LOG("[Governor] Flash attention: %s (device tier: %d)", 
          use_flash ? "ENABLED" : "DISABLED", ethervox_device_profile_get_tier());
  
  // Adaptive KV cache type based on available memory
  int optimal_kv_type = ethervox_device_profile_get_optimal_kv_cache_type();
  const char* kv_type_names[] = {"F32", "F16", "Q8_0", "Q4_0"};
  int kv_name_idx = (optimal_kv_type == 1) ? 1 : (optimal_kv_type == 7) ? 2 : 3;
  ctx_params.type_k = optimal_kv_type;
  ctx_params.type_v = optimal_kv_type;
  GOV_LOG("[Governor] Using adaptive KV cache: %s (available RAM: %ld MB)", 
          kv_type_names[kv_name_idx], ethervox_device_profile_get_total_ram_mb());
  
  ctx_params.no_perf = false;  // Enable performance tracking for metrics

  // Create context
  governor->llm_ctx = llama_init_from_model(governor->llm_model, ctx_params);
  if (!governor->llm_ctx) {
    GOV_ERROR("Failed to create llama context");
    llama_model_free(governor->llm_model);
    governor->llm_model = NULL;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  GOV_LOG("Context created successfully");

  // Get vocab early - needed for both cache load and normal generation paths
  const struct llama_vocab* vocab = llama_model_get_vocab(governor->llm_model);

  // === KV CACHE OPTIMIZATION ===
  // Check for saved KV cache to skip expensive system prompt processing
  bool cache_loaded = false;
  if (cache_dir) {
    char cache_path[512];
    ethervox_result_t cache_path_result = ethervox_kv_cache_get_path(
        model_path, cache_dir, cache_path, sizeof(cache_path)
    );
    
    if (ethervox_is_success(cache_path_result) && 
        ethervox_kv_cache_exists(cache_path, model_path)) {
      
      GOV_LOG("═══════════════════════════════════════════════════════");
      GOV_LOG("KV CACHE FOUND - Loading system prompt instantly");
      GOV_LOG("Cache file: %s", cache_path);
      GOV_LOG("═══════════════════════════════════════════════════════");
      
      if (progress_callback) {
        progress_callback("processing_prompt", 0.1f, 
                        "Loading system prompt from cache...", user_data);
      }
      
      ethervox_result_t load_result = ethervox_kv_cache_load(governor, cache_path);
      
      if (ethervox_is_success(load_result)) {
        cache_loaded = true;
        
        if (progress_callback) {
          progress_callback("processing_prompt", 1.0f, 
                          "✓ System prompt ready (loaded from cache)", user_data);
        }
        
        GOV_LOG("KV cache loaded successfully");
        GOV_LOG("Skipped system prompt generation");
      } else {
        GOV_LOG("KV cache load failed (code %d), will generate from scratch", load_result);
      }
    }
  }
  
  // If cache was loaded successfully, skip system prompt generation
  if (cache_loaded) {
    // Set up essentials that would normally be set during system prompt processing
    governor->model_path = strdup(model_path);
    governor->tools_available = (governor->config.system_prompt_mode != ETHERVOX_GOVERNOR_MODE_MINIMAL);
    
    // System prompt token count and KV pos were restored by ethervox_kv_cache_load
    // via ethervox_governor_set_system_tokens
    
    // We'll handle tool result wrapper tokenization at the end (shared code)
    goto setup_tool_wrappers;
  }
  
  // No cache - generate system prompt normally
  if (progress_callback) {
    progress_callback("processing_prompt", 0.0f, 
                    "Generating system prompt (first launch)...", user_data);
  }
  GOV_LOG("No KV cache found - generating system prompt (~5 minutes)");

  // Log tool registry state BEFORE building system prompt
  GOV_LOG("Tool registry has %u tools registered", governor->tool_registry->tool_count);
  for (uint32_t i = 0; i < governor->tool_registry->tool_count; i++) {
    GOV_LOG("  Tool %u: %s", i, governor->tool_registry->tools[i].name);
  }

  // Build system prompt based on mode (full vs minimal)
  // Increased to 16KB to accommodate all tools and examples (full mode)
  char system_prompt[16384];

  if (governor->config.system_prompt_mode == ETHERVOX_GOVERNOR_MODE_MINIMAL) {
    // Minimal mode: brief prompt without tools for fast mobile loading
    GOV_LOG("Building MINIMAL system prompt (fast mobile mode, tools unavailable)");

    // Ultra-brief system prompt - optimized for mobile startup speed
    // ~50 tokens vs ~1200 tokens in full mode (96% reduction)
    const char* minimal_prompt =
        "You are EthervoxAI, a helpful and concise voice assistant. "
        "Your tools are currently unavailable, so provide direct answers based on your knowledge. "
        "Be brief, accurate, and conversational.";

    strncpy(system_prompt, minimal_prompt, sizeof(system_prompt) - 1);
    system_prompt[sizeof(system_prompt) - 1] = '\0';

    GOV_LOG("Minimal system prompt (%zu chars) - tools disabled", strlen(system_prompt));

    // Mark tools as unavailable in governor state
    governor->tools_available = false;

  } else {
    // Full mode: complete system prompt with all tools and capabilities
    GOV_LOG("Building FULL system prompt (all tools available)");

    // Use manifest-based prompt builder if available (optimized), otherwise fall back to old system
    ethervox_result_t build_result;
    if (governor->manifest_registry && governor->manifest_registry->tools_available) {
      GOV_LOG("Using MANIFEST system prompt builder (optimized prompts)");
      GOV_LOG("  Fallback level: %u (%s)",
              governor->manifest_registry->fallback_level,
              ethervox_tool_fallback_level_name(governor->manifest_registry->fallback_level));
      GOV_LOG("  Optimization loaded: %s",
              governor->manifest_registry->optimization_loaded ? "YES" : "NO");
      GOV_LOG("  Tool count: %u", governor->manifest_registry->header.tool_count);
      
      build_result = ethervox_governor_build_system_prompt_with_manifest(
          governor->manifest_registry, governor->chat_template, system_prompt, sizeof(system_prompt));
    } else {
      GOV_LOG("Using LEGACY system prompt builder (no optimizations)");
      if (governor->manifest_registry) {
        GOV_LOG("  Manifest exists but tools_available=false (fallback level %u)",
                governor->manifest_registry->fallback_level);
      } else {
        GOV_LOG("  No manifest registry available");
      }
      
      build_result = ethervox_tool_registry_build_system_prompt(
          governor->tool_registry, governor->chat_template, system_prompt, sizeof(system_prompt),
          NULL,  // TODO: Wire memory_store for adaptive learning
          governor->model_path);
    }
    
    if (build_result < 0) {
      GOV_ERROR("Failed to build system prompt (error code: %d)", build_result);
      llama_free(governor->llm_ctx);
      llama_model_free(governor->llm_model);
      governor->llm_ctx = NULL;
      governor->llm_model = NULL;
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    size_t prompt_length = strlen(system_prompt);
    GOV_LOG("Full system prompt built (%zu chars, buffer size: %zu)", prompt_length, sizeof(system_prompt));

    // Mark tools as available in governor state
    governor->tools_available = true;
  }

  // Log system prompt in chunks to avoid logcat truncation (Android limit ~1KB per line)
  size_t prompt_len = strlen(system_prompt);
  GOV_LOG("===== SYSTEM PROMPT (%zu chars) =====", prompt_len);
  
  const size_t log_chunk_size = 250;  // Log in 250-char chunks to fit within logcat line limit
  for (size_t i = 0; i < prompt_len; i += log_chunk_size) {
    size_t remaining = prompt_len - i;
    size_t current_chunk = remaining < log_chunk_size ? remaining : log_chunk_size;
    
    // Create null-terminated chunk
    char chunk[log_chunk_size + 1];
    strncpy(chunk, system_prompt + i, current_chunk);
    chunk[current_chunk] = '\0';
    
    GOV_LOG("PROMPT[%zu-%zu]: %s", i, i + current_chunk - 1, chunk);
  }
  GOV_LOG("===== END SYSTEM PROMPT =====");

  // Tokenize system prompt (vocab already declared earlier for cache path compatibility)
  int n_tokens = -llama_tokenize(vocab, system_prompt, strlen(system_prompt), NULL, 0, true, false);

  if (n_tokens <= 0) {
    GOV_ERROR("Failed to tokenize system prompt");
    llama_free(governor->llm_ctx);
    llama_model_free(governor->llm_model);
    governor->llm_ctx = NULL;
    governor->llm_model = NULL;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  llama_token* tokens = malloc(n_tokens * sizeof(llama_token));
  if (!tokens) {
    GOV_ERROR("Failed to allocate token buffer");
    llama_free(governor->llm_ctx);
    llama_model_free(governor->llm_model);
    governor->llm_ctx = NULL;
    governor->llm_model = NULL;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  llama_tokenize(vocab, system_prompt, strlen(system_prompt), tokens, n_tokens, true, false);

  // Save a copy of the system prompt tokens for recovery after nuclear clear
  governor->system_prompt_tokens = malloc(n_tokens * sizeof(llama_token));
  if (governor->system_prompt_tokens) {
    memcpy(governor->system_prompt_tokens, tokens, n_tokens * sizeof(llama_token));
    governor->system_prompt_tokens_len = n_tokens;
    GOV_LOG("Saved system prompt tokens (%d tokens) for nuclear clear recovery", n_tokens);
  } else {
    GOV_ERROR("Failed to allocate memory for system prompt backup");
  }

  GOV_LOG("Processing %d system prompt tokens in chunks...", n_tokens);

  // Call progress callback for start of system prompt processing
  if (progress_callback) {
    progress_callback("processing_prompt", 0.0f, "Processing system prompt...", user_data);
  }

  // Use SMALL batches for startup prompt to reduce time-to-first-token (TTFT)
  // This significantly improves perceived responsiveness during model loading
  // Android/iOS can override via JNI, desktop/ESP32 use config.h default
  int chunk_size = ethervox_get_startup_batch_size();
  GOV_LOG("Using startup batch size: %d tokens per batch (optimized for fast loading)", chunk_size);
  
  // Allocate batch once outside loop for maximum efficiency
  llama_batch batch = llama_batch_init(chunk_size, 0, 1);
  
  for (int i = 0; i < n_tokens; i += chunk_size) {
    int chunk_len = (i + chunk_size > n_tokens) ? (n_tokens - i) : chunk_size;
    bool is_final_chunk = (i + chunk_size >= n_tokens);

    // Reuse the batch, just update the token count and contents
    batch.n_tokens = chunk_len;
    for (int j = 0; j < chunk_len; j++) {
      batch.token[j] = tokens[i + j];
      batch.pos[j] = i + j;  // Absolute position from start
      batch.n_seq_id[j] = 1;
      batch.seq_id[j][0] = 0;   // Use sequence 0 for everything
      batch.logits[j] = false;  // Skip logits for all tokens
    }
    // Only compute logits for the very last token of the entire system prompt
    if (is_final_chunk) {
      batch.logits[chunk_len - 1] = true;
    }

    if (llama_decode(governor->llm_ctx, batch) != 0) {
      GOV_ERROR("Failed to process system prompt chunk at token %d", i);
      llama_batch_free(batch);
      free(tokens);
      llama_free(governor->llm_ctx);
      llama_model_free(governor->llm_model);
      governor->llm_ctx = NULL;
      governor->llm_model = NULL;
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Track tokens added to KV cache for debugging
    track_kv_cache_tokens(governor, &tokens[i], chunk_len);

    // Report progress after each chunk
    int tokens_processed = i + chunk_len;
    float progress = (float)tokens_processed / (float)n_tokens;
    int percent = (int)(progress * 100.0f);

    // Log progress
    char progress_msg[128];
    snprintf(progress_msg, sizeof(progress_msg), "System prompt processing: %d%% (%d/%d tokens)",
             percent, tokens_processed, n_tokens);
    GOV_LOG("%s", progress_msg);

    // Call progress callback
    if (progress_callback && !is_final_chunk) {  // Don't duplicate the final 100% callback
      if (!progress_callback("processing_prompt", progress, progress_msg, user_data)) {
        // User requested cancellation
        GOV_LOG("System prompt processing cancelled by user");
        free(tokens);
        llama_free(governor->llm_ctx);
        llama_model_free(governor->llm_model);
        governor->llm_ctx = NULL;
        governor->llm_model = NULL;
        return ETHERVOX_ERROR_INTERRUPTED;
      }
    }
  }
  
  // Free the batch after all chunks processed
  llama_batch_free(batch);

  free(tokens);

  // Call progress callback for completion
  if (progress_callback) {
    progress_callback("processing_prompt", 0.98f, "Saving cache for faster future startups...", user_data);
  }

  governor->system_prompt_token_count = n_tokens;
  governor->current_kv_pos = n_tokens;  // Start after system prompt
  governor->model_path = strdup(model_path);
  
  // Save KV cache for next app launch
  if (cache_dir) {
    char cache_path[512];
    ethervox_result_t cache_path_result = ethervox_kv_cache_get_path(
        model_path, cache_dir, cache_path, sizeof(cache_path)
    );
    
    if (ethervox_is_success(cache_path_result)) {
      ethervox_result_t save_result = ethervox_kv_cache_save(governor, cache_path);
      if (ethervox_is_success(save_result)) {
        GOV_LOG("✓ KV cache saved to: %s", cache_path);
        GOV_LOG("✓ Next app start will load from cache (~3-5 seconds)");
      } else {
        GOV_LOG("Failed to save KV cache (code %d) - next startup will regenerate", save_result);
      }
    }
  }
  
  if (progress_callback) {
    progress_callback("processing_prompt", 1.0f, "✓ System prompt ready", user_data);
  }

setup_tool_wrappers:
  // Pre-tokenize static tool result wrappers for speed
  // Validate chat_template before accessing members
  if (!governor->chat_template) {
    GOV_LOG("ERROR: chat_template is NULL during tool result tokenization");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  const char* prefix = governor->chat_template->tool_result_start;
  const char* suffix = governor->chat_template->tool_result_end;
  
  GOV_LOG("Pre-tokenizing tool result wrappers:");
  GOV_LOG("  Prefix text: '%s'", prefix);
  GOV_LOG("  Suffix text: '%s'", suffix);

  governor->tool_result_prefix_len =
      -llama_tokenize(vocab, prefix, strlen(prefix), NULL, 0, false, false);
  governor->tool_result_prefix_tokens =
      malloc(governor->tool_result_prefix_len * sizeof(llama_token));
  if (governor->tool_result_prefix_tokens) {
    llama_tokenize(vocab, prefix, strlen(prefix), governor->tool_result_prefix_tokens,
                   governor->tool_result_prefix_len, false, false);
    
    // Decode back to verify
    char prefix_verify[512] = {0};
    for (int i = 0; i < governor->tool_result_prefix_len && i < 50; i++) {
      char token_str[64];
      int token_len = llama_token_to_piece(vocab, governor->tool_result_prefix_tokens[i],
                                            token_str, sizeof(token_str), 0, false);
      if (token_len > 0 && token_len < sizeof(token_str)) {
        token_str[token_len] = '\0';
        strncat(prefix_verify, token_str, sizeof(prefix_verify) - strlen(prefix_verify) - 1);
      }
    }
    GOV_LOG("  Prefix tokenized to %d tokens, decodes to: '%s'",
            governor->tool_result_prefix_len, prefix_verify);
  }

  governor->tool_result_suffix_len =
      -llama_tokenize(vocab, suffix, strlen(suffix), NULL, 0, false, false);
  governor->tool_result_suffix_tokens =
      malloc(governor->tool_result_suffix_len * sizeof(llama_token));
  if (governor->tool_result_suffix_tokens) {
    llama_tokenize(vocab, suffix, strlen(suffix), governor->tool_result_suffix_tokens,
                   governor->tool_result_suffix_len, false, false);
    
    // Decode back to verify
    char suffix_verify[512] = {0};
    for (int i = 0; i < governor->tool_result_suffix_len && i < 50; i++) {
      char token_str[64];
      int token_len = llama_token_to_piece(vocab, governor->tool_result_suffix_tokens[i],
                                            token_str, sizeof(token_str), 0, false);
      if (token_len > 0 && token_len < sizeof(token_str)) {
        token_str[token_len] = '\0';
        strncat(suffix_verify, token_str, sizeof(suffix_verify) - strlen(suffix_verify) - 1);
      }
    }
    GOV_LOG("  Suffix tokenized to %d tokens, decodes to: '%s'",
            governor->tool_result_suffix_len, suffix_verify);
  }

  governor->llm_loaded = true;

  GOV_LOG("System prompt processed into KV cache (%d tokens)", 
          governor->system_prompt_token_count);
  GOV_LOG("Pre-tokenized wrappers: prefix=%d tokens, suffix=%d tokens",
          governor->tool_result_prefix_len, governor->tool_result_suffix_len);

  // ========================================================================
  // STARTUP CONTEXT RESTORATION - Load most recent conversation summary
  // ========================================================================
  // Provides conversation continuity across sessions by restoring context
  // from previous conversations. This is similar to the RELIGHT sequence
  // but happens at initial model load instead of after KV cache clear.

  GOV_LOG("STARTUP: Checking for previous conversation context...");
  // Use plain text without brackets to avoid confusing the model
  load_conversation_summary(governor, "[\"context_summary\"]", "Previous conversation context:",
                            "STARTUP");

  GOV_LOG("[Governor] Model loaded and ready");
  ;

  return ETHERVOX_SUCCESS;
#endif
}

/**
 * Unload the Governor model to free memory
 */
ethervox_result_t ethervox_governor_unload_model(ethervox_governor_t* governor) {
  if (!governor)
    return ETHERVOX_ERROR_INVALID_ARGUMENT;

  if (!governor->llm_loaded) {
    GOV_LOG("[Governor] Model already unloaded");
    return ETHERVOX_SUCCESS;  // Already unloaded, success
  }

#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  GOV_ERROR("llama.cpp not available");
  return ETHERVOX_ERROR_INVALID_ARGUMENT;
#else
  GOV_LOG("[Governor] Unloading model to free memory (keeping model path for reload)");

  // Free LLM context and model
  if (governor->llm_ctx) {
    llama_free(governor->llm_ctx);
    governor->llm_ctx = NULL;
  }
  if (governor->llm_model) {
    llama_model_free(governor->llm_model);
    governor->llm_model = NULL;
  }

  // Free pre-tokenized wrappers
  if (governor->tool_result_prefix_tokens) {
    free(governor->tool_result_prefix_tokens);
    governor->tool_result_prefix_tokens = NULL;
    governor->tool_result_prefix_len = 0;
  }
  if (governor->tool_result_suffix_tokens) {
    free(governor->tool_result_suffix_tokens);
    governor->tool_result_suffix_tokens = NULL;
    governor->tool_result_suffix_len = 0;
  }

  // Keep model_path for reload, but mark as unloaded
  governor->llm_loaded = false;
  governor->system_prompt_token_count = 0;
  governor->current_kv_pos = 0;

  // Clear conversation history since KV cache is gone
  cleanup_conversation_history(&governor->conversation_history);
  init_conversation_history(&governor->conversation_history, 32);
  governor->turn_counter = 0;

  GOV_LOG("[Governor] Model unloaded successfully (path preserved for reload)");

  return ETHERVOX_SUCCESS;
#endif
}

/**
 * Reload the Governor model using the previously saved model path
 */
ethervox_result_t ethervox_governor_reload_model(ethervox_governor_t* governor) {
  if (!governor)
    return ETHERVOX_ERROR_INVALID_ARGUMENT;

  if (governor->llm_loaded) {
    GOV_LOG("[Governor] Model already loaded");
    return ETHERVOX_SUCCESS;  // Already loaded, success
  }

  if (!governor->model_path) {
    GOV_ERROR("[Governor] Cannot reload - no previous model path saved");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  GOV_LOG("[Governor] Reloading model from: %s", governor->model_path);

  // Use the existing load_model function with the saved path
  // Note: cache_dir=NULL for reload (don't save/load cache on explicit reload)
  return ethervox_governor_load_model(governor, governor->model_path, NULL, NULL, NULL);
}

/**
 * Update Governor runtime configuration
 */
ethervox_result_t ethervox_governor_update_config(ethervox_governor_t* governor,
                                                  const ethervox_governor_config_t* config) {
  if (!governor || !config) {
    GOV_ERROR("Invalid governor or config");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Update configuration
  governor->config = *config;
  GOV_LOG("Governor config updated: gpu_layers=%u, context=%u, threads=%d", config->gpu_layers,
          config->context_size, config->n_threads);

  return ETHERVOX_SUCCESS;
}

/**
 * Check if the Governor model is currently loaded
 */
bool ethervox_governor_is_loaded(ethervox_governor_t* governor) {
  if (!governor)
    return false;
  return governor->llm_loaded;
}

/**
 * Get KV cache usage information
 */
float ethervox_governor_get_kv_cache_usage(ethervox_governor_t* governor, int32_t* current_pos, int32_t* context_size) {
  if (!governor || !governor->llm_loaded) {
    return -1.0f;
  }

  int32_t n_ctx = llama_n_ctx(governor->llm_ctx);
  int32_t pos = governor->current_kv_pos;

  if (current_pos) {
    *current_pos = pos;
  }
  if (context_size) {
    *context_size = n_ctx;
  }

  return (float)pos / (float)n_ctx;
}

/**
 * Get full KV cache contents as decoded text string
 */
char* ethervox_governor_get_kv_cache_contents(ethervox_governor_t* governor) {
  if (!governor) {
    return NULL;
  }
  
  return get_kv_cache_contents_string(governor);
}

/**
 * Initialize Governor with configuration
 */
ethervox_result_t ethervox_governor_init(ethervox_governor_t** governor,
                                         const ethervox_governor_config_t* config,
                                         ethervox_tool_registry_t* tool_registry) {
  if (!governor || !tool_registry)
    return ETHERVOX_ERROR_INVALID_ARGUMENT;

  error_context_t gov_error = {0};
  ethervox_governor_t* gov = (ethervox_governor_t*)safe_calloc(1, sizeof(ethervox_governor_t), &gov_error);
  if (!gov) {
    GOV_ERROR("Failed to allocate governor: %s", error_context_get_message(&gov_error));
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  if (!gov)
    return ETHERVOX_ERROR_INVALID_ARGUMENT;

  // Copy config or use defaults from config.h
  if (config) {
    gov->config = *config;
  } else {
    gov->config.confidence_threshold = ETHERVOX_GOVERNOR_CONFIDENCE_THRESHOLD;
    gov->config.max_iterations = ETHERVOX_GOVERNOR_MAX_ITERATIONS;
    gov->config.max_tool_calls_per_iteration = 10;
    gov->config.timeout_seconds = ETHERVOX_GOVERNOR_TIMEOUT_SECONDS;
    gov->config.max_tokens_per_response = 384;  // Balanced limit - allows stories while keeping responses focused
  }

  gov->tool_registry = tool_registry;
  gov->manifest_registry = NULL;       // Set later via ethervox_governor_set_manifest()
  gov->initialized = true;
  gov->llm_loaded = false;
  gov->tool_execution_enabled = true;  // Enabled by default
  gov->system_prompt_lost = false;     // System prompt present until nuclear clear
  gov->tools_available = true;         // Tools available by default (changes in minimal mode)
  gov->interrupt_requested = false;    // No interrupt pending
  gov->system_prompt_tokens = NULL;
  gov->system_prompt_tokens_len = 0;

  // Initialize KV cache tracking for debugging
  gov->kv_cache_tokens = NULL;
  gov->kv_cache_tokens_capacity = 0;
  gov->kv_cache_tokens_count = 0;
  gov->kv_cache_tracking_enabled = true;  // Enable by default for debugging

  // Initialize context manager
  memset(&gov->context_manager, 0, sizeof(context_manager_state_t));
  gov->context_manager.current_health = CTX_HEALTH_OK;
  gov->turn_counter = 0;

  // Initialize conversation history with capacity for 100 turns
  if (init_conversation_history(&gov->conversation_history, 100) != 0) {
    free(gov);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  *governor = gov;
  return ETHERVOX_SUCCESS;
}

/**
 * Execute user query with execution context for conversational awareness
 *
 * Extended version that includes execution context (input source, capabilities, callbacks)
 * for conversational AI features. The LLM receives context about where input came from
 * and can use conversation tools (speak, listen) accordingly.
 *
 * @param governor Governor instance
 * @param user_query User's natural language query
 * @param exec_context Execution context (NULL for legacy behavior)
 * @param response Output: Final response (caller must free)
 * @param error Output: Error message if failed (caller must free)
 * @param metrics Output: Confidence metrics (optional)
 * @param progress_callback Progress callback (optional)
 * @param token_callback Token streaming callback (optional)
 * @param user_data User data for callbacks (optional)
 * @return Governor status
 */
ethervox_governor_status_t ethervox_governor_execute_with_context(
    ethervox_governor_t* governor, const char* user_query,
    const ethervox_execution_context_t* exec_context, char** response, char** error,
    ethervox_confidence_metrics_t* metrics, ethervox_governor_progress_callback progress_callback,
    void (*token_callback)(const char* token, void* user_data), void* user_data) {
  // Set conversation callbacks if provided
  if (exec_context && exec_context->callbacks) {
    // External function in conversation_tools plugin
    extern void ethervox_conversation_tools_set_callbacks(void* callbacks);
    ethervox_conversation_tools_set_callbacks((void*)exec_context->callbacks);
  }

  // If no execution context, just pass through to standard execute
  if (!exec_context) {
    return ethervox_governor_execute(governor, user_query, response, error, metrics,
                                     progress_callback, token_callback, user_data);
  }

  // Build enhanced query with execution context metadata
  // This gives the LLM awareness of its environment and available capabilities
  char enhanced_query[8192];
  int offset = 0;

  // Add input source context
  const char* source_name = "unknown";
  switch (exec_context->source) {
    case ETHERVOX_INPUT_SOURCE_CLI:
      source_name = "command-line interface";
      break;
    case ETHERVOX_INPUT_SOURCE_VOICE:
      source_name = "voice conversation";
      break;
    case ETHERVOX_INPUT_SOURCE_API:
      source_name = "API request";
      break;
    default:
      source_name = exec_context->source_description ? exec_context->source_description : "unknown";
      break;
  }

  offset += snprintf(enhanced_query + offset, sizeof(enhanced_query) - offset,
                     "[Context: Input from %s", source_name);

  // Add capability information
  if (exec_context->tts_available || exec_context->microphone_available) {
    offset +=
        snprintf(enhanced_query + offset, sizeof(enhanced_query) - offset, " | Available tools:");

    if (exec_context->tts_available) {
      offset += snprintf(enhanced_query + offset, sizeof(enhanced_query) - offset,
                         " speak (text-to-speech)");
    }

    if (exec_context->microphone_available) {
      offset += snprintf(enhanced_query + offset, sizeof(enhanced_query) - offset,
                         "%s listen (microphone input)", exec_context->tts_available ? "," : "");
    }
  }

  // Add usage requirement for voice mode
  if (exec_context->source == ETHERVOX_INPUT_SOURCE_VOICE && exec_context->tts_available) {
    offset += snprintf(enhanced_query + offset, sizeof(enhanced_query) - offset,
                       " | IMPORTANT: You MUST use the 'speak' tool for ALL responses. "
                       "Do NOT return plain text - ALWAYS call speak(text=\"your response\")");
  }

  offset +=
      snprintf(enhanced_query + offset, sizeof(enhanced_query) - offset, "]\n\n%s", user_query);

  GOV_LOG("Context-aware query (%d chars): %s", offset, enhanced_query);

  // Execute with enhanced query
  return ethervox_governor_execute(governor, enhanced_query, response, error, metrics,
                                   progress_callback, token_callback, user_data);
}

// ============================================================================
// Helper Functions for Token Streaming and Loop Detection
// ============================================================================

/**
 * Check if text ends with a prefix of any stop sequence
 * Used by lookahead buffer to detect potential stop sequence formation
 * 
 * IMPORTANT: Requires at least 2 characters to match to avoid false positives
 * from single characters like '<' that appear in normal text
 */
static bool is_potential_stop_prefix_check(const chat_template_t* template, const char* text) {
  if (!template || !text) return false;
  
  size_t text_len = strlen(text);
  if (text_len == 0) return false;
  
  // Use template's stop sequences (centralized, consistent with all other checks)
  const char** stop_sequences = (const char**)template->stop_sequences;
  int stop_count = template->stop_sequence_count;
  
  for (int i = 0; i < stop_count && stop_sequences[i] != NULL; i++) {
    const char* stop_seq = stop_sequences[i];
    size_t stop_len = strlen(stop_seq);
    
    // Check if text could be building up to this stop sequence
    // CRITICAL FIX: Start from prefix_len=2 to avoid false positives on single chars like '<'
    // Single character matches are too ambiguous (could be '<' in story text, not a stop sequence)
    size_t min_prefix = (stop_len >= 2) ? 2 : 1;  // Require 2 chars unless stop sequence is only 1 char
    for (size_t prefix_len = min_prefix; prefix_len < stop_len && prefix_len <= text_len; prefix_len++) {
      const char* text_suffix = text + (text_len - prefix_len);
      if (strncmp(text_suffix, stop_seq, prefix_len) == 0) {
        return true;  // Text ends with a prefix of a stop sequence
      }
    }
  }
  
  return false;
}

/**
 * Simple hash function for n-gram loop detection
 * Uses DJB2 algorithm for fast, reasonable distribution
 */
static uint32_t compute_ngram_hash_djb2(const char* tokens[], int n) {
  uint32_t hash = 5381;  // DJB2 hash initial value
  for (int i = 0; i < n; i++) {
    const char* s = tokens[i];
    while (*s) {
      hash = ((hash << 5) + hash) + (unsigned char)(*s++);
    }
  }
  return hash;
}

/**
 * Execute Governor reasoning loop (legacy API - for backward compatibility)
 *
 * Flow:
 * 1. Submit query to LLM (appending to KV cache after system prompt)
 * 2. Parse response for tool calls
 * 3. If tool calls present, execute them and add results to conversation
 * 4. If no tool calls, return the response as final answer
 * 5. Repeat until answer generated, max iterations, or timeout
 */
ethervox_governor_status_t ethervox_governor_execute(
    ethervox_governor_t* governor, const char* user_query, char** response, char** error,
    ethervox_confidence_metrics_t* metrics, ethervox_governor_progress_callback progress_callback,
    void (*token_callback)(const char* token, void* user_data), void* user_data) {
  if (!governor || !governor->initialized || !user_query || !response) {
    if (error)
      *error = strdup("Invalid parameters");
    return ETHERVOX_GOVERNOR_ERROR;
  }

  // Check if LLM is loaded
  if (!governor->llm_loaded || !governor->llm_ctx) {
    if (error)
      *error = strdup("Governor model not loaded - call ethervox_governor_load_model first");
    return ETHERVOX_GOVERNOR_ERROR;
  }

#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  if (error)
    *error = strdup("llama.cpp not available");
  return ETHERVOX_GOVERNOR_ERROR;
#else

  // Reset iteration counter and interrupt flag
  governor->last_iteration_count = 0;
  governor->interrupt_requested = false;

  // Only clear KV cache if we're running low on space or this is explicitly requested
  // This allows conversation history to accumulate naturally
  llama_memory_t mem = llama_get_memory(governor->llm_ctx);
  int32_t max_pos = llama_memory_seq_pos_max(mem, 0);
  int n_ctx = llama_n_ctx(governor->llm_ctx);

  // Clear if we're past system prompt and getting close to context limit (>50% full)
  if (max_pos > governor->system_prompt_token_count && max_pos > (n_ctx / 2)) {
    GOV_LOG("KV cache clearing: removing positions %d to %d (was at %d%% capacity)",
            governor->system_prompt_token_count, max_pos, (max_pos * 100 / n_ctx));

    // ========================================================================
    // CONTEXT SUMMARIZATION - Preserve conversation knowledge before clearing
    // ========================================================================

    // Notify UI that summarization is starting
    if (progress_callback) {
      char summary_msg[256];
      snprintf(summary_msg, sizeof(summary_msg),
               "Context at %d%% - summarizing conversation before clearing...",
               (max_pos * 100 / n_ctx));
      progress_callback(ETHERVOX_GOVERNOR_EVENT_CONTEXT_SUMMARIZING, summary_msg, user_data);
    }

    // Generate an LLM-based summary of the recent conversation
    GOV_LOG("Generating conversation summary before clearing...");

    // Build conversation context from history
    char conversation_context[4096] = {0};
    int ctx_len = 0;

    // Include recent turns (last 10 or all if fewer)
    uint32_t start_turn = (governor->conversation_history.turn_count > 10)
                              ? (governor->conversation_history.turn_count - 10)
                              : 0;

    for (uint32_t i = start_turn; i < governor->conversation_history.turn_count; i++) {
      conversation_turn_t* turn = &governor->conversation_history.turns[i];
      int remaining = sizeof(conversation_context) - ctx_len - 1;

      if (remaining > 200) {  // Need space for role + preview
        int written = snprintf(conversation_context + ctx_len, remaining, "%s: %s\n",
                               turn->is_user ? "User" : "Assistant", turn->preview);
        if (written > 0 && written < remaining) {
          ctx_len += written;
        }
      }
    }

    // Create summarization prompt
    char summary_prompt[5120];
    snprintf(summary_prompt, sizeof(summary_prompt),
             "Summarize this conversation in 2-3 concise sentences, capturing key topics, "
             "decisions, and context that should be remembered:\n\n%s\n\n"
             "Summary (2-3 sentences):",
             conversation_context);

    // DISABLED: LLM-based summarization (causes bus errors, needs proper batch handling)
    // For now, use simple text-based summary
    char llm_summary[1024] = {0};

    // Create a simple summary from conversation context
    if (ctx_len > 0) {
      snprintf(llm_summary, sizeof(llm_summary),
               "Recent conversation covered %d turns with topics discussed.",
               (int)(governor->conversation_history.turn_count - start_turn));
    }

    GOV_LOG("Using simple conversation summary (LLM summarization disabled)");

  skip_llm_summary:;  // Empty statement to avoid C23 extension warning

    // Store the summary in memory
    ethervox_tool_t* memory_tool = NULL;
    for (uint32_t i = 0; i < governor->tool_registry->tool_count; i++) {
      if (strcmp(governor->tool_registry->tools[i].name, "memory_store") == 0) {
        memory_tool = &governor->tool_registry->tools[i];
        break;
      }
    }

    if (memory_tool) {
      // Use LLM summary if available, otherwise fallback to simple marker
      char summary_content[2048];
      if (llm_summary[0] != '\0') {
        snprintf(summary_content, sizeof(summary_content),
                 "[Conversation Summary - Context Cleared at %d%% capacity]\n\n%s\n\n"
                 "Turn count: %u. Full conversation history preserved in memory.",
                 (max_pos * 100 / n_ctx), llm_summary, governor->turn_counter);
      } else {
        snprintf(summary_content, sizeof(summary_content),
                 "Context cleared at position %d (%d%% full). "
                 "Conversation history up to this point preserved. "
                 "Turn count: %u. Use memory_search to recall earlier conversation if needed.",
                 max_pos, (max_pos * 100 / n_ctx), governor->turn_counter);
      }

      // Escape quotes for JSON
      char escaped_summary[4096];
      int esc_idx = 0;
      for (int i = 0; summary_content[i] && esc_idx < sizeof(escaped_summary) - 2; i++) {
        if (summary_content[i] == '"' || summary_content[i] == '\\') {
          escaped_summary[esc_idx++] = '\\';
        }
        if (summary_content[i] == '\n') {
          escaped_summary[esc_idx++] = '\\';
          escaped_summary[esc_idx++] = 'n';
        } else {
          escaped_summary[esc_idx++] = summary_content[i];
        }
      }
      escaped_summary[esc_idx] = '\0';

      // Build JSON args for memory_store
      char memory_args[8192];
      snprintf(memory_args, sizeof(memory_args),
               "{\"text\":\"%s\","
               "\"importance\":0.90,"
               "\"tags\":[\"context_summary\",\"auto_generated\",\"kv_cleared\"]}",
               escaped_summary);

      char* store_result = NULL;
      char* store_error = NULL;

      int store_status = memory_tool->execute(memory_args, &store_result, &store_error);
      if (store_status == 0) {
        GOV_LOG("Stored conversation summary in memory");
      } else {
        GOV_ERROR("Failed to store conversation summary: %s",
                  store_error ? store_error : "unknown error");
      }

      free(store_result);
      free(store_error);
    } else {
      GOV_LOG("Warning: memory_store tool not available, summary not persisted");
    }

    // Now clear the KV cache
    llama_memory_seq_rm(mem, 0, governor->system_prompt_token_count, -1);

    // Reset KV cache tracking - partial clear (keep system prompt)
    reset_kv_cache_tracking(governor, governor->system_prompt_token_count);

    // CRITICAL: After removal, re-query the actual max position
    // llama_memory_seq_rm doesn't immediately update internal position tracking
    // We must verify what llama.cpp thinks the position is NOW
    int32_t actual_max = llama_memory_seq_pos_max(mem, 0);

    // If llama.cpp still thinks we're beyond the system prompt, we have a problem
    bool needed_nuclear_clear = false;
    if (actual_max >= governor->system_prompt_token_count) {
      GOV_ERROR("KV cache removal failed: max_pos still at %d after clearing", actual_max);
      // Force-clear using llama_memory_clear (nuclear option - clears EVERYTHING)
      llama_memory_clear(mem, true);  // Clear both metadata and data
      actual_max = llama_memory_seq_pos_max(mem, 0);
      needed_nuclear_clear = true;
      GOV_LOG("Nuclear clear: completely wiped KV cache, max_pos now: %d", actual_max);
      
      // Reset KV cache tracking - full clear (nothing left)
      reset_kv_cache_tracking(governor, 0);
    }

    // Use the actual max position from llama.cpp, or system_prompt_token_count if empty
    if (needed_nuclear_clear) {
      // ========================================================================
      // RELIGHT SEQUENCE - Restore system prompt after catastrophic failure
      // ========================================================================
      // Like relighting a rocket engine after shutdown, we restore the governor
      // to full operational state by reprocessing the saved system prompt

      GOV_LOG("Nuclear clear wiped KV cache - initiating RELIGHT sequence...");

      // Attempt to restore system prompt from saved tokens
      if (governor->system_prompt_tokens && governor->system_prompt_tokens_len > 0) {
        GOV_LOG("RELIGHT: Restoring system prompt (%d tokens)...",
                governor->system_prompt_tokens_len);

        // Reprocess system prompt in chunks (same as initial load)
        // CRITICAL: Use actual n_batch from context (not hardcoded 1024)
        int actual_n_batch = llama_n_batch(governor->llm_ctx);
        int chunk_size = (actual_n_batch > 0) ? actual_n_batch : 512;
        GOV_LOG("Using chunk_size=%d (llama_n_batch=%d)", chunk_size, actual_n_batch);
        bool relight_successful = true;

        for (int i = 0; i < governor->system_prompt_tokens_len; i += chunk_size) {
          int chunk_len = (i + chunk_size > governor->system_prompt_tokens_len)
                              ? (governor->system_prompt_tokens_len - i)
                              : chunk_size;
          bool is_final_chunk = (i + chunk_size >= governor->system_prompt_tokens_len);

          // Create batch with explicit positions
          llama_batch batch = llama_batch_init(chunk_len, 0, 1);
          batch.n_tokens = chunk_len;
          for (int j = 0; j < chunk_len; j++) {
            batch.token[j] = governor->system_prompt_tokens[i + j];
            batch.pos[j] = i + j;
            batch.n_seq_id[j] = 1;
            batch.seq_id[j][0] = 0;
            batch.logits[j] = false;
          }
          // Compute logits only for the very last token
          if (is_final_chunk) {
            batch.logits[chunk_len - 1] = true;
          }

          if (llama_decode(governor->llm_ctx, batch) != 0) {
            GOV_ERROR("RELIGHT FAILED: Could not restore system prompt chunk at token %d", i);
            relight_successful = false;
            llama_batch_free(batch);
            break;
          }
          
          // Track restored tokens
          track_kv_cache_tokens(governor, &governor->system_prompt_tokens[i], chunk_len);

          llama_batch_free(batch);
        }

        if (relight_successful) {
          // System prompt successfully restored!
          governor->current_kv_pos = governor->system_prompt_token_count;
          governor->system_prompt_lost = false;
          GOV_LOG("RELIGHT COMPLETE: System prompt restored, tools re-enabled");
          GOV_LOG("KV cache restored to position %d (%d%% full)", governor->current_kv_pos,
                  (governor->current_kv_pos * 100 / n_ctx));

          // Load conversation summary from memory to restore context
          GOV_LOG("RELIGHT: Loading conversation summary from memory...");
          // Use plain text without brackets to avoid confusing the model
          load_conversation_summary(governor, "[\"context_summary\",\"kv_cleared\"]",
                                    "Restored conversation context:", "RELIGHT");

          // Notify UI that recovery was successful
          if (progress_callback) {
            char relight_msg[256];
            snprintf(relight_msg, sizeof(relight_msg),
                     "System recovered - full capabilities restored with conversation context");
            progress_callback(ETHERVOX_GOVERNOR_EVENT_CONTEXT_CLEARED, relight_msg, user_data);
          }
        } else {
          // Relight failed - governor remains in degraded state
          governor->current_kv_pos = 0;
          governor->system_prompt_lost = true;
          GOV_ERROR("RELIGHT FAILED: System prompt could not be restored");
          GOV_ERROR("Governor in degraded mode - tools disabled");
        }
      } else {
        // No saved system prompt - cannot relight
        governor->current_kv_pos = 0;
        governor->system_prompt_lost = true;
        GOV_ERROR("RELIGHT IMPOSSIBLE: No saved system prompt tokens");
        GOV_ERROR("Governor in degraded mode - continuing without tools");
      }
    } else {
      // Normal partial clear - system prompt is intact
      governor->current_kv_pos =
          (actual_max >= 0) ? actual_max + 1 : governor->system_prompt_token_count;
      GOV_LOG(
          "KV cache cleared: kept system prompt [0..%d), resuming at position %d (verified: %d)",
          governor->system_prompt_token_count, governor->current_kv_pos, actual_max);
    }

    // Notify UI that clearing is complete
    if (progress_callback) {
      char clear_msg[256];
      snprintf(clear_msg, sizeof(clear_msg),
               "Context cleared - conversation summary saved to memory (resuming from %d%%)",
               (governor->current_kv_pos * 100 / n_ctx));
      progress_callback(ETHERVOX_GOVERNOR_EVENT_CONTEXT_CLEARED, clear_msg, user_data);
    }
  } else if (max_pos >= governor->system_prompt_token_count) {
    // Continue from where we left off (normal case with system prompt)
    governor->current_kv_pos = max_pos + 1;
    GOV_LOG("KV cache continuing: from position %d (%d%% full)", governor->current_kv_pos,
            (governor->current_kv_pos * 100 / n_ctx));
  } else if (governor->system_prompt_lost) {
    // After nuclear clear: system prompt is gone, continue from actual position
    // This applies to all iterations until model is reloaded
    governor->current_kv_pos = (max_pos >= 0) ? max_pos + 1 : 0;
    GOV_LOG("KV cache continuing (post-nuclear): from position %d (%d%% full, NO SYSTEM PROMPT)",
            governor->current_kv_pos, (governor->current_kv_pos * 100 / n_ctx));
  } else {
    // First query - start after system prompt
    GOV_LOG("KV position decision: max_pos=%d, current_kv_pos=%d, system_prompt=%d", max_pos,
            governor->current_kv_pos, governor->system_prompt_token_count);
    governor->current_kv_pos = governor->system_prompt_token_count;
    GOV_LOG("KV cache initialized: starting at position %d", governor->current_kv_pos);
  }

  const struct llama_vocab* vocab = llama_model_get_vocab(governor->llm_model);

  // Build conversation history in Qwen2.5 format
  // Include recent context from memory if available
  char conversation[8192];
  size_t conv_pos = 0;

  // Check if we have a memory store to get recent conversation turns
  // We want the actual RECENT conversation, not a search result
  // Access the memory store directly if possible through the tool registry

  // For now, just use the current user query without trying to inject old context
  // The memory is better used explicitly via memory_search tool when needed

  // Add current user query using chat template
  // CRITICAL: Validate chat_template before dereferencing
  if (!governor->chat_template || (uintptr_t)governor->chat_template < 0x1000 ||
      ((uintptr_t)governor->chat_template >= 0x100000000 && (uintptr_t)governor->chat_template < 0x100001000)) {
    GOV_LOG("CRITICAL: chat_template corrupted (%p) during conversation formatting", (void*)governor->chat_template);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  conv_pos += snprintf(conversation + conv_pos, sizeof(conversation) - conv_pos, "%s%s%s%s",
                       governor->chat_template->user_start, user_query,
                       governor->chat_template->user_end, governor->chat_template->assistant_start);

  // ========================================================================
  // RESPONSE PREFILLING - Force tool usage for common queries
  // ========================================================================
  // The model is trained to answer queries like "What time is it?" directly
  // even with aggressive prompting. Prefilling forces it to start with a tool call.
  // 
  // IMPORTANT: These triggers should be SPECIFIC to avoid false positives.
  // Generic triggers like "what's" or "tell me" will fire on conversational
  // requests like "tell me a story" which should NOT use tools.
  // 
  // NOTE: Disabled by default - confuses custom trained models that already
  // know when to use tools. Enable via config.enable_tool_prefilling if needed.
  bool prefilled_tool_call = false;  // Track if we prefilled
  if (governor->tool_execution_enabled && governor->config.enable_tool_prefilling) {
    // Convert query to lowercase for case-insensitive matching
    char query_lower[512];
    size_t query_len = strlen(user_query);
    if (query_len >= sizeof(query_lower)) query_len = sizeof(query_lower) - 1;
    for (size_t i = 0; i < query_len; i++) {
      query_lower[i] = tolower((unsigned char)user_query[i]);
    }
    query_lower[query_len] = '\0';
    
    // Detect tool-triggering queries
    // Use SPECIFIC phrases to avoid false positives on conversational queries
    bool should_prefill_tool = false;
    const char* tool_triggers[] = {
      // Time tool triggers
      "what time", "current time", "time is it", "what's the time", "whats the time",
      
      // Weather tool triggers  
      "weather in", "weather at", "weather for", "temperature in", "forecast for",
      "how's the weather", "hows the weather", "what's the weather", "whats the weather",
      
      // Math tool triggers
      "calculate", "compute", "what is the result", "solve",
      
      NULL
    };
    
    for (int i = 0; tool_triggers[i] != NULL; i++) {
      if (strstr(query_lower, tool_triggers[i]) != NULL) {
        should_prefill_tool = true;
        GOV_LOG("PREFILL: Detected tool trigger '%s' in query", tool_triggers[i]);
        break;
      }
    }
    
    // Add tool call prefix to force model to use tools
    if (should_prefill_tool) {
      conv_pos += snprintf(conversation + conv_pos, sizeof(conversation) - conv_pos, 
                          "<tool_call>\n");
      prefilled_tool_call = true;
      GOV_LOG("PREFILL: Forcing response to start with '<tool_call>' tag");
    }
  }

  // Track how much of conversation has been processed to avoid re-processing
  size_t processed_length = 0;

  for (uint32_t iteration = 0; iteration < governor->config.max_iterations; iteration++) {
    governor->last_iteration_count = iteration + 1;

    // Initialize statistics for this iteration
    generation_stats_t gen_stats;
    stats_init(&gen_stats);
    double iteration_start_time = (double)clock() / CLOCKS_PER_SEC;

    // Check for interrupt request
    if (governor->interrupt_requested) {
      GOV_LOG("Governor interrupted at iteration %d", iteration + 1);
      if (error)
        *error = strdup("Generation interrupted by user");
      return ETHERVOX_GOVERNOR_INTERRUPTED;
    }

    // Notify iteration start
    if (progress_callback) {
      char iter_msg[128];
      snprintf(iter_msg, sizeof(iter_msg), "Iteration %d/%d", iteration + 1,
               governor->config.max_iterations);
      progress_callback(ETHERVOX_GOVERNOR_EVENT_ITERATION_START, iter_msg, user_data);
    }

    GOV_LOG("Governor iteration %d: %s", iteration + 1, conversation);

    // Notify thinking
    if (progress_callback) {
      progress_callback(ETHERVOX_GOVERNOR_EVENT_THINKING, "Analyzing query...", user_data);
    }

    // Generate LLM response
    // CRITICAL: Use heap allocation instead of stack for large buffer (64KB) to prevent stack overflow
    // Stack overflow can cause pointer corruption and SIGSEGV
    // Use safe_calloc for response buffer with error checking
    error_context_t buf_error = {0};
    char* llm_response_buffer = (char*)safe_calloc(65536, 1, &buf_error);
    if (!llm_response_buffer) {
      GOV_ERROR("Failed to allocate response buffer: %s", error_context_get_message(&buf_error));
      if (error) *error = strdup("Memory allocation failed");
      return ETHERVOX_GOVERNOR_ERROR;
    }
    
    // CRITICAL: If we prefilled a tool call, set inside_tool_call IMMEDIATELY
    // This prevents the first JSON tokens from leaking through before detection
    bool inside_tool_call = prefilled_tool_call;
    if (prefilled_tool_call) {
      GOV_LOG("PREFILL: Setting inside_tool_call=true from start of generation");
    }

    // Only tokenize and decode the NEW part of the conversation (what hasn't been processed yet)
    const char* new_content = conversation + processed_length;
    size_t new_content_len = strlen(new_content);

    GOV_LOG(
        "KV cache status: processed_length=%zu, conversation_length=%zu, new_content_length=%zu",
        processed_length, strlen(conversation), new_content_len);
    GOV_LOG("New content to process: '%s'", new_content_len > 0 ? new_content : "(none)");
    GOV_LOG("Full conversation buffer: '%s'", conversation);

    if (new_content_len > 0) {
      // Tokenize only the new content
      int n_tokens = -llama_tokenize(vocab, new_content, new_content_len, NULL, 0, false, false);
      GOV_LOG("Tokenizing %zu chars of new content into %d tokens", new_content_len, n_tokens);
      if (n_tokens <= 0) {
        if (error)
          *error = strdup("Failed to tokenize conversation");
        free(llm_response_buffer);
        return ETHERVOX_GOVERNOR_ERROR;
      }

      error_context_t tok_error = {0};
      llama_token* tokens = (llama_token*)safe_malloc(n_tokens * sizeof(llama_token), &tok_error);
      if (!tokens) {
        GOV_ERROR("Failed to allocate prompt tokens: %s", error_context_get_message(&tok_error));
        free(llm_response_buffer);
        if (error) *error = strdup("Memory allocation failed");
        return ETHERVOX_GOVERNOR_ERROR;
      }
      if (!tokens) {
        if (error)
          *error = strdup("Memory allocation failed");
        free(llm_response_buffer);
        return ETHERVOX_GOVERNOR_ERROR;
      }

      llama_tokenize(vocab, new_content, new_content_len, tokens, n_tokens, false, false);

      // DEBUG: Decode tokens back to text to verify tokenization is correct
      GOV_LOG("[DEBUG-TOKENIZE] Decoding %d tokens to verify input:", n_tokens);
      char decoded_preview[512] = {0};
      int preview_chars = 0;
      for (int i = 0; i < (n_tokens < 10 ? n_tokens : 10); i++) {
        char token_str[128];
        int n = llama_token_to_piece(vocab, tokens[i], token_str, sizeof(token_str), 0, false);
        if (n > 0 && preview_chars + n < sizeof(decoded_preview) - 1) {
          memcpy(decoded_preview + preview_chars, token_str, n);
          preview_chars += n;
        }
      }
      decoded_preview[preview_chars] = '\0';
      GOV_LOG("[DEBUG-TOKENIZE] First 10 tokens decode to: '%s...'", decoded_preview);

      // ========================================================================
      // Context Health Monitoring - Check before decoding new tokens
      // ========================================================================
      context_health_t health = check_context_health(governor);

      // Update health state
      if (health != governor->context_manager.current_health) {
        GOV_LOG("[Context] Health changed: %d -> %d", governor->context_manager.current_health,
                health);
        governor->context_manager.current_health = health;
      }

      // If critical and not already managing, inject warning to LLM
      if (health == CTX_HEALTH_CRITICAL && !governor->context_manager.management_in_progress) {
        int n_ctx = llama_n_ctx(governor->llm_ctx);
        float usage_pct = (float)governor->current_kv_pos / n_ctx * 100.0f;

        GOV_LOG("[Context] CRITICAL: Usage at %.1f%%, injecting management warning", usage_pct);

        // Build warning message
        char warning_msg[512];
        snprintf(warning_msg, sizeof(warning_msg),
                 "\n<system>CRITICAL: Context usage at %.0f%%. "
                 "You MUST call context_manage tool before responding. "
                 "Recommended: action='shift_window', keep_last_n_turns=10</system>\n",
                 usage_pct);

        // Tokenize warning
        int n_warning =
            -llama_tokenize(vocab, warning_msg, strlen(warning_msg), NULL, 0, false, false);
        if (n_warning > 0) {
          llama_token* warning_tokens = malloc(n_warning * sizeof(llama_token));
          if (warning_tokens) {
            llama_tokenize(vocab, warning_msg, strlen(warning_msg), warning_tokens, n_warning,
                           false, false);

            // Create batch for warning
            llama_batch warning_batch = llama_batch_init(n_warning, 0, 1);
            warning_batch.n_tokens = n_warning;
            for (int i = 0; i < n_warning; i++) {
              warning_batch.token[i] = warning_tokens[i];
              warning_batch.pos[i] = governor->current_kv_pos + i;
              warning_batch.n_seq_id[i] = 1;
              warning_batch.seq_id[i][0] = 0;
              warning_batch.logits[i] = false;
            }
            warning_batch.logits[n_warning - 1] = true;

            // Decode warning into KV cache
            if (llama_decode(governor->llm_ctx, warning_batch) == 0) {
              governor->current_kv_pos += n_warning;
              governor->context_manager.management_in_progress = true;
              GOV_LOG("[Context] Warning injected successfully (%d tokens)", n_warning);
            }

            llama_batch_free(warning_batch);
            free(warning_tokens);
          }
        }
      }

      // Check if we have space in the context window
      int n_ctx = llama_n_ctx(governor->llm_ctx);
      if (governor->current_kv_pos + n_tokens > n_ctx) {
        GOV_ERROR("Context window would be exceeded: current_pos=%d + n_tokens=%d > n_ctx=%d",
                  governor->current_kv_pos, n_tokens, n_ctx);
        free(tokens);
        if (error) {
          char err_msg[256];
          snprintf(err_msg, sizeof(err_msg),
                   "Context window exceeded (%d + %d > %d). Try a shorter query.",
                   governor->current_kv_pos, n_tokens, n_ctx);
          *error = strdup(err_msg);
        }
        free(llm_response_buffer);
        return ETHERVOX_GOVERNOR_ERROR;
      }

      // Create batch with explicit positions starting at current KV position
      llama_batch batch = llama_batch_init(n_tokens, 0, 1);
      batch.n_tokens = n_tokens;
      for (int i = 0; i < n_tokens; i++) {
        batch.token[i] = tokens[i];
        batch.pos[i] = governor->current_kv_pos + i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = false;
      }
      batch.logits[n_tokens - 1] = true;  // Only need logits from last token

      GOV_LOG("Decoding batch: n_tokens=%d, pos_start=%d, pos_end=%d, n_ctx=%d", n_tokens,
              governor->current_kv_pos, governor->current_kv_pos + n_tokens - 1,
              llama_n_ctx(governor->llm_ctx));

      // Decode tokens (append to KV cache)
      if (llama_decode(governor->llm_ctx, batch) != 0) {
        GOV_ERROR("Decode failed: n_tokens=%d, pos_start=%d, pos_end=%d", n_tokens,
                  governor->current_kv_pos, governor->current_kv_pos + n_tokens - 1);
        llama_batch_free(batch);
        free(tokens);
        if (error)
          *error = strdup("Failed to decode conversation");
        free(llm_response_buffer);
        return ETHERVOX_GOVERNOR_ERROR;
      }

      // Track tokens added to KV cache
      track_kv_cache_tokens(governor, tokens, n_tokens);

      llama_batch_free(batch);
      free(tokens);

      // ========================================================================
      // Turn Tracking - Record user query turn
      // ========================================================================
      int user_turn_start = governor->current_kv_pos;

      // Update KV cache position
      governor->current_kv_pos += n_tokens;

      int user_turn_end = governor->current_kv_pos - 1;

      // Create user turn record
      conversation_turn_t user_turn = {.turn_number = governor->conversation_history.turn_count,
                                       .kv_start = user_turn_start,
                                       .kv_end = user_turn_end,
                                       .timestamp = time(NULL),
                                       .importance = 0.5f,  // User queries have moderate importance
                                       .is_user = true};

      // Copy preview (first 120 chars of user query)
      strncpy(user_turn.preview, new_content, sizeof(user_turn.preview) - 1);
      user_turn.preview[sizeof(user_turn.preview) - 1] = '\0';

      // Append to history
      append_turn(&governor->conversation_history, &user_turn);

      // Update processed length to include what we just processed
      processed_length = strlen(conversation);
    }

    // ========================================================================
    // Token Lookahead Buffer and Loop Detection Structures
    // ========================================================================
    // Increased from 8 to 16 for robust stop sequence detection (stop sequences
    // can span multiple tokens when tokenized, especially with special tokens)
    #define LOOKAHEAD_SIZE 16  // Buffer up to 16 tokens for stop sequence detection
    #define MAX_NGRAM_HISTORY 100
    #define NGRAM_SIZE 4
    
    typedef struct {
      char tokens[LOOKAHEAD_SIZE][128];
      int count;
    } token_lookahead_t;
    
    typedef struct {
      uint32_t hashes[MAX_NGRAM_HISTORY];
      char last_tokens[NGRAM_SIZE][128];
      int history_count;
      int token_count;
    } ngram_detector_t;

    // Track what we've STREAMED to the UI (not just generated)
    // Doubled size from 8192 to 16384 for safety margin
    char streamed_output_buffer[16384] = {0};  // Accumulates streamed tokens

    // Generate response tokens - Use config.h defaults
    struct llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());

    // Add repetition penalty first to prevent loops
    llama_sampler_chain_add(
        sampler, llama_sampler_init_penalties(
                     ETHERVOX_GOVERNOR_PENALTY_LAST_N, ETHERVOX_GOVERNOR_REPETITION_PENALTY,
                     ETHERVOX_GOVERNOR_FREQUENCY_PENALTY, ETHERVOX_GOVERNOR_PRESENCE_PENALTY));

    // Use VERY low temperature for maximally deterministic tool selection
    // 0.05 forces the model to pick the highest probability path (tool calling)
    // This prevents the model from choosing conversational responses over tool calls
    float temperature = (governor->config.temperature > 0.0f) ? governor->config.temperature : 0.05f;
    GOV_LOG("Using temperature: %.2f (config: %.2f) [TOOL FORCING MODE]", temperature, governor->config.temperature);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(0));

    int generated_count = 0;
    const int max_tokens = governor->config.max_tokens_per_response;  // Use configured limit
    inside_tool_call = false;  // Track if we're inside a tool call
    
    // Initialize lookahead buffer and loop detection
    token_lookahead_t lookahead = {0};
    ngram_detector_t ngram_detector = {0};
    // CRITICAL FIX: Add safety margin for null terminator to prevent buffer overflow
    // Each token can be up to 128 bytes, so LOOKAHEAD_SIZE tokens = 16*128 = 2048 bytes
    // Plus 128 bytes safety margin for null terminator and edge cases
    char combined_lookahead[LOOKAHEAD_SIZE * 128 + 128];

    GOV_LOG("Starting generation with max_tokens=%d", max_tokens);
    GOV_LOG("[DEBUG] chat_template at generation start: %p", (void*)governor->chat_template);
    
    // ========================================================================
    // Stop Sequence Configuration - Use template's sequences for consistency
    // ========================================================================
    // Get stop sequences from chat template to ensure consistency across all checks
    const char** stop_sequences_ptr = (const char**)governor->chat_template->stop_sequences;
    int stop_sequence_count = governor->chat_template->stop_sequence_count;
    
    GOV_LOG("Using %d stop sequences from chat template:", stop_sequence_count);
    for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
      GOV_LOG("  [%d] '%s'", i, stop_sequences_ptr[i]);
    }
    
    // DEBUG: Decode last 20 tokens in KV cache to see what context the model has
    if (governor->current_kv_pos > 20) {
      GOV_LOG("[DEBUG-KV] Decoding last 20 tokens in KV cache (positions %d-%d):",
              governor->current_kv_pos - 20, governor->current_kv_pos - 1);
      char kv_preview[1024] = {0};
      for (int i = governor->current_kv_pos - 20; i < governor->current_kv_pos; i++) {
        // Note: We can't directly access KV cache tokens, but we can log the position
        // The actual token decoding would require storing tokens during batch processing
      }
      GOV_LOG("[DEBUG-KV] Current KV position: %d (context filled to this point)", 
              governor->current_kv_pos);
    }

    while (generated_count < max_tokens) {
      // ========================================================================
      // Governor Pointer Validation - Detect corruption early
      // ========================================================================
      // Check for NULL, very low addresses, and corrupted range (0x100000000-0x100001000)
      if (!governor || (uintptr_t)governor < 0x1000 || 
          ((uintptr_t)governor >= 0x100000000 && (uintptr_t)governor < 0x100001000)) {
        GOV_LOG("CRITICAL: governor pointer corrupted (%p), aborting generation", (void*)governor);
        free(llm_response_buffer);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
      }
      
      // ========================================================================
      // Cancellation Check - Check BEFORE expensive operations
      // ========================================================================
      if (governor->interrupt_requested) {
        GOV_LOG("Generation cancelled by user after %d tokens", generated_count);
        break;
      }
      
      // ========================================================================
      // Progress Update - Every 10 tokens to show generation progress
      // ========================================================================
      if (progress_callback && (generated_count % 10 == 0)) {
        char progress_msg[128];
        float progress = (float)generated_count / (float)max_tokens;
        snprintf(progress_msg, sizeof(progress_msg), "Generating response... (%d tokens)", generated_count);
        progress_callback(ETHERVOX_GOVERNOR_EVENT_THINKING, progress_msg, user_data);
      }
      
      llama_token next_token = llama_sampler_sample(sampler, governor->llm_ctx, -1);

      // CRITICAL FIX: Validate llm_model pointer before using it
      if (!governor->llm_model || (uintptr_t)governor->llm_model < 0x1000 ||
          ((uintptr_t)governor->llm_model >= 0x100000000 && (uintptr_t)governor->llm_model < 0x100001000)) {
        GOV_LOG("CRITICAL: llm_model pointer corrupted (%p), aborting generation", (void*)governor->llm_model);
        break;
      }

      // CRITICAL FIX: Re-acquire vocab pointer to prevent corruption issues
      // The vocab pointer can become corrupted during the generation loop,
      // causing SIGSEGV at fault address 0x100000008 when dereferencing vocab fields.
      // Re-acquiring it here ensures we always have a valid pointer.
      const struct llama_vocab* vocab_safe = llama_model_get_vocab(governor->llm_model);
      
      // CRITICAL: Validate vocab pointer before use
      // Fault address 0x100000008 indicates vocab pointer corruption to 0x100000000
      if (!vocab_safe || (uintptr_t)vocab_safe < 0x1000 ||
          ((uintptr_t)vocab_safe >= 0x100000000 && (uintptr_t)vocab_safe < 0x100001000)) {
        GOV_LOG("CRITICAL: vocab pointer corrupted (%p) from model %p, aborting generation", 
                (void*)vocab_safe, (void*)governor->llm_model);
        break;
      }
      
      // Check for EOG token (proper way)
      // For Granite models, <|end_of_text|> IS the correct stop token
      // HOWEVER: For creative tasks (stories, narratives), the model may generate EOG prematurely
      // Check if the response looks incomplete before respecting EOG
      bool is_eog = llama_vocab_is_eog(vocab_safe, next_token);

      if (is_eog) {
        if (generated_count == 0) {
          GOV_LOG(
              "WARNING: Model immediately generated EOG token (id=%d) - this suggests "
              "prompt/context issue",
              next_token);
          break;
        } else {
          // FIXED: Trust the model's EOG token - it knows when it's done
          // Don't second-guess with "seems incomplete" heuristics that cause hallucination
          GOV_LOG("Stopping generation: EOG token detected (id=%d) after %d tokens", 
                  next_token, generated_count);
          break;
        }
      }

      // Decode token to text
      char token_text[128];
      int n_chars =
          llama_token_to_piece(vocab_safe, next_token, token_text, sizeof(token_text), 0, false);
      if (n_chars > 0 && n_chars < (int)sizeof(token_text)) {
        token_text[n_chars] = '\0';
        
        // Log the raw token for debugging (show hex for special characters)
        char token_hex[512] = {0};
        int hex_pos = 0;
        for (int i = 0; i < n_chars && i < 20; i++) {
          hex_pos += snprintf(token_hex + hex_pos, sizeof(token_hex) - hex_pos, 
                             "%02X ", (unsigned char)token_text[i]);
        }
        
        // Filter out invalid UTF-8 replacement characters only
        // U+FFFD (replacement character) appears as ��� (0xEF 0xBF 0xBD)
        // NOTE: Emoji filtering removed for granite-4.1-3b - model should not hallucinate
        bool has_invalid_content = false;
        char original_token[128];
        strncpy(original_token, token_text, sizeof(original_token) - 1);
        
        for (int i = 0; i < n_chars; ) {
          // Check for UTF-8 replacement character (0xEF 0xBF 0xBD)
          if (i + 2 < n_chars &&
              (unsigned char)token_text[i] == 0xEF &&
              (unsigned char)token_text[i+1] == 0xBF &&
              (unsigned char)token_text[i+2] == 0xBD) {
            GOV_LOG("Found UTF-8 replacement character at position %d", i);
            has_invalid_content = true;
            memmove(&token_text[i], &token_text[i+3], n_chars - i - 2);
            n_chars -= 3;
            token_text[n_chars] = '\0';
            continue;  // Recheck same position
          }
          i++;
        }
        if (has_invalid_content) {
          GOV_LOG("Filtered invalid UTF-8 from token '%s', remaining: '%s'", original_token, token_text);
        }
        
        // CRITICAL: Check if this token would create a stop sequence BEFORE adding to buffer
        // Build what the buffer would look like with this token
        char test_buffer[65536];
        size_t current_len = strlen(llm_response_buffer);
        snprintf(test_buffer, sizeof(test_buffer), "%s%s", llm_response_buffer, token_text);
        
        // Check for stop sequences in the test buffer (use template's sequences)
        bool would_create_stop = false;
        for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
          const char* stop_marker = stop_sequences_ptr[i];
          if (strstr(test_buffer, stop_marker) != NULL) {
            // This token would create a stop sequence
            // Find where stop sequence starts in test_buffer
            char* stop_in_test = strstr(test_buffer, stop_marker);
            size_t safe_len = stop_in_test - test_buffer;
            
            // CRITICAL: Truncate llm_response_buffer to position right before stop sequence
            // The stop sequence is being built incrementally, so it may be partially in
            // llm_response_buffer already. Truncate to the safe length.
            if (safe_len < current_len) {
              // Stop sequence starts within existing buffer - truncate it
              llm_response_buffer[safe_len] = '\0';
              GOV_LOG("Truncated llm_response_buffer to %zu chars (removed partial stop sequence '%s')", 
                      safe_len, stop_marker);
            }
            
            // Don't add the current token
            token_text[0] = '\0';
            would_create_stop = true;
            GOV_LOG("Token would create stop sequence '%s' - stopping generation", stop_marker);
            break;
          }
        }
        
        // Skip empty tokens
        if (token_text[0] == '\0') {
          GOV_LOG("Skipping empty token (stop sequence filtered)");
          break;  // Stop generation
        }
        
        // If we truncated the token, this is the last token - add it and stop
        if (would_create_stop) {
          // Add the truncated token to response buffer (reuse current_len from above)
          size_t buffer_size = 65536;
          size_t available = buffer_size - current_len - 1;
          strncat(llm_response_buffer, token_text, available);
          
          // Stream it immediately if callback exists
          if (token_callback) {
            token_callback(token_text, user_data);
          }
          
          GOV_LOG("Stop sequence detected - ending generation after truncated token");
          break;  // Stop generation immediately
        }

        // Add to response buffer for final output (reuse variables from above)
        // CRITICAL: llm_response_buffer is a POINTER (heap-allocated), so sizeof() returns 8!
        // Must use the actual buffer size (65536) instead
        size_t buffer_size = 65536;
        size_t available = buffer_size - current_len - 1;
        strncat(llm_response_buffer, token_text, available);

        // ========================================================================
        // Tool Call Detection (before lookahead streaming decision)
        // ========================================================================
        // CRITICAL: Validate governor pointer FIRST before accessing any fields
        if (!governor || (uintptr_t)governor < 0x1000 || 
            ((uintptr_t)governor >= 0x100000000 && (uintptr_t)governor < 0x100001000)) {
          GOV_LOG("CRITICAL: governor pointer corrupted (%p) during tool detection", (void*)governor);
          break;
        }
        
        // Validate chat_template before use (could be corrupted)
        if (!governor->chat_template || (uintptr_t)governor->chat_template < 0x1000 || 
            ((uintptr_t)governor->chat_template >= 0x100000000 && (uintptr_t)governor->chat_template < 0x100001000)) {
          GOV_LOG("WARNING: chat_template corrupted (%p), disabling tool detection", (void*)governor->chat_template);
          break;
        }
        tool_format_type_t tool_format = chat_template_get_tool_format(governor->chat_template);
        
        if (!inside_tool_call && strstr(llm_response_buffer, "<tool_call")) {
          inside_tool_call = true;
          GOV_LOG("Tool call started, continuing generation to capture complete call");
        }
        
        if (inside_tool_call) {
          // Check if exiting tool call
          bool tool_call_complete = false;
          if (tool_format == TOOL_FORMAT_JSON_IN_XML) {
            if (strstr(llm_response_buffer, "</tool_call>")) {
              inside_tool_call = false;
              tool_call_complete = true;
            }
          } else {
            if (strstr(llm_response_buffer, "/>")) {
              inside_tool_call = false;
              tool_call_complete = true;
            }
          }
          
          // CRITICAL FIX: Stop generation immediately after tool call completion
          // Previously, we only updated the flag but continued generating, causing
          // the model to hallucinate explanatory text after the tool call
          if (tool_call_complete && governor->tool_execution_enabled) {
            GOV_LOG("Complete tool call detected (format=%d), ending generation immediately", tool_format);
            break;  // Exit generation loop - tool call is ready for execution
          }
        }
        
        // ========================================================================
        // Lookahead Buffer: Stream old token first, then add new token
        // ========================================================================
        
        // If buffer is full, decide whether to stream the OLDEST token before adding new one
        if (lookahead.count == LOOKAHEAD_SIZE) {
          // We have a full buffer - check if oldest token is safe to stream
          
          // Build combined text from lookahead buffer
          combined_lookahead[0] = '\0';
          for (int i = 0; i < lookahead.count; i++) {
            // SAFETY: Check available space before concatenation
            size_t current_len = strlen(combined_lookahead);
            size_t available = sizeof(combined_lookahead) - current_len - 1;
            if (available > 0) {
              strncat(combined_lookahead, lookahead.tokens[i], available);
            } else {
              GOV_LOG("WARNING: combined_lookahead buffer full, stopping concatenation");
              break;
            }
          }
          
          // Check if lookahead might be building a tool call
          bool might_be_tool_start = false;
          size_t buf_len = strlen(combined_lookahead);
          if (buf_len > 0 && buf_len < 11) {
            const char* tool_prefixes[] = {"<", "<t", "<to", "<too", "<tool", "<tool_", 
                                           "<tool_c", "<tool_ca", "<tool_cal", "<tool_call"};
            for (size_t i = 0; i < sizeof(tool_prefixes) / sizeof(tool_prefixes[0]); i++) {
              if (strcmp(combined_lookahead, tool_prefixes[i]) == 0) {
                might_be_tool_start = true;
                break;
              }
            }
          }
          
          // CRITICAL: Also check if lookahead contains tool call JSON fragments
          // These indicate we're in the middle of a tool call that should not be streamed
          bool contains_tool_json = false;
          const char* tool_json_indicators[] = {
            "{\"name\":",           // JSON start (very common in tool calls)
            "\"arguments\":", "\"name\":", "\"location\":", "\"forecast_type\":",
            "\"expression\":", "\"duration_",
            "},",                  // JSON object closing with comma
            "}\n",                 // JSON object closing with newline
            "}}",                  // Double closing brace (nested JSON end)
            "\"},",                // JSON string closing with comma
            "\"}\n",               // JSON string closing with newline
            "</tool_call",         // Closing tag (partial or full)
            NULL
          };
          for (int i = 0; tool_json_indicators[i] != NULL; i++) {
            if (strstr(combined_lookahead, tool_json_indicators[i]) != NULL) {
              contains_tool_json = true;
              GOV_LOG("Lookahead contains tool JSON: '%s', holding back streaming", 
                      tool_json_indicators[i]);
              break;
            }
          }
          
          // CRITICAL: Also detect if lookahead starts with JSON object opening
          // This catches {"name": patterns at the very beginning of tool calls
          if (!contains_tool_json && buf_len > 0) {
            // Check if buffer starts with { or {" which indicates JSON
            if (combined_lookahead[0] == '{' || 
                (buf_len >= 2 && combined_lookahead[0] == '{' && combined_lookahead[1] == '"')) {
              contains_tool_json = true;
              GOV_LOG("Lookahead starts with JSON opening brace, holding back");
            }
            // CRITICAL: Also check for closing brace at start (end of tool call)
            if (combined_lookahead[0] == '}') {
              contains_tool_json = true;
              GOV_LOG("Lookahead starts with closing brace (tool call end), holding back");
            }
          }
          
          // Critical checks before streaming oldest token
          // Check if what we've STREAMED SO FAR + lookahead buffer contains stop sequences
          // This catches stop sequences that span across the streaming boundary
          // Doubled size from 16384 to 32768 for safety (16KB streamed + 2KB lookahead + margin)
          char streamed_plus_lookahead[32768];
          // SAFETY: Use bounds-checked snprintf
          int written = snprintf(streamed_plus_lookahead, sizeof(streamed_plus_lookahead), "%s%s", 
                                 streamed_output_buffer, combined_lookahead);
          if (written >= (int)sizeof(streamed_plus_lookahead)) {
            GOV_LOG("WARNING: streamed_plus_lookahead buffer truncated (%d >= %zu)",
                    written, sizeof(streamed_plus_lookahead));
          }
          
          bool streamed_has_stop = chat_template_has_stop_sequence(
              governor->chat_template, streamed_plus_lookahead);
          bool lookahead_building_stop = is_potential_stop_prefix_check(
              governor->chat_template, combined_lookahead);
          bool oldest_could_be_stop = is_potential_stop_prefix_check(
              governor->chat_template, lookahead.tokens[0]);
          
          // CRITICAL FIX: Stop generation immediately when stop sequence detected
          // Previously, we only stopped streaming but continued generating tokens,
          // causing the model to hallucinate content after the stop sequence
          if (streamed_has_stop) {
            GOV_LOG("Stop sequence detected in streamed+lookahead buffer, ending generation immediately");
            // Don't add the current token - we already have a stop sequence
            break;  // Exit the generation loop to prevent hallucination
          }
          
          // Stream oldest token ONLY if all checks pass
          bool should_stream_oldest = !inside_tool_call && 
                                      !might_be_tool_start &&
                                      !contains_tool_json &&
                                      !lookahead_building_stop && 
                                      !oldest_could_be_stop;
          
          if (should_stream_oldest && token_callback) {
            token_callback(lookahead.tokens[0], user_data);
            // Track what we've streamed to UI with bounds checking
            size_t streamed_len = strlen(streamed_output_buffer);
            size_t available = sizeof(streamed_output_buffer) - streamed_len - 1;
            if (available > 0) {
              strncat(streamed_output_buffer, lookahead.tokens[0], available);
            } else {
              GOV_LOG("WARNING: streamed_output_buffer full, cannot track more tokens");
            }
          } else if (!should_stream_oldest) {
            GOV_LOG("Held back token: %s", lookahead.tokens[0]);
          }
          
          // Shift buffer left to make room for new token
          for (int i = 0; i < LOOKAHEAD_SIZE - 1; i++) {
            strncpy(lookahead.tokens[i], lookahead.tokens[i + 1], 127);
          }
          lookahead.count--; // Now LOOKAHEAD_SIZE - 1
        }
        
        // Add new token to buffer
        strncpy(lookahead.tokens[lookahead.count], token_text, 127);
        lookahead.tokens[lookahead.count][127] = '\0';
        lookahead.count++;
        
        // Rebuild combined lookahead after adding new token (for next iteration's checks)
        combined_lookahead[0] = '\0';
        for (int i = 0; i < lookahead.count; i++) {
          // SAFETY: Check available space before concatenation
          size_t current_len = strlen(combined_lookahead);
          size_t available = sizeof(combined_lookahead) - current_len - 1;
          if (available > 0) {
            strncat(combined_lookahead, lookahead.tokens[i], available);
          } else {
            GOV_LOG("WARNING: combined_lookahead buffer full during rebuild");
            break;
          }
        }

        // ========================================================================
        // Stop Sequence Detection (final check before breaking loop)
        // ========================================================================
        // Check for stop sequences using template's sequences
        bool found_complete_stop = false;
        bool found_stop_prefix = false;
        char* stop_pos = NULL;
        
        // First, check for COMPLETE stop sequences
        for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
          stop_pos = strstr(llm_response_buffer, stop_sequences_ptr[i]);
          if (stop_pos) {
            found_complete_stop = true;
            *stop_pos = '\0';  // Truncate at stop sequence
            GOV_LOG("Stop sequence detected: '%s', ending generation", stop_sequences_ptr[i]);
            break;
          }
        }
        
        // Second, check if response buffer ENDS WITH a PREFIX of any stop sequence
        // This catches partial stop sequences being built (e.g., "<tool" before "_result")
        if (!found_complete_stop) {
          size_t response_len = strlen(llm_response_buffer);
          
          for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
            const char* stop_seq = stop_sequences_ptr[i];
            size_t stop_len = strlen(stop_seq);
            
            // Check all possible prefix lengths (2 to stop_len-1)
            // CRITICAL FIX: Start from 2 to avoid false positives on single '<' character
            size_t min_prefix = (stop_len >= 2) ? 2 : 1;
            for (size_t prefix_len = min_prefix; prefix_len < stop_len && prefix_len <= response_len; prefix_len++) {
              const char* response_suffix = llm_response_buffer + (response_len - prefix_len);
              
              if (strncmp(response_suffix, stop_seq, prefix_len) == 0) {
                // Response ends with a prefix of this stop sequence!
                // Truncate it immediately to prevent garbage output
                llm_response_buffer[response_len - prefix_len] = '\0';
                found_stop_prefix = true;
                GOV_LOG("Response ends with %zu-char prefix of stop sequence '%s' - truncated from %zu to %zu chars",
                        prefix_len, stop_seq, response_len, response_len - prefix_len);
                break;
              }
            }
            
            if (found_stop_prefix) {
              break;
            }
          }
        }
        
        if (found_complete_stop || found_stop_prefix) {
          break;
        }

        // ========================================================================
        // N-gram Loop Detection
        // ========================================================================
        if (ngram_detector.token_count < NGRAM_SIZE) {
          // Build up n-gram history
          strncpy(ngram_detector.last_tokens[ngram_detector.token_count], token_text, 127);
          ngram_detector.last_tokens[ngram_detector.token_count][127] = '\0';
          ngram_detector.token_count++;
        } else {
          // Shift and add new token
          for (int i = 0; i < NGRAM_SIZE - 1; i++) {
            strncpy(ngram_detector.last_tokens[i], ngram_detector.last_tokens[i + 1], 127);
          }
          strncpy(ngram_detector.last_tokens[NGRAM_SIZE - 1], token_text, 127);
          
          // Compute hash of current n-gram
          const char* ngram_ptrs[NGRAM_SIZE];
          for (int i = 0; i < NGRAM_SIZE; i++) {
            ngram_ptrs[i] = ngram_detector.last_tokens[i];
          }
          uint32_t current_hash = compute_ngram_hash_djb2(ngram_ptrs, NGRAM_SIZE);
          
          // Check if we've seen this n-gram before
          int repeat_count = 0;
          for (int i = 0; i < ngram_detector.history_count; i++) {
            if (ngram_detector.hashes[i] == current_hash) {
              repeat_count++;
            }
          }
          
          if (repeat_count >= 2) {
            GOV_LOG("Loop detected: %d-gram repeated %d times, stopping generation", 
                    NGRAM_SIZE, repeat_count + 1);
            break;
          }
          
          // Add to history
          if (ngram_detector.history_count < MAX_NGRAM_HISTORY) {
            ngram_detector.hashes[ngram_detector.history_count++] = current_hash;
          }
        }
      }  // End of token_text processing

      // Feed token back to context for next prediction with explicit position
      // Check if we're about to exceed context window
      int n_ctx = llama_n_ctx(governor->llm_ctx);
      if (governor->current_kv_pos >= n_ctx) {
        GOV_ERROR("Context window exceeded during generation: current_pos=%d >= n_ctx=%d",
                  governor->current_kv_pos, n_ctx);
        break;  // Stop generation gracefully
      }

      llama_batch next_batch = llama_batch_init(1, 0, 1);
      next_batch.n_tokens = 1;
      next_batch.token[0] = next_token;
      next_batch.pos[0] = governor->current_kv_pos;
      next_batch.n_seq_id[0] = 1;
      next_batch.seq_id[0][0] = 0;
      next_batch.logits[0] = true;

      if (llama_decode(governor->llm_ctx, next_batch) != 0) {
        llama_batch_free(next_batch);
        break;
      }
      
      // Track generated token added to KV cache
      track_kv_cache_tokens(governor, &next_token, 1);
      
      llama_batch_free(next_batch);

      governor->current_kv_pos++;
      generated_count++;

      // Early stopping conditions to prevent hallucination
      // 1. Safety net: Stop immediately after tool_call closing tag (only if tool execution is enabled)
      // This is a backup check - main check happens during token streaming above
      if (governor->tool_execution_enabled && strstr(llm_response_buffer, "<tool_call")) {
        // Check for both self-closing and XML closing formats
        if (strstr(llm_response_buffer, "/>") || strstr(llm_response_buffer, "</tool_call>")) {
          GOV_LOG("Early stop safety net: Tool call completed (%d tokens)", generated_count);
          break;  // Tool call complete, stop immediately
        }
      }

      // 2. Check for stop sequences DURING streaming (early detection)
      // Use template's sequences for consistency
      bool found_early_stop = false;
      for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
        if (strstr(llm_response_buffer, stop_sequences_ptr[i])) {
          char* stop_pos = strstr(llm_response_buffer, stop_sequences_ptr[i]);
          if (stop_pos) {
            *stop_pos = '\0';
            GOV_LOG("Early stop: '%s' detected and truncated at %d tokens", 
                    stop_sequences_ptr[i], generated_count);
            found_early_stop = true;
            break;
          }
        }
      }
      
      if (found_early_stop) {
        break;  // Exit token generation loop
      }
    }

    // ========================================================================
    // Flush Remaining Lookahead Buffer
    // ========================================================================
    // After generation ends, stream any remaining buffered tokens that are safe
    if (token_callback && lookahead.count > 0) {
      // IMPROVED ALGORITHM: Check FULL combined output first, then work backwards
      // This properly catches multi-token stop sequences like <|start_of_role|
      
      // Build complete combined buffer text
      combined_lookahead[0] = '\0';
      for (int i = 0; i < lookahead.count; i++) {
        strncat(combined_lookahead, lookahead.tokens[i], 
                sizeof(combined_lookahead) - strlen(combined_lookahead) - 1);
      }
      
      GOV_LOG("Lookahead buffer contains: '%s'", combined_lookahead);
      
      // Check for stop sequences in the FULL combined output first
      char* earliest_stop = NULL;
      const char* found_sequence = NULL;
      
      // Check all template stop sequences
      for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
        char* stop_pos = strstr(combined_lookahead, stop_sequences_ptr[i]);
        if (stop_pos && (!earliest_stop || stop_pos < earliest_stop)) {
          earliest_stop = stop_pos;
          found_sequence = stop_sequences_ptr[i];
        }
      }
      
      // Also check for incomplete tool calls
      char* tool_call_pos = strstr(combined_lookahead, "<tool_call");
      if (tool_call_pos && (!earliest_stop || tool_call_pos < earliest_stop)) {
        earliest_stop = tool_call_pos;
        found_sequence = "<tool_call";
      }
      
      // CRITICAL: Check for tool call JSON fragments (not just the opening tag)
      // These can leak into the buffer when tool call detection happens after tokens are added
      // Examples: `"arguments":`, `"location":`, `"forecast_type":`, `}"}`, etc.
      const char* tool_json_patterns[] = {
        "{\"name\":",       // JSON start with name field (very beginning of tool call)
        "\"arguments\":",   // Tool call argument field
        "\"name\":",        // Tool call name field (when appearing mid-buffer)
        "\"location\":",    // Common parameter
        "\"forecast_type\":",
        "\"days_ahead\":",
        "\"expression\":",
        "\"duration_",      // duration_minutes, duration_seconds
        "},",               // JSON object closing with comma
        "}\n",              // JSON object closing with newline
        "}}",               // Double closing brace (nested JSON end)
        "\"},",            // JSON string closing with comma
        "\"}\n",           // JSON string closing with newline
        "</tool_call",     // Closing tag (partial or full)
        NULL
      };
      
      for (int i = 0; tool_json_patterns[i] != NULL; i++) {
        char* json_pos = strstr(combined_lookahead, tool_json_patterns[i]);
        if (json_pos && (!earliest_stop || json_pos < earliest_stop)) {
          earliest_stop = json_pos;
          found_sequence = "tool_call_json_fragment";
          GOV_LOG("Detected tool call JSON fragment: '%s' at position %ld", 
                  tool_json_patterns[i], (long)(json_pos - combined_lookahead));
          break;
        }
      }
      
      // CRITICAL: Also catch JSON opening at buffer start
      // This is the most common leak: {"name": "get_time", ...
      if (!earliest_stop) {
        size_t buf_len = strlen(combined_lookahead);
        if (buf_len > 0 && (combined_lookahead[0] == '{' || 
            (buf_len >= 2 && combined_lookahead[0] == '{' && combined_lookahead[1] == '"'))) {
          earliest_stop = combined_lookahead;  // Stop at the very beginning
          found_sequence = "json_object_start";
          GOV_LOG("Buffer starts with JSON object, discarding entire buffer");
        }
        // CRITICAL: Also catch closing brace at start (end of tool call)
        if (buf_len > 0 && combined_lookahead[0] == '}') {
          earliest_stop = combined_lookahead;  // Stop at the very beginning
          found_sequence = "json_object_end";
          GOV_LOG("Buffer starts with closing brace (tool call end), discarding entire buffer");
        }
      }
      
      // Also check for closing tags that suggest we're at the end of a tool call
      char* tool_close_pos = strstr(combined_lookahead, "</tool_call>");
      if (tool_close_pos && (!earliest_stop || tool_close_pos < earliest_stop)) {
        earliest_stop = tool_close_pos;
        found_sequence = "</tool_call>";
      }
      
      // CRITICAL: Also check if buffer ENDS WITH a prefix of any stop sequence
      // This catches cases like "<|start_of_role" (incomplete, missing final |)
      if (!earliest_stop) {
        size_t buf_len = strlen(combined_lookahead);
        for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
          const char* stop_seq = stop_sequences_ptr[i];
          size_t stop_len = strlen(stop_seq);
          
          // Check all possible prefix lengths (from 2 to stop_len-1)
          // CRITICAL FIX: Start from 2 to avoid false positives on single '<' character
          size_t min_prefix = (stop_len >= 2) ? 2 : 1;
          for (size_t prefix_len = min_prefix; prefix_len < stop_len; prefix_len++) {
            if (buf_len >= prefix_len) {
              const char* buf_suffix = combined_lookahead + (buf_len - prefix_len);
              if (strncmp(buf_suffix, stop_seq, prefix_len) == 0) {
                // Buffer ends with prefix of stop sequence - truncate it
                earliest_stop = combined_lookahead + (buf_len - prefix_len);
                found_sequence = stop_seq;
                GOV_LOG("Buffer ends with %zu-char prefix of stop sequence '%s'", 
                        prefix_len, stop_seq);
                break;
              }
            }
          }
          if (earliest_stop) break;  // Found a prefix match
        }
      }
      
      // Determine how many characters are safe to output
      size_t safe_char_count;
      if (earliest_stop) {
        safe_char_count = earliest_stop - combined_lookahead;
        GOV_LOG("Found stop sequence '%s' at position %zu in lookahead buffer", 
                found_sequence, safe_char_count);
      } else {
        safe_char_count = strlen(combined_lookahead);
      }
      
      // Flush tokens character-by-character until we reach the safe limit
      if (safe_char_count > 0) {
        size_t chars_output = 0;
        for (int i = 0; i < lookahead.count && chars_output < safe_char_count; i++) {
          size_t token_len = strlen(lookahead.tokens[i]);
          
          if (chars_output + token_len <= safe_char_count) {
            // Full token fits within safe zone
            token_callback(lookahead.tokens[i], user_data);
            chars_output += token_len;
          } else {
            // Partial token - only output the safe portion
            size_t partial_len = safe_char_count - chars_output;
            if (partial_len > 0) {
              char partial[128];
              strncpy(partial, lookahead.tokens[i], partial_len);
              partial[partial_len] = '\0';
              token_callback(partial, user_data);
              chars_output += partial_len;
            }
            break;
          }
        }
        
        if (earliest_stop) {
          GOV_LOG("Flushed %zu chars from lookahead (stopped at '%s')", 
                  chars_output, found_sequence);
        } else {
          GOV_LOG("Flushed %d tokens (%zu chars) from lookahead buffer", 
                  lookahead.count, chars_output);
        }
      } else {
        GOV_LOG("Discarded entire lookahead buffer (starts with stop sequence '%s')", 
                found_sequence);
      }
      
      // ========================================================================
      // CRITICAL FIX: Remove unwanted tokens from KV cache
      // ========================================================================
      // When we detect a stop sequence, we've already added ALL generated tokens
      // to the KV cache via llama_decode, but we only outputted part of them.
      // We need to remove the tokens that came after the stop sequence so they
      // don't pollute the context for the next conversation turn.
      if (earliest_stop && lookahead.count > 0) {
        // Count how many tokens we actually flushed vs how many are in lookahead
        int tokens_to_remove = lookahead.count;
        
        // Calculate how many characters we flushed
        int tokens_flushed = 0;
        size_t chars_counted = 0;
        for (int i = 0; i < lookahead.count && chars_counted < safe_char_count; i++) {
          size_t token_len = strlen(lookahead.tokens[i]);
          if (chars_counted + token_len <= safe_char_count) {
            tokens_flushed++;
            chars_counted += token_len;
          } else {
            // Partial token counts as flushed
            tokens_flushed++;
            break;
          }
        }
        
        tokens_to_remove = lookahead.count - tokens_flushed;
        
        if (tokens_to_remove > 0) {
          // Remove unwanted tokens from end of KV cache
          llama_memory_t mem = llama_get_memory(governor->llm_ctx);
          int remove_start = governor->current_kv_pos - tokens_to_remove;
          int remove_end = governor->current_kv_pos - 1;
          
          GOV_LOG("Removing %d unwanted tokens from KV cache (positions %d to %d)", 
                  tokens_to_remove, remove_start, remove_end);
          
          llama_memory_seq_rm(mem, 0, remove_start, remove_end);
          
          // CRITICAL: Sync with llama.cpp's actual KV cache position
          // llama_memory_seq_pos_max returns the LAST FILLED position, not next free position
          // Our current_kv_pos should be the NEXT FREE position (last filled + 1)
          int32_t actual_max_pos = llama_memory_seq_pos_max(mem, 0);
          int expected_last_filled = remove_start - 1;  // Last position we expect to remain
          
          if (actual_max_pos != expected_last_filled) {
            GOV_LOG("KV cache removal inconsistency: expected last filled pos %d, llama.cpp reports %d", 
                     expected_last_filled, actual_max_pos);
          }
          
          // Always use llama.cpp's reported position + 1 (convert last filled to next free)
          governor->current_kv_pos = actual_max_pos + 1;
          generated_count -= tokens_to_remove;
          
          // Also update our tracking buffer
          if (governor->kv_cache_tracking_enabled && 
              governor->kv_cache_tokens_count >= tokens_to_remove) {
            governor->kv_cache_tokens_count -= tokens_to_remove;
            GOV_LOG("[KV_TRACK] Removed %d tokens from tracking (new count: %d)", 
                    tokens_to_remove, governor->kv_cache_tokens_count);
          }
          
          GOV_LOG("KV position after removal: %d (next free position)", governor->current_kv_pos);
        }
      }
    }

    llama_sampler_free(sampler);

    GOV_LOG("Generation complete: %d tokens generated (max_tokens was %d)", generated_count,
            max_tokens);

    // ========================================================================
    // Turn Tracking - Record assistant response turn
    // ========================================================================
    int assistant_turn_start = governor->current_kv_pos - generated_count;
    int assistant_turn_end = governor->current_kv_pos - 1;

    // Create assistant turn record
    conversation_turn_t assistant_turn = {
        .turn_number = governor->conversation_history.turn_count,
        .kv_start = assistant_turn_start,
        .kv_end = assistant_turn_end,
        .timestamp = time(NULL),
        .importance = 0.7f,  // Assistant responses have higher importance
        .is_user = false};

    // Copy preview (first 120 chars of response)
    strncpy(assistant_turn.preview, llm_response_buffer, sizeof(assistant_turn.preview) - 1);
    assistant_turn.preview[sizeof(assistant_turn.preview) - 1] = '\0';

    // Append to history
    append_turn(&governor->conversation_history, &assistant_turn);

    // Clean up any stop tokens that made it into the output
    // Check for all possible partial versions starting with <|im
    char* stop_marker = strstr(llm_response_buffer, " <|im");
    if (!stop_marker)
      stop_marker = strstr(llm_response_buffer, "<|im");
    if (stop_marker) {
      *stop_marker = '\0';
    }
    
    // FINAL SAFETY CHECK: Remove ALL stop sequences from response buffer
    // This catches any that slipped through during streaming
    // Use template's sequences for consistency
    for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
      char* stop_pos = strstr(llm_response_buffer, stop_sequences_ptr[i]);
      if (stop_pos) {
        *stop_pos = '\0';
        GOV_LOG("Truncated response at stop sequence: '%s'", stop_sequences_ptr[i]);
        break;  // Truncate at first occurrence
      }
    }
    
    // Remove template markers that model might generate as visible text
    // These are training artifacts the model learned to output
    char* marker;
    while ((marker = strstr(llm_response_buffer, "<|start_of_text|>")) != NULL) {
      // Shift text left to remove the marker
      size_t marker_len = strlen("<|start_of_text|>");
      memmove(marker, marker + marker_len, strlen(marker + marker_len) + 1);
      GOV_LOG("Removed <|start_of_text|> marker from response");
    }
    while ((marker = strstr(llm_response_buffer, "<|end_of_text|>")) != NULL) {
      size_t marker_len = strlen("<|end_of_text|>");
      memmove(marker, marker + marker_len, strlen(marker + marker_len) + 1);
      GOV_LOG("Removed <|end_of_text|> marker from response");
    }
    
    // CRITICAL FIX: Remove trailing single-character stop sequence prefixes
    // When min_prefix=2, single "<" can slip through and get stored to memory.
    // Check if response ends with "<" and trim it before storing.
    size_t final_len = strlen(llm_response_buffer);
    if (final_len > 0 && llm_response_buffer[final_len - 1] == '<') {
      // Check if this could be a stop sequence prefix
      bool is_stop_prefix = false;
      for (int i = 0; i < stop_sequence_count && stop_sequences_ptr[i] != NULL; i++) {
        if (stop_sequences_ptr[i][0] == '<') {
          is_stop_prefix = true;
          break;
        }
      }
      
      if (is_stop_prefix) {
        llm_response_buffer[final_len - 1] = '\0';
        GOV_LOG("Removed trailing '<' (potential stop sequence prefix) from response before storage");
      }
    }

    const char* llm_response = llm_response_buffer;
    GOV_LOG("Generated response: %s", llm_response);

    // ========================================================================
    // PREFILL RECONSTRUCTION - Add back the prefilled tag for extraction
    // ========================================================================
    // If we prefilled "<tool_call>\n", the model's output doesn't include it.
    // We need to prepend it so the extraction logic can find it.
    // CRITICAL: Only reconstruct if response looks like JSON (starts with '{')
    // Otherwise we incorrectly tag normal text responses as tool calls!
    if (prefilled_tool_call && strlen(llm_response_buffer) > 0) {
      // Check if response already starts with <tool_call> (shouldn't happen, but be safe)
      if (strncmp(llm_response_buffer, "<tool_call>", 11) != 0) {
        // Skip whitespace to check first non-whitespace character
        const char* first_char = llm_response_buffer;
        while (*first_char == ' ' || *first_char == '\t' || *first_char == '\n') {
          first_char++;
        }
        
        // Only reconstruct if response starts with JSON object ('{')
        if (*first_char == '{') {
          // Create temp buffer with prefilled tag
          size_t response_len = strlen(llm_response_buffer);
          size_t needed = 12 + response_len + 1;  // "<tool_call>\n" + response + '\0'
          
          if (needed < 65536) {  // Buffer size check
            // Shift response right to make room for tag
            memmove(llm_response_buffer + 12, llm_response_buffer, response_len + 1);
            memcpy(llm_response_buffer, "<tool_call>\n", 12);
            GOV_LOG("PREFILL: Reconstructed full tool call with opening tag (response is JSON)");
          } else {
            GOV_ERROR("PREFILL: Response too large to prepend tag (%zu bytes)", needed);
          }
        } else {
          GOV_LOG("PREFILL: Response is normal text (not JSON), skipping reconstruction");
        }
      }
    }

    if (metrics) {
      metrics->iteration_count = iteration + 1;
      metrics->tool_calls_made = 0;
    }

    // Extract tool calls based on model's tool format
    char* tool_calls[10];
    int num_tools = 0;
    tool_format_type_t tool_format = chat_template_get_tool_format(governor->chat_template);

    if (tool_format == TOOL_FORMAT_JSON_IN_XML) {
      // Granite 4.0 format: <tool_call>\n{"name": "...", "arguments": {...}}\n</tool_call>
      num_tools = extract_tool_calls_json(llm_response, tool_calls, 10);
    } else {
      // XML attribute format: <tool_call name="..." attr="..." />
      num_tools = extract_tool_calls(llm_response, tool_calls, 10);
    }

    // Debug: Log tool extraction result
    if (num_tools > 0) {
      GOV_LOG("Found %d tool call(s) in response", num_tools);
    } else {
      // Use DEBUG level since it's normal for final responses to have no tool calls
      ETHERVOX_LOGD("No tool calls in this response (length: %zu)", strlen(llm_response));
      ETHERVOX_LOGD("LLM response without tool calls: %.200s%s", llm_response,
                    strlen(llm_response) > 200 ? "..." : "");
    }

    if (metrics) {
      metrics->tool_calls_made = num_tools;
    }

    // Execute tools if present and tool execution is enabled
    if (num_tools > 0 && governor->tool_execution_enabled) {
      for (int i = 0; i < num_tools; i++) {
        char* tool_result = NULL;
        char* tool_error = NULL;

        // Extract tool name for progress notification (format-specific)
        char* tool_name = NULL;
        if (tool_format == TOOL_FORMAT_JSON_IN_XML) {
          // Parse JSON to get name
          const char* name_start = strstr(tool_calls[i], "\"name\"");
          if (name_start) {
            const char* value_start = strchr(name_start, ':');
            if (value_start) {
              value_start++;
              while (*value_start == ' ' || *value_start == '"')
                value_start++;
              const char* value_end = strchr(value_start, '"');
              if (value_end) {
                size_t len = value_end - value_start;
                tool_name = malloc(len + 1);
                if (tool_name) {
                  strncpy(tool_name, value_start, len);
                  tool_name[len] = '\0';
                }
              }
            }
          }
        } else {
          tool_name = parse_attribute(tool_calls[i], "name");
        }

        if (tool_name && progress_callback) {
          char tool_msg[256];
          snprintf(tool_msg, sizeof(tool_msg), "Calling tool: %s", tool_name);
          progress_callback(ETHERVOX_GOVERNOR_EVENT_TOOL_CALL, tool_msg, user_data);
        }

        // Execute tool using format-specific handler
        int status;
        if (tool_format == TOOL_FORMAT_JSON_IN_XML) {
          status = execute_tool_call_json(tool_calls[i], governor->tool_registry, &tool_result,
                                          &tool_error);
        } else {
          status =
              execute_tool_call(tool_calls[i], governor->tool_registry, &tool_result, &tool_error);
        }

        if (status == 0 && tool_result) {
          GOV_LOG("Tool '%s' called and e successfully, result length: %zu",
                  tool_name ? tool_name : "unknown", strlen(tool_result));
          GOV_LOG("Tool result content: %s", tool_result);

          // Notify tool result (simplified - avoid formatting in hot path)
          if (progress_callback) {
            progress_callback(ETHERVOX_GOVERNOR_EVENT_TOOL_RESULT, tool_result, user_data);
          }

          // Use pre-tokenized wrappers for speed - bypass string operations entirely
          // Instead of: concatenate string → tokenize → decode
          // Do: decode prefix tokens → decode result tokens → decode suffix tokens

          // ============================================================================
          // TOOL RESULT KV CACHE ADDITION - WITH DETAILED LOGGING
          // ============================================================================
          GOV_LOG("=== TOOL RESULT KV CACHE ADDITION START ===");
          GOV_LOG("Tool: %s", tool_name ? tool_name : "unknown");
          GOV_LOG("Result length: %zu chars", strlen(tool_result));
          GOV_LOG("Current KV position: %d", governor->current_kv_pos);
          
          // Decode prefix: chat_template->tool_result_start
          if (governor->tool_result_prefix_tokens && governor->tool_result_prefix_len > 0) {
            // Decode tokens back to text for verification
            char prefix_decoded[512] = {0};
            for (int j = 0; j < governor->tool_result_prefix_len && j < 50; j++) {
              char token_str[64];
              int token_len = llama_token_to_piece(vocab, governor->tool_result_prefix_tokens[j],
                                                    token_str, sizeof(token_str), 0, false);
              if (token_len > 0 && token_len < sizeof(token_str)) {
                token_str[token_len] = '\0';
                strncat(prefix_decoded, token_str, sizeof(prefix_decoded) - strlen(prefix_decoded) - 1);
              }
            }
            GOV_LOG("[PREFIX] Adding %d tokens: '%s'", 
                    governor->tool_result_prefix_len, prefix_decoded);
            
            // Check if prefix will fit in context
            int n_ctx = llama_n_ctx(governor->llm_ctx);
            if (governor->current_kv_pos + governor->tool_result_prefix_len > n_ctx) {
              GOV_ERROR(
                  "Cannot add tool result prefix: would exceed context (pos=%d, prefix=%d, ctx=%d)",
                  governor->current_kv_pos, governor->tool_result_prefix_len, n_ctx);
            } else {
              llama_batch prefix_batch = llama_batch_init(governor->tool_result_prefix_len, 0, 1);
              prefix_batch.n_tokens = governor->tool_result_prefix_len;
              for (int j = 0; j < governor->tool_result_prefix_len; j++) {
                prefix_batch.token[j] = governor->tool_result_prefix_tokens[j];
                prefix_batch.pos[j] = governor->current_kv_pos + j;
                prefix_batch.n_seq_id[j] = 1;
                prefix_batch.seq_id[j][0] = 0;
                prefix_batch.logits[j] = false;
              }
              if (llama_decode(governor->llm_ctx, prefix_batch) != 0) {
                GOV_ERROR("Failed to decode tool result prefix at pos %d",
                          governor->current_kv_pos);
              } else {
                governor->current_kv_pos += governor->tool_result_prefix_len;
                GOV_LOG("[PREFIX] Successfully added to KV cache, new position: %d", 
                        governor->current_kv_pos);
              }
              llama_batch_free(prefix_batch);
            }
          }

          // Tokenize and decode actual tool result (in chunks if large)
          size_t result_len = strlen(tool_result);
          
          // Log first 200 chars of tool result content
          GOV_LOG("[CONTENT] Tool result content (first 200 chars): %.200s%s",
                  tool_result, result_len > 200 ? "..." : "");
          
          int result_n_tokens =
              -llama_tokenize(vocab, tool_result, result_len, NULL, 0, false, false);
          GOV_LOG("[CONTENT] Tool result tokenized to %d tokens", result_n_tokens);
          
          if (result_n_tokens > 0) {
            // Check if tool result will fit in context
            int n_ctx = llama_n_ctx(governor->llm_ctx);
            if (governor->current_kv_pos + result_n_tokens > n_ctx) {
              GOV_LOG(
                  "Tool result too large: %d tokens would exceed context (pos=%d, ctx=%d). "
                  "Truncating.",
                  result_n_tokens, governor->current_kv_pos, n_ctx);
              // Truncate to fit
              result_n_tokens = n_ctx - governor->current_kv_pos;
              if (result_n_tokens <= 0) {
                GOV_ERROR("No context space remaining for tool result");
                continue;  // Skip this tool result
              }
            }

            llama_token* result_tokens = malloc(result_n_tokens * sizeof(llama_token));
            if (result_tokens) {
              llama_tokenize(vocab, tool_result, result_len, result_tokens, result_n_tokens, false,
                             false);

              // Decode first few tokens for verification
              char result_preview[256] = {0};
              int preview_tokens = result_n_tokens < 20 ? result_n_tokens : 20;
              for (int j = 0; j < preview_tokens; j++) {
                char token_str[64];
                int token_len = llama_token_to_piece(vocab, result_tokens[j],
                                                      token_str, sizeof(token_str), 0, false);
                if (token_len > 0 && token_len < sizeof(token_str)) {
                  token_str[token_len] = '\0';
                  strncat(result_preview, token_str, sizeof(result_preview) - strlen(result_preview) - 1);
                }
              }
              GOV_LOG("[CONTENT] First %d tokens decode to: '%s%s'",
                      preview_tokens, result_preview, result_n_tokens > 20 ? "..." : "");

              // Process in chunks to respect batch size limit
              // CRITICAL: Use actual n_batch from context
              const int n_batch = llama_n_batch(governor->llm_ctx);
              int tokens_processed = 0;

              while (tokens_processed < result_n_tokens) {
                int batch_size = (result_n_tokens - tokens_processed > n_batch)
                                     ? n_batch
                                     : (result_n_tokens - tokens_processed);

                llama_batch result_batch = llama_batch_init(batch_size, 0, 1);
                result_batch.n_tokens = batch_size;
                for (int j = 0; j < batch_size; j++) {
                  result_batch.token[j] = result_tokens[tokens_processed + j];
                  result_batch.pos[j] = governor->current_kv_pos + j;
                  result_batch.n_seq_id[j] = 1;
                  result_batch.seq_id[j][0] = 0;
                  result_batch.logits[j] = false;
                }

                if (llama_decode(governor->llm_ctx, result_batch) != 0) {
                  GOV_ERROR("Failed to decode tool result chunk at pos %d",
                            governor->current_kv_pos);
                  llama_batch_free(result_batch);
                  break;
                }
                llama_batch_free(result_batch);

                governor->current_kv_pos += batch_size;
                tokens_processed += batch_size;
                
                GOV_LOG("[CONTENT] Processed chunk %d/%d (%d tokens), KV position now: %d",
                        (tokens_processed / n_batch), 
                        (result_n_tokens + n_batch - 1) / n_batch,
                        batch_size, governor->current_kv_pos);
              }

              free(result_tokens);
              GOV_LOG("[CONTENT] Successfully added %d tokens to KV cache", result_n_tokens);
            }
          }

          // Decode suffix: chat_template->tool_result_end
          if (governor->tool_result_suffix_tokens && governor->tool_result_suffix_len > 0) {
            // Decode tokens back to text for verification
            char suffix_decoded[512] = {0};
            for (int j = 0; j < governor->tool_result_suffix_len && j < 50; j++) {
              char token_str[64];
              int token_len = llama_token_to_piece(vocab, governor->tool_result_suffix_tokens[j],
                                                    token_str, sizeof(token_str), 0, false);
              if (token_len > 0 && token_len < sizeof(token_str)) {
                token_str[token_len] = '\0';
                strncat(suffix_decoded, token_str, sizeof(suffix_decoded) - strlen(suffix_decoded) - 1);
              }
            }
            GOV_LOG("[SUFFIX] Adding %d tokens: '%s'", 
                    governor->tool_result_suffix_len, suffix_decoded);
            
            // Check if suffix will fit in context
            int n_ctx = llama_n_ctx(governor->llm_ctx);
            if (governor->current_kv_pos + governor->tool_result_suffix_len > n_ctx) {
              GOV_ERROR(
                  "Cannot add tool result suffix: would exceed context (pos=%d, suffix=%d, ctx=%d)",
                  governor->current_kv_pos, governor->tool_result_suffix_len, n_ctx);
            } else {
              llama_batch suffix_batch = llama_batch_init(governor->tool_result_suffix_len, 0, 1);
              suffix_batch.n_tokens = governor->tool_result_suffix_len;
              for (int j = 0; j < governor->tool_result_suffix_len; j++) {
                suffix_batch.token[j] = governor->tool_result_suffix_tokens[j];
                suffix_batch.pos[j] = governor->current_kv_pos + j;
                suffix_batch.n_seq_id[j] = 1;
                suffix_batch.seq_id[j][0] = 0;
                suffix_batch.logits[j] =
                    (j == governor->tool_result_suffix_len - 1);  // Only last needs logits
              }
              if (llama_decode(governor->llm_ctx, suffix_batch) != 0) {
                GOV_ERROR("Failed to decode tool result suffix at pos %d",
                          governor->current_kv_pos);
              } else {
                governor->current_kv_pos += governor->tool_result_suffix_len;
                GOV_LOG("[SUFFIX] Successfully added to KV cache, new position: %d", 
                        governor->current_kv_pos);
              }
              llama_batch_free(suffix_batch);
            }
          }

          GOV_LOG("=== TOOL RESULT KV CACHE ADDITION COMPLETE ===");
          GOV_LOG("Final KV position: %d (%.1f%% of context)",
                  governor->current_kv_pos, 
                  (float)governor->current_kv_pos / llama_n_ctx(governor->llm_ctx) * 100.0f);

          // Update processed_length - no string concat needed
          processed_length = strlen(conversation);

          free(tool_result);
        } else if (tool_error) {
          // Format and add tool error to LLM context
          char error_msg[512];
          snprintf(error_msg, sizeof(error_msg), "<tool_error>%s</tool_error>", tool_error);

          GOV_LOG("Tool error: %s", tool_error);

          // Notify error
          if (progress_callback) {
            progress_callback(ETHERVOX_GOVERNOR_EVENT_TOOL_ERROR, error_msg, user_data);
          }

          // Add error tokens to LLM context using same approach as tool_result
          // Decode prefix (error is treated like a tool_result for format purposes)
          // CRITICAL: Validate chat_template before accessing members
          if (!governor->chat_template || (uintptr_t)governor->chat_template < 0x1000 ||
              ((uintptr_t)governor->chat_template >= 0x100000000 && (uintptr_t)governor->chat_template < 0x100001000)) {
            GOV_LOG("WARNING: chat_template corrupted (%p), skipping error formatting", (void*)governor->chat_template);
            continue;  // Skip to next iteration
          }
          
          const char* error_prefix = governor->chat_template->user_start;
          int prefix_n_tokens =
              -llama_tokenize(vocab, error_prefix, strlen(error_prefix), NULL, 0, false, false);
          if (prefix_n_tokens > 0) {
            int n_ctx = llama_n_ctx(governor->llm_ctx);
            if (governor->current_kv_pos + prefix_n_tokens > n_ctx) {
              GOV_ERROR("Cannot add error prefix: would exceed context (pos=%d, prefix=%d, ctx=%d)",
                        governor->current_kv_pos, prefix_n_tokens, n_ctx);
            } else {
              llama_token* prefix_tokens = malloc(prefix_n_tokens * sizeof(llama_token));
              if (prefix_tokens) {
                llama_tokenize(vocab, error_prefix, strlen(error_prefix), prefix_tokens,
                               prefix_n_tokens, false, false);
                llama_batch prefix_batch = llama_batch_init(prefix_n_tokens, 0, 1);
                prefix_batch.n_tokens = prefix_n_tokens;
                for (int j = 0; j < prefix_n_tokens; j++) {
                  prefix_batch.token[j] = prefix_tokens[j];
                  prefix_batch.pos[j] = governor->current_kv_pos + j;
                  prefix_batch.n_seq_id[j] = 1;
                  prefix_batch.seq_id[j][0] = 0;
                  prefix_batch.logits[j] = false;
                }
                if (llama_decode(governor->llm_ctx, prefix_batch) != 0) {
                  GOV_ERROR("Failed to decode error prefix at pos %d", governor->current_kv_pos);
                } else {
                  governor->current_kv_pos += prefix_n_tokens;
                }
                llama_batch_free(prefix_batch);
                free(prefix_tokens);
              }
            }
          }

          // Add error message tokens
          int error_n_tokens =
              -llama_tokenize(vocab, error_msg, strlen(error_msg), NULL, 0, false, false);
          if (error_n_tokens > 0) {
            int n_ctx = llama_n_ctx(governor->llm_ctx);
            if (governor->current_kv_pos + error_n_tokens > n_ctx) {
              error_n_tokens = n_ctx - governor->current_kv_pos;
              if (error_n_tokens <= 0) {
                GOV_ERROR("No context space for tool error");
                free(tool_error);
                if (tool_name)
                  free(tool_name);
                free(tool_calls[i]);
                continue;
              }
            }

            llama_token* error_tokens = malloc(error_n_tokens * sizeof(llama_token));
            if (error_tokens) {
              llama_tokenize(vocab, error_msg, strlen(error_msg), error_tokens, error_n_tokens,
                             false, false);

              // CRITICAL: Use actual n_batch from context
              const int n_batch = llama_n_batch(governor->llm_ctx);
              int tokens_processed = 0;
              while (tokens_processed < error_n_tokens) {
                int batch_size = (error_n_tokens - tokens_processed > n_batch)
                                     ? n_batch
                                     : (error_n_tokens - tokens_processed);
                llama_batch error_batch = llama_batch_init(batch_size, 0, 1);
                error_batch.n_tokens = batch_size;
                for (int j = 0; j < batch_size; j++) {
                  error_batch.token[j] = error_tokens[tokens_processed + j];
                  error_batch.pos[j] = governor->current_kv_pos + j;
                  error_batch.n_seq_id[j] = 1;
                  error_batch.seq_id[j][0] = 0;
                  error_batch.logits[j] = false;
                }
                if (llama_decode(governor->llm_ctx, error_batch) != 0) {
                  GOV_ERROR("Failed to decode error message chunk at pos %d",
                            governor->current_kv_pos);
                  llama_batch_free(error_batch);
                  break;
                }
                llama_batch_free(error_batch);
                governor->current_kv_pos += batch_size;
                tokens_processed += batch_size;
              }
              free(error_tokens);
            }
          }

          // Add suffix to return to assistant context
          // CRITICAL: Validate chat_template before accessing members
          if (!governor->chat_template || (uintptr_t)governor->chat_template < 0x1000 ||
              ((uintptr_t)governor->chat_template >= 0x100000000 && (uintptr_t)governor->chat_template < 0x100001000)) {
            GOV_LOG("WARNING: chat_template corrupted (%p) during suffix", (void*)governor->chat_template);
            break;  // Exit tool error handling
          }
          
          const char* error_suffix = governor->chat_template->assistant_start;
          int suffix_n_tokens =
              -llama_tokenize(vocab, error_suffix, strlen(error_suffix), NULL, 0, false, false);
          if (suffix_n_tokens > 0) {
            int n_ctx = llama_n_ctx(governor->llm_ctx);
            if (governor->current_kv_pos + suffix_n_tokens > n_ctx) {
              GOV_ERROR("Cannot add error suffix: would exceed context (pos=%d, suffix=%d, ctx=%d)",
                        governor->current_kv_pos, suffix_n_tokens, n_ctx);
            } else {
              llama_token* suffix_tokens = malloc(suffix_n_tokens * sizeof(llama_token));
              if (suffix_tokens) {
                llama_tokenize(vocab, error_suffix, strlen(error_suffix), suffix_tokens,
                               suffix_n_tokens, false, false);
                llama_batch suffix_batch = llama_batch_init(suffix_n_tokens, 0, 1);
                suffix_batch.n_tokens = suffix_n_tokens;
                for (int j = 0; j < suffix_n_tokens; j++) {
                  suffix_batch.token[j] = suffix_tokens[j];
                  suffix_batch.pos[j] = governor->current_kv_pos + j;
                  suffix_batch.n_seq_id[j] = 1;
                  suffix_batch.seq_id[j][0] = 0;
                  suffix_batch.logits[j] = (j == suffix_n_tokens - 1);
                }
                if (llama_decode(governor->llm_ctx, suffix_batch) != 0) {
                  GOV_ERROR("Failed to decode error suffix at pos %d", governor->current_kv_pos);
                } else {
                  governor->current_kv_pos += suffix_n_tokens;
                }
                llama_batch_free(suffix_batch);
                free(suffix_tokens);
              }
            }
          }

          free(tool_error);
        }

        if (tool_name)
          free(tool_name);
      }

      // Check if any executed tool was a terminal tool (speak, listen)
      // This MUST happen before freeing tool_calls
      bool has_terminal_tool = false;
      for (int i = 0; i < num_tools; i++) {
        // Parse tool name from tool call
        const char* name_start = strstr(tool_calls[i], "\"name\"");
        if (name_start) {
          const char* value_start = strchr(name_start, ':');
          if (value_start) {
            value_start++;
            while (*value_start == ' ' || *value_start == '"')
              value_start++;

            // Extract tool name for logging
            char tool_name_buf[64] = {0};
            int j = 0;
            while (j < 63 && value_start[j] && value_start[j] != '"' && value_start[j] != ',') {
              tool_name_buf[j] = value_start[j];
              j++;
            }

            ETHERVOX_LOGD("Checking tool #%d: '%s'", i, tool_name_buf);

            if (strncmp(value_start, "speak", 5) == 0 || strncmp(value_start, "listen", 6) == 0) {
              has_terminal_tool = true;
              GOV_LOG("Detected terminal tool: %s", tool_name_buf);
              break;
            }
          }
        }
      }

      // Now free tool_calls AFTER checking for terminal tools
      for (int i = 0; i < num_tools; i++) {
        free(tool_calls[i]);
      }

      // If we executed a terminal tool (speak/listen), stop iteration
      if (has_terminal_tool) {
        GOV_LOG("Terminal tool executed (speak/listen), ending iteration");
        // Return empty response - terminal tool already handled output
        if (response) {
          *response = strdup("");
        }
        if (progress_callback) {
          progress_callback(ETHERVOX_GOVERNOR_EVENT_COMPLETE, "Terminal tool executed", user_data);
        }
        return ETHERVOX_GOVERNOR_SUCCESS;
      }

      // If we executed tools, continue iteration to let LLM incorporate results
      // (unless we're at max iterations)
      if (iteration + 1 < governor->config.max_iterations) {
        continue;
      } else {
        // Max iterations reached after tool execution - return what we have
        GOV_LOG("Max iterations reached after tool execution, returning last response");
        if (response) {
          *response = strdup(llm_response);
        }
        if (progress_callback) {
          progress_callback(ETHERVOX_GOVERNOR_EVENT_COMPLETE, "Max iterations reached", user_data);
        }
        return ETHERVOX_GOVERNOR_TIMEOUT;
      }
    } else {
      // No tools executed - this is a final response
      // Return the response and mark as complete
      if (response) {
        *response = strdup(llm_response);
        if (!*response) {
          if (error)
            *error = strdup("Memory allocation failed");
          return ETHERVOX_GOVERNOR_ERROR;
        }
      }

      if (progress_callback) {
        progress_callback(ETHERVOX_GOVERNOR_EVENT_COMPLETE, "Answer ready", user_data);
      }

      free(llm_response_buffer);
      return ETHERVOX_GOVERNOR_SUCCESS;
    }
  }

  // Should not reach here, but handle gracefully
  if (error) {
    *error = strdup("Unexpected loop termination");
  }
  return ETHERVOX_GOVERNOR_ERROR;

#endif  // ETHERVOX_WITH_LLAMA && LLAMA_HEADER_AVAILABLE
}

/**
 * Get the number of iterations used in last execute call
 */
uint32_t ethervox_governor_get_last_iteration_count(ethervox_governor_t* governor) {
  return governor ? governor->last_iteration_count : 0;
}

/**
 * Cleanup Governor resources
 */
void ethervox_governor_cleanup(ethervox_governor_t* governor) {
  if (!governor)
    return;

#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  if (governor->llm_ctx) {
    llama_free(governor->llm_ctx);
    governor->llm_ctx = NULL;
  }
  if (governor->llm_model) {
    llama_model_free(governor->llm_model);
    governor->llm_model = NULL;
  }
  if (governor->model_path) {
    free(governor->model_path);
    governor->model_path = NULL;
  }

  // Free pre-tokenized wrappers
  if (governor->tool_result_prefix_tokens) {
    free(governor->tool_result_prefix_tokens);
    governor->tool_result_prefix_tokens = NULL;
  }
  if (governor->tool_result_suffix_tokens) {
    free(governor->tool_result_suffix_tokens);
    governor->tool_result_suffix_tokens = NULL;
  }
  
  // Free KV cache tracking buffer
  if (governor->kv_cache_tokens) {
    free(governor->kv_cache_tokens);
    governor->kv_cache_tokens = NULL;
    governor->kv_cache_tokens_capacity = 0;
    governor->kv_cache_tokens_count = 0;
  }

  llama_backend_free();
#endif

  // Cleanup conversation history
  cleanup_conversation_history(&governor->conversation_history);

  free(governor);
}

ethervox_result_t ethervox_governor_reset_conversation(ethervox_governor_t* governor) {
  if (!governor) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  return ETHERVOX_ERROR_INVALID_ARGUMENT;
#else
  // Clear KV cache back to system prompt
  if (!governor->llm_ctx || governor->system_prompt_token_count == 0) {
    GOV_LOG("Cannot reset: model not loaded");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (!governor->system_prompt_tokens || governor->system_prompt_tokens_len == 0) {
    GOV_ERROR("Cannot reset: no saved system prompt tokens for RELIGHT");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  llama_memory_t mem = llama_get_memory(governor->llm_ctx);
  int32_t max_pos = llama_memory_seq_pos_max(mem, 0);

  if (max_pos <= governor->system_prompt_token_count) {
    GOV_LOG("Conversation already clean (at position %d, system prompt is %d tokens)", max_pos,
            governor->system_prompt_token_count);
    return ETHERVOX_SUCCESS;
  }

  int n_ctx = llama_n_ctx(governor->llm_ctx);
  GOV_LOG("Resetting conversation: max_pos=%d, system_prompt=%d (%d%% full)", max_pos,
          governor->system_prompt_token_count, (max_pos * 100 / n_ctx));

  // ========================================================================
  // NUCLEAR CLEAR + RELIGHT (without summary reload)
  // ========================================================================
  // We can't trust llama_memory_seq_rm due to llama.cpp bugs, so we nuclear
  // clear everything and RELIGHT with system prompt only (no conversation summary)

  GOV_LOG("Nuclear clear: wiping entire KV cache for clean reset...");
  llama_memory_clear(mem, true);  // Clear both metadata and data
  int32_t actual_max = llama_memory_seq_pos_max(mem, 0);
  GOV_LOG("Nuclear clear complete: KV cache at position %d", actual_max);

  // RELIGHT: Restore system prompt from saved tokens
  GOV_LOG("RELIGHT: Restoring system prompt (%d tokens) without conversation summary...",
          governor->system_prompt_tokens_len);

  // Reprocess system prompt in chunks (same as initial load)
  // CRITICAL: Use actual n_batch from context (not hardcoded 1024)
  // Small models may have n_batch=512 or even 256
  int actual_n_batch = llama_n_batch(governor->llm_ctx);
  int chunk_size = (actual_n_batch > 0) ? actual_n_batch : 512;
  GOV_LOG("Using chunk_size=%d (llama_n_batch=%d)", chunk_size, actual_n_batch);
  bool relight_successful = true;

  for (int i = 0; i < governor->system_prompt_tokens_len; i += chunk_size) {
    int chunk_len = (i + chunk_size > governor->system_prompt_tokens_len)
                        ? (governor->system_prompt_tokens_len - i)
                        : chunk_size;
    bool is_final_chunk = (i + chunk_size >= governor->system_prompt_tokens_len);

    // Create batch with explicit positions
    llama_batch batch = llama_batch_init(chunk_len, 0, 1);
    batch.n_tokens = chunk_len;
    for (int j = 0; j < chunk_len; j++) {
      batch.token[j] = governor->system_prompt_tokens[i + j];
      batch.pos[j] = i + j;
      batch.n_seq_id[j] = 1;
      batch.seq_id[j][0] = 0;
      batch.logits[j] = false;
    }
    // Compute logits only for the very last token
    if (is_final_chunk) {
      batch.logits[chunk_len - 1] = true;
    }

    if (llama_decode(governor->llm_ctx, batch) != 0) {
      GOV_ERROR("RELIGHT FAILED: Could not restore system prompt chunk at token %d", i);
      relight_successful = false;
      llama_batch_free(batch);
      break;
    }

    llama_batch_free(batch);
  }

  if (!relight_successful) {
    GOV_ERROR("Conversation reset failed: could not restore system prompt");
    governor->current_kv_pos = 0;
    governor->system_prompt_lost = true;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // System prompt successfully restored!
  governor->current_kv_pos = governor->system_prompt_token_count;
  governor->system_prompt_lost = false;
  GOV_LOG("RELIGHT COMPLETE: System prompt restored at position %d (%d%% full)",
          governor->current_kv_pos, (governor->current_kv_pos * 100 / n_ctx));

  // Clear conversation history tracking
  cleanup_conversation_history(&governor->conversation_history);
  init_conversation_history(&governor->conversation_history, 32);

  // Reset context manager state (compatible with moving window system)
  governor->context_manager.current_health = CTX_HEALTH_OK;
  governor->context_manager.overflow_event_count = 0;
  governor->context_manager.last_gc_position = governor->system_prompt_token_count;
  governor->context_manager.management_in_progress = false;

  // TODO: Handle persistent memory - currently memory survives reset
  // Future: Create new memory data file or mark old memories as archived
  GOV_LOG("Note: Persistent memory not cleared (memories from before reset remain searchable)");

  GOV_LOG("Conversation reset complete: clean slate with system prompt only");

  return ETHERVOX_SUCCESS;
#endif
}

ethervox_tool_registry_t* ethervox_governor_get_registry(ethervox_governor_t* governor) {
  return governor ? governor->tool_registry : NULL;
}

void ethervox_governor_set_manifest(ethervox_governor_t* governor, tool_manifest_registry_t* manifest) {
  if (governor) {
    governor->manifest_registry = manifest;
    GOV_LOG("[Manifest] Manifest registry attached to governor");
    if (manifest) {
      GOV_LOG("  Tools available: %s", manifest->tools_available ? "YES" : "NO");
      GOV_LOG("  Fallback level: %u (%s)",
              manifest->fallback_level,
              ethervox_tool_fallback_level_name(manifest->fallback_level));
      GOV_LOG("  Optimization loaded: %s", manifest->optimization_loaded ? "YES" : "NO");
    }
  }
}

// ============================================================================
// KV Cache Persistence Accessor Functions
// ============================================================================

llama_token* ethervox_governor_get_system_tokens(struct ethervox_governor* gov, int* len) {
  if (!gov || !len) return NULL;
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  *len = gov->system_prompt_tokens_len;
  return gov->system_prompt_tokens;
#else
  *len = 0;
  return NULL;
#endif
}

struct llama_context* ethervox_governor_get_context(struct ethervox_governor* gov) {
  if (!gov) return NULL;
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  return gov->llm_ctx;
#else
  return NULL;
#endif
}

void ethervox_governor_set_system_tokens(struct ethervox_governor* gov, 
                                         llama_token* tokens, int len) {
  if (!gov || !tokens || len <= 0) return;
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  // Free old tokens if they exist
  if (gov->system_prompt_tokens) {
    free(gov->system_prompt_tokens);
  }
  
  // Store new tokens (takes ownership)
  gov->system_prompt_tokens = tokens;
  gov->system_prompt_tokens_len = len;
  gov->system_prompt_token_count = len;
  gov->current_kv_pos = len;
  
  GOV_LOG("[KV Cache] Stored %d cached system prompt tokens, KV pos set to %d", len, len);
#endif
}

const char* ethervox_governor_get_model_path(struct ethervox_governor* gov) {
  if (!gov) return "";
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  return gov->model_path ? gov->model_path : "";
#else
  return "";
#endif
}

void ethervox_governor_request_interrupt(ethervox_governor_t* governor) {
  if (governor) {
    governor->interrupt_requested = true;
    GOV_LOG("[Interrupt] Interrupt requested - will abort at next iteration");
  }
}

const chat_template_t* ethervox_governor_get_chat_template(ethervox_governor_t* governor) {
  return governor ? governor->chat_template : NULL;
}

void ethervox_governor_set_tool_execution(ethervox_governor_t* governor, bool enabled) {
  if (governor) {
    governor->tool_execution_enabled = enabled;
    GOV_LOG("Tool execution %s", enabled ? "enabled" : "disabled");
  }
}

ethervox_result_t ethervox_governor_summarize_and_clear_cache(ethervox_governor_t* governor,
                                                              bool force_clear) {
  if (!governor) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  return ETHERVOX_ERROR_INVALID_ARGUMENT;
#else
  if (!governor->llm_ctx || governor->system_prompt_token_count == 0) {
    GOV_LOG("Cannot summarize: model not loaded");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  llama_memory_t mem = llama_get_memory(governor->llm_ctx);
  int32_t max_pos = llama_memory_seq_pos_max(mem, 0);
  int n_ctx = llama_n_ctx(governor->llm_ctx);

  // Check if we need to clear
  if (!force_clear && max_pos <= governor->system_prompt_token_count) {
    GOV_LOG("Cache already clean (at position %d, system prompt is %d tokens)", max_pos,
            governor->system_prompt_token_count);
    return ETHERVOX_SUCCESS;
  }

  if (!force_clear && max_pos <= (n_ctx / 2)) {
    GOV_LOG("Cache only at %d%% capacity - not clearing (use force_clear=true to override)",
            (max_pos * 100 / n_ctx));
    return ETHERVOX_SUCCESS;
  }

  GOV_LOG("Manual cache summarization: max_pos=%d, system_prompt=%d (%d%% full)", max_pos,
          governor->system_prompt_token_count, (max_pos * 100 / n_ctx));

  // Build conversation context from history
  char conversation_context[4096] = {0};
  int ctx_len = 0;

  // Include recent turns (last 10 or all if fewer)
  uint32_t start_turn = (governor->conversation_history.turn_count > 10)
                            ? (governor->conversation_history.turn_count - 10)
                            : 0;

  for (uint32_t i = start_turn; i < governor->conversation_history.turn_count; i++) {
    conversation_turn_t* turn = &governor->conversation_history.turns[i];
    int remaining = sizeof(conversation_context) - ctx_len - 1;

    if (remaining > 200) {
      int written = snprintf(conversation_context + ctx_len, remaining, "%s: %s\n",
                             turn->is_user ? "User" : "Assistant", turn->preview);
      if (written > 0 && written < remaining) {
        ctx_len += written;
      }
    }
  }

  // Create summarization prompt
  char summary_prompt[5120];
  snprintf(summary_prompt, sizeof(summary_prompt),
           "Summarize this conversation in 2-3 concise sentences, capturing key topics, "
           "decisions, and context that should be remembered:\n\n%s\n\n"
           "Summary (2-3 sentences):",
           conversation_context);

  // Tokenize
  llama_token* summary_tokens = (llama_token*)malloc(1024 * sizeof(llama_token));
  if (!summary_tokens) {
    GOV_ERROR("Failed to allocate summary tokens");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  const struct llama_vocab* vocab = llama_model_get_vocab(governor->llm_model);
  int n_summary_tokens = llama_tokenize(vocab, summary_prompt, strlen(summary_prompt),
                                        summary_tokens, 1024, true, false);

  if (n_summary_tokens < 0) {
    GOV_ERROR("Failed to tokenize summary prompt");
    free(summary_tokens);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  GOV_LOG("Processing summary prompt (%d tokens)...", n_summary_tokens);

  // First, evaluate the prompt tokens through the model in chunks
  int chunk_size = 512;
  for (int i = 0; i < n_summary_tokens; i += chunk_size) {
    int chunk_len = (i + chunk_size > n_summary_tokens) ? (n_summary_tokens - i) : chunk_size;
    bool is_last_chunk = (i + chunk_size >= n_summary_tokens);

    llama_batch batch = llama_batch_init(chunk_len, 0, 1);
    batch.n_tokens = chunk_len;
    for (int j = 0; j < chunk_len; j++) {
      batch.token[j] = summary_tokens[i + j];
      batch.pos[j] = governor->current_kv_pos + i + j;
      batch.n_seq_id[j] = 1;
      batch.seq_id[j][0] = 0;
      batch.logits[j] = false;
    }
    // Only compute logits for the last token of the last chunk
    if (is_last_chunk) {
      batch.logits[chunk_len - 1] = true;
    }

    if (llama_decode(governor->llm_ctx, batch) != 0) {
      GOV_ERROR("Failed to process summary prompt chunk at token %d", i);
      llama_batch_free(batch);
      free(summary_tokens);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    llama_batch_free(batch);
  }

  GOV_LOG("Summary prompt processed, generating response...");

  // Generate summary
  char llm_summary[1024] = {0};
  int summary_len = 0;
  bool summary_complete = false;

  struct llama_sampler* temp_sampler =
      llama_sampler_chain_init(llama_sampler_chain_default_params());
  llama_sampler_chain_add(temp_sampler, llama_sampler_init_temp(0.3f));
  llama_sampler_chain_add(temp_sampler, llama_sampler_init_dist(0));

  int current_gen_pos = governor->current_kv_pos + n_summary_tokens;

  for (int i = 0; i < 100 && !summary_complete; i++) {
    llama_token next_token = llama_sampler_sample(temp_sampler, governor->llm_ctx, -1);
    llama_sampler_accept(temp_sampler, next_token);

    const struct llama_vocab* vocab_check = llama_model_get_vocab(governor->llm_model);
    if (next_token == llama_vocab_eos(vocab_check) || next_token == llama_vocab_nl(vocab_check)) {
      summary_complete = true;
    } else {
      const struct llama_vocab* vocab = llama_model_get_vocab(governor->llm_model);
      char piece[32];
      int n_chars = llama_token_to_piece(vocab, next_token, piece, sizeof(piece), 0, false);

      if (n_chars > 0 && summary_len + n_chars < sizeof(llm_summary) - 1) {
        memcpy(llm_summary + summary_len, piece, n_chars);
        summary_len += n_chars;
        llm_summary[summary_len] = '\0';
      }

      // Feed the token back for next iteration
      llama_batch batch = llama_batch_init(1, 0, 1);
      batch.n_tokens = 1;
      batch.token[0] = next_token;
      batch.pos[0] = current_gen_pos + i;
      batch.n_seq_id[0] = 1;
      batch.seq_id[0][0] = 0;
      batch.logits[0] = true;

      if (llama_decode(governor->llm_ctx, batch) != 0) {
        llama_batch_free(batch);
        break;
      }

      llama_batch_free(batch);
    }
  }

  llama_sampler_free(temp_sampler);
  free(summary_tokens);

  GOV_LOG("Generated summary: %s", llm_summary[0] ? llm_summary : "(empty)");

  // Store in memory
  ethervox_tool_t* memory_tool = NULL;
  for (uint32_t i = 0; i < governor->tool_registry->tool_count; i++) {
    if (strcmp(governor->tool_registry->tools[i].name, "memory_store") == 0) {
      memory_tool = &governor->tool_registry->tools[i];
      break;
    }
  }

  if (memory_tool && llm_summary[0] != '\0') {
    char summary_content[2048];
    snprintf(summary_content, sizeof(summary_content),
             "[Manual Cache Clear - Context Summary]\n\n%s\n\n"
             "Turn count: %u. Cache cleared manually at %d%% capacity.",
             llm_summary, governor->turn_counter, (max_pos * 100 / n_ctx));

    // Escape for JSON
    char escaped_summary[4096];
    int esc_idx = 0;
    for (int i = 0; summary_content[i] && esc_idx < sizeof(escaped_summary) - 2; i++) {
      if (summary_content[i] == '"' || summary_content[i] == '\\') {
        escaped_summary[esc_idx++] = '\\';
      }
      if (summary_content[i] == '\n') {
        escaped_summary[esc_idx++] = '\\';
        escaped_summary[esc_idx++] = 'n';
      } else {
        escaped_summary[esc_idx++] = summary_content[i];
      }
    }
    escaped_summary[esc_idx] = '\0';

    char memory_args[8192];
    snprintf(memory_args, sizeof(memory_args),
             "{\"text\":\"%s\","
             "\"importance\":0.90,"
             "\"tags\":[\"context_summary\",\"manual_clear\",\"auto_generated\"]}",
             escaped_summary);

    char* store_result = NULL;
    char* store_error = NULL;

    if (memory_tool->execute(memory_args, &store_result, &store_error) == 0) {
      GOV_LOG("Stored conversation summary in memory");
    } else {
      GOV_ERROR("Failed to store summary: %s", store_error ? store_error : "unknown");
    }

    free(store_result);
    free(store_error);
  }

  // Clear the cache
  llama_memory_seq_rm(mem, 0, governor->system_prompt_token_count, -1);
  governor->current_kv_pos = governor->system_prompt_token_count;

  GOV_LOG("Cache cleared: now at position %d (%d%% full)", governor->current_kv_pos,
          (governor->current_kv_pos * 100 / n_ctx));

  return ETHERVOX_SUCCESS;
#endif
}
