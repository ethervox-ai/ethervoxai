/**
 * @file governor.h
 * @brief Governor orchestration system for LLM-driven tool execution
 *
 * The Governor is the central orchestrator that manages iterative interaction
 * between the LLM (Phi-3.5-mini-instruct) and compute/action tools until
 * confidence threshold is met.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_GOVERNOR_H
#define ETHERVOX_GOVERNOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "chat_template.h"
#include "error.h"
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Execution Context for Conversational AI
// ============================================================================

/**
 * Input source tracking - where did the user's query come from?
 */
typedef enum {
  ETHERVOX_INPUT_SOURCE_CLI,     // Typed command in terminal
  ETHERVOX_INPUT_SOURCE_VOICE,   // Voice conversation (wake word → STT)
  ETHERVOX_INPUT_SOURCE_API,     // External API call
  ETHERVOX_INPUT_SOURCE_UNKNOWN  // Default/unspecified
} ethervox_input_source_t;

/**
 * Conversation turn state for bidirectional interaction
 */
typedef enum {
  ETHERVOX_TURN_USER,        // User is speaking/typing
  ETHERVOX_TURN_ASSISTANT,   // Assistant is speaking
  ETHERVOX_TURN_WAITING,     // Waiting for user response
  ETHERVOX_TURN_INTERRUPTED  // User interrupted assistant
} ethervox_conversation_turn_t;

/**
 * Tool callbacks for real-time conversational control
 *
 * These callbacks allow LLM tools (speak, listen) to interact with
 * the conversation system in real-time, enabling bidirectional
 * interruption and natural turn-taking.
 */
typedef struct {
  /**
   * Called when LLM invokes "speak" tool
   * @param text Text to speak via TTS
   * @param language Language code ("en", "de", "es", "zh") or NULL for auto-detect
   * @param wait_for_response If true, open microphone after speaking
   * @param allow_interrupt If true, user can interrupt by speaking
   * @param user_data Typically the conversation session
   * @return ETHERVOX_SUCCESS or error code
   */
  ethervox_result_t (*on_speak)(const char* text, const char* language, bool wait_for_response,
                                bool allow_interrupt, void* user_data);

  /**
   * Called when LLM invokes "listen" tool
   * @param user_input Output: captured user speech (caller must free)
   * @param timeout_ms Maximum wait time in milliseconds
   * @param prompt_hint Optional hint about what to listen for
   * @param user_data Typically the conversation session
   * @return ETHERVOX_SUCCESS or error code
   */
  ethervox_result_t (*on_listen)(char** user_input, int timeout_ms, const char* prompt_hint,
                                 void* user_data);

  /**
   * Called when user interrupts during TTS playback
   * @param user_data Typically the conversation session
   * @return ETHERVOX_SUCCESS or error code
   */
  ethervox_result_t (*on_interrupt)(void* user_data);

  void* user_data;  // Opaque pointer (typically conversation_session)
} ethervox_conversation_callbacks_t;

/**
 * Execution context passed to Governor for conversational awareness
 *
 * This gives the LLM knowledge about:
 * - Where input came from (voice vs CLI)
 * - What output capabilities are available (TTS, screen)
 * - Conversation state (turn-taking)
 * - Callbacks for real-time interaction
 */
typedef struct {
  ethervox_input_source_t source;  // Where did input come from?
  const char* source_description;  // Human-readable (e.g., "voice conversation")

  // Capability flags
  bool tts_available;         // Can we speak responses?
  bool microphone_available;  // Can we listen for input?

  // Conversation state
  ethervox_conversation_turn_t current_turn;  // Who's turn is it?

  // Callbacks for conversational tools (NULL if not applicable)
  ethervox_conversation_callbacks_t* callbacks;
} ethervox_execution_context_t;

// Android platform helper (defined in ethervox_android_core.c)
#ifdef ETHERVOX_PLATFORM_ANDROID
const char* ethervox_android_get_files_dir(void);
#endif

// Forward declarations
typedef struct tool_manifest_registry tool_manifest_registry_t;

/**
 * Tool execution function signature
 *
 * @param args_json JSON string with tool parameters
 * @param result Output: JSON result string (caller must free)
 * @param error Output: Error message if execution fails (caller must free)
 * @return 0 on success, negative on error
 */
typedef int (*ethervox_tool_execute_fn)(const char* args_json, char** result, char** error);

/**
 * Tool definition
 */
typedef struct {
  char name[64];                      // Tool name (e.g., "calculator_compute")
  char description[256];              // Human-readable description
  char parameters_json_schema[1024];  // JSON schema for parameters

  ethervox_tool_execute_fn execute;  // Execution function

  bool is_deterministic;       // Same input = same output
  bool requires_confirmation;  // Ask user before executing
  bool is_stateful;            // Modifies system state
  float estimated_latency_ms;  // Expected execution time
} ethervox_tool_t;

/**
 * Tool registry
 */
typedef struct {
  ethervox_tool_t* tools;  // Array of tools
  uint32_t tool_count;     // Number of registered tools
  uint32_t capacity;       // Allocated capacity
} ethervox_tool_registry_t;

/**
 * Export current tool registry to binary manifest file
 * This generates tools.bin from runtime-registered tools
 *
 * @param registry Tool registry to export
 * @param binary_path Output path for tools.bin
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_tool_registry_export_manifest(const ethervox_tool_registry_t* registry,
                                                         const char* binary_path);

/**
 * Context health status
 */
typedef enum {
  CTX_HEALTH_OK,        // 0-60% full - normal operation
  CTX_HEALTH_WARNING,   // 60-80% full - start planning
  CTX_HEALTH_CRITICAL,  // 80-95% full - must act now
  CTX_HEALTH_OVERFLOW   // >95% full - emergency fallback
} context_health_t;

/**
 * Context manager state
 */
typedef struct {
  context_health_t current_health;
  uint32_t overflow_event_count;
  int32_t last_gc_position;
  bool management_in_progress;
} context_manager_state_t;

/**
 * Conversation turn tracking
 */
typedef struct {
  uint32_t turn_number;  // Sequence number
  int32_t kv_start;      // First token position
  int32_t kv_end;        // Last token position
  time_t timestamp;      // When this turn occurred
  float importance;      // Estimated importance (0.0-1.0)
  bool is_user;          // User vs assistant turn
  char preview[128];     // First 128 chars for debugging
} conversation_turn_t;

/**
 * Conversation history
 */
typedef struct {
  conversation_turn_t* turns;
  uint32_t turn_count;
  uint32_t capacity;
} conversation_history_t;

/**
 * Governor execution status
 */
typedef enum {
  ETHERVOX_GOVERNOR_SUCCESS,             // Confidence met, response ready
  ETHERVOX_GOVERNOR_NEED_CLARIFICATION,  // Need more info from user
  ETHERVOX_GOVERNOR_TIMEOUT,             // Exceeded iteration/time limits
  ETHERVOX_GOVERNOR_ERROR,               // Execution error
  ETHERVOX_GOVERNOR_USER_DENIED,         // User denied tool execution
  ETHERVOX_GOVERNOR_INTERRUPTED          // Generation interrupted by user
} ethervox_governor_status_t;

/**
 * Confidence metrics from LLM response
 */
typedef struct {
  float confidence;              // 0.0-1.0
  uint32_t iteration_count;      // Iterations used
  uint32_t tool_calls_made;      // Tools called
  bool has_explicit_confidence;  // LLM provided <confidence> tag
} ethervox_confidence_metrics_t;

/**
 * Governor progress event types
 */
typedef enum {
  ETHERVOX_GOVERNOR_EVENT_ITERATION_START,      // Starting new iteration
  ETHERVOX_GOVERNOR_EVENT_THINKING,             // Generating response from LLM
  ETHERVOX_GOVERNOR_EVENT_TOOL_CALL,            // Calling a tool
  ETHERVOX_GOVERNOR_EVENT_TOOL_RESULT,          // Tool execution complete
  ETHERVOX_GOVERNOR_EVENT_TOOL_ERROR,           // Tool execution failed
  ETHERVOX_GOVERNOR_EVENT_CONFIDENCE_UPDATE,    // Confidence level changed
  ETHERVOX_GOVERNOR_EVENT_CONTEXT_SUMMARIZING,  // Context being summarized before clearing
  ETHERVOX_GOVERNOR_EVENT_CONTEXT_CLEARED,      // Context cleared, summary stored in memory
  ETHERVOX_GOVERNOR_EVENT_MANIFEST_LOADING,     // Manifest system initializing
  ETHERVOX_GOVERNOR_EVENT_MANIFEST_READY,       // Manifest loaded successfully (optimal state)
  ETHERVOX_GOVERNOR_EVENT_MANIFEST_FALLBACK_LEVEL_1,  // Binary manifest one-liners (good fallback)
  ETHERVOX_GOVERNOR_EVENT_MANIFEST_FALLBACK_LEVEL_2,  // LLM-only mode (degraded, suggest
                                                      // optimization)
  ETHERVOX_GOVERNOR_EVENT_COMPLETE                    // Final answer ready
} ethervox_governor_event_type_t;

/**
 * Progress callback for Governor execution
 * Allows UI to display intermediate reasoning steps
 *
 * @param event_type Type of event
 * @param message Human-readable message
 * @param user_data User data passed to execute function
 */
typedef void (*ethervox_governor_progress_callback)(ethervox_governor_event_type_t event_type,
                                                    const char* message, void* user_data);

/**
 * System prompt loading mode
 */
typedef enum {
  ETHERVOX_GOVERNOR_MODE_FULL,    // Full prompt with all tools (slower load, full capabilities)
  ETHERVOX_GOVERNOR_MODE_MINIMAL  // Brief prompt without tools (fast load, limited capabilities)
} ethervox_governor_system_prompt_mode_t;

/**
 * Governor configuration
 */
typedef struct {
  float confidence_threshold;             // Stop when confidence >= this (default: 0.85)
  uint32_t max_iterations;                // Max tool execution loops (default: 5)
  uint32_t max_tool_calls_per_iteration;  // Max tool calls per iteration (default: 10)
  uint32_t timeout_seconds;               // Total execution timeout (default: 30)
  uint32_t max_tokens_per_response;       // Max tokens to generate per LLM response (default: 2048)

  // Runtime-adjustable LLM parameters (from settings)
  uint32_t gpu_layers;    // Number of layers to offload to GPU (0 = CPU only)
  uint32_t context_size;  // Context window size (default from config.h)
  int n_threads;          // Number of CPU threads (-1 = auto-detect)
  float temperature;      // Sampling temperature (0.0-2.0)

  // Mobile optimization and privacy features
  ethervox_governor_system_prompt_mode_t
      system_prompt_mode;       // Full or minimal prompt (default: FULL)
  bool disable_memory_logging;  // Secret mode - disable conversation logging (default: false)
  bool enable_tool_prefilling;   // Force tool calls with prefix for common queries (default: false)
} ethervox_governor_config_t;

/**
 * Governor state (opaque)
 */
typedef struct ethervox_governor ethervox_governor_t;

// ============================================================================
// Tool Registry Functions
// ============================================================================

/**
 * Initialize tool registry
 */
ethervox_result_t ethervox_tool_registry_init(ethervox_tool_registry_t* registry,
                                              uint32_t initial_capacity);

/**
 * Register a tool
 */
ethervox_result_t ethervox_tool_registry_add(ethervox_tool_registry_t* registry,
                                             const ethervox_tool_t* tool);

/**
 * Find tool by name
 */
const ethervox_tool_t* ethervox_tool_registry_find(const ethervox_tool_registry_t* registry,
                                                   const char* name);

/**
 * Build system prompt with all tools
 *
 * @param registry Tool registry
 * @param chat_template Chat template for formatting
 * @param buffer Output buffer for system prompt
 * @param buffer_size Size of output buffer
 * @param memory_store Optional memory store for adaptive learning (can be NULL)
 * @param model_path Optional model path for loading optimized prompts (can be NULL)
 */
ethervox_result_t ethervox_tool_registry_build_system_prompt(
    const ethervox_tool_registry_t* registry, const chat_template_t* chat_template, char* buffer,
    size_t buffer_size, void* memory_store, const char* model_path);

/**
 * Cleanup registry
 */
void ethervox_tool_registry_cleanup(ethervox_tool_registry_t* registry);

// ============================================================================
// Governor Functions
// ============================================================================

/**
 * Initialize Governor with configuration
 *
 * @param governor Output: Governor instance (caller must free with cleanup)
 * @param config Governor configuration (NULL for defaults)
 * @param registry Tool registry
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_init(ethervox_governor_t** governor,
                                         const ethervox_governor_config_t* config,
                                         ethervox_tool_registry_t* tool_registry);

/**
 * Progress callback for model loading and system prompt processing
 *
 * @param stage Description of current stage (e.g., "loading_model", "processing_prompt")
 * @param progress Progress value 0.0-1.0
 * @param message Human-readable progress message
 * @param user_data User data pointer
 * @return true to continue, false to cancel
 */
typedef bool (*ethervox_load_progress_callback)(const char* stage, float progress,
                                                const char* message, void* user_data);

/**
 * Load Phi-3.5-mini-instruct model and process system prompt
 *
 * @param governor Governor instance
 * @param model_path Path to GGUF model file
 * @param cache_dir Directory to store/load KV cache (can be NULL to disable caching)
 * @param progress_callback Optional progress callback (can be NULL)
 * @param user_data User data for progress callback (can be NULL)
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_load_model(ethervox_governor_t* governor,
                                               const char* model_path,
                                               const char* cache_dir,
                                               ethervox_load_progress_callback progress_callback,
                                               void* user_data);

/**
 * Unload the Governor model to free memory
 * Keeps the Governor structure intact so it can be reloaded later
 *
 * @param governor Governor instance
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_unload_model(ethervox_governor_t* governor);

/**
 * Reload the Governor model using the previously saved model path
 *
 * @param governor Governor instance
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_reload_model(ethervox_governor_t* governor);

/**
 * Check if the Governor model is currently loaded
 *
 * @param governor Governor instance
 * @return true if model is loaded, false otherwise
 */
bool ethervox_governor_is_loaded(ethervox_governor_t* governor);

/**
 * Get KV cache usage information
 *
 * @param governor Governor instance
 * @param current_pos Output: Current KV cache position (can be NULL)
 * @param context_size Output: Total context size (can be NULL)
 * @return Usage percentage (0.0 to 1.0), or -1.0 if not loaded
 */
float ethervox_governor_get_kv_cache_usage(ethervox_governor_t* governor, int32_t* current_pos, int32_t* context_size);

/**
 * Get full KV cache contents as decoded text string
 * Useful for debugging context issues and hallucination problems
 *
 * @param governor Governor instance
 * @return Malloc'd string containing decoded KV cache content (caller must free), or NULL if tracking disabled or model not loaded
 */
char* ethervox_governor_get_kv_cache_contents(ethervox_governor_t* governor);

/**
 * Execute user query with tool orchestration
 *
 * @param governor Governor instance
 * @param user_query User's natural language query
 * @param response Output: Final response (caller must free)
 * @param error Output: Error message if failed (caller must free)
 * @param metrics Output: Confidence metrics (optional, can be NULL)
 * @param progress_callback Progress callback for UI updates (optional, can be NULL)
 * @param token_callback Token-by-token streaming callback (optional, can be NULL)
 * @param user_data User data passed to callbacks (optional, can be NULL)
 * @return Governor status
 */
ethervox_governor_status_t ethervox_governor_execute(
    ethervox_governor_t* governor, const char* user_query, char** response, char** error,
    ethervox_confidence_metrics_t* metrics, ethervox_governor_progress_callback progress_callback,
    void (*token_callback)(const char* token, void* user_data), void* user_data);

/**
 * Execute user query with execution context for conversational awareness
 *
 * Extended version of ethervox_governor_execute() that includes execution context
 * for conversational AI features like voice interaction, turn-taking, and interrupts.
 *
 * @param governor Governor instance
 * @param user_query User's natural language query
 * @param exec_context Execution context (input source, capabilities, callbacks) - can be NULL for
 * legacy behavior
 * @param response Output: Final response (caller must free)
 * @param error Output: Error message if failed (caller must free)
 * @param metrics Output: Confidence metrics (optional, can be NULL)
 * @param progress_callback Progress callback for UI updates (optional, can be NULL)
 * @param token_callback Token-by-token streaming callback (optional, can be NULL)
 * @param user_data User data passed to callbacks (optional, can be NULL)
 * @return Governor status
 */
ethervox_governor_status_t ethervox_governor_execute_with_context(
    ethervox_governor_t* governor, const char* user_query,
    const ethervox_execution_context_t* exec_context, char** response, char** error,
    ethervox_confidence_metrics_t* metrics, ethervox_governor_progress_callback progress_callback,
    void (*token_callback)(const char* token, void* user_data), void* user_data);
/**
 * Get iteration count from last execution (for debugging)
 */
uint32_t ethervox_governor_get_last_iteration_count(ethervox_governor_t* governor);

/**
 * Reset conversation history (clears KV cache back to system prompt)
 *
 * This is useful for testing or when you want to start a fresh conversation
 * while keeping the model loaded and system prompt intact.
 *
 * @param governor Governor instance
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_reset_conversation(ethervox_governor_t* governor);

/**
 * Manually trigger conversation summarization and KV cache clearing
 *
 * Generates an LLM-based summary of recent conversation history, stores it in memory,
 * then clears the KV cache back to the system prompt. Useful for testing or when
 * you want to free up context space manually.
 *
 * @param governor Governor instance
 * @param force_clear If true, clears cache regardless of usage level
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_summarize_and_clear_cache(ethervox_governor_t* governor,
                                                              bool force_clear);

/**
 * Enable or disable tool call execution
 * When disabled, tool calls in LLM responses will be extracted but not executed.
 * This is useful for optimization processes that need raw tool call templates.
 *
 * @param governor Governor instance
 * @param enabled true to enable tool execution (default), false to disable
 */
void ethervox_governor_set_tool_execution(ethervox_governor_t* governor, bool enabled);

/**
 * Cleanup Governor (frees resources)
 */
void ethervox_governor_cleanup(ethervox_governor_t* governor);

// ============================================================================
// Default Configuration
// ============================================================================

static inline ethervox_governor_config_t ethervox_governor_default_config(void) {
  ethervox_governor_config_t config = {
      .confidence_threshold = 0.85f,
      .max_iterations = 5,
      .max_tool_calls_per_iteration = 10,
      .timeout_seconds = 30,
      .max_tokens_per_response = 192,  // Conservative limit for voice assistant responses (was 2048 - too verbose)
      .gpu_layers = 999,     // Full GPU offload by default (overridden by settings)
      .context_size = 768,   // Optimized for mobile (was 8192 - too high for 350M models)
      .n_threads = 8,        // 8 threads by default (overridden by settings)
      .temperature = 0.7f,   // Balanced creativity (overridden by settings)
      .system_prompt_mode = ETHERVOX_GOVERNOR_MODE_FULL,  // Default to full capabilities
      .disable_memory_logging = false,                    // Default to normal memory logging
      .enable_tool_prefilling = false                     // Disabled by default (confuses trained models)
  };
  return config;
}

/**
 * Get the tool registry from Governor
 *
 * @param governor Governor instance
 * @return Tool registry pointer, or NULL if invalid
 */
ethervox_tool_registry_t* ethervox_governor_get_registry(ethervox_governor_t* governor);

/**
 * Set the manifest registry for Governor (enables optimized system prompts)
 *
 * Must be called BEFORE loading the model to use optimized prompts.
 * If called after model load, will take effect on next model load.
 *
 * @param governor Governor instance
 * @param manifest Manifest registry pointer (can be NULL to disable)
 */
void ethervox_governor_set_manifest(ethervox_governor_t* governor, tool_manifest_registry_t* manifest);

/**
 * Request interruption of ongoing generation
 *
 * Sets an interrupt flag that will be checked at the start of each iteration.
 * Safe to call from signal handlers or conversation interrupt callbacks.
 *
 * @param governor Governor instance
 */
void ethervox_governor_request_interrupt(ethervox_governor_t* governor);

/**
 * Update Governor runtime configuration (for settings changes)
 *
 * @param governor Governor instance
 * @param config New configuration to apply
 * @return 0 on success, -1 on error
 */
ethervox_result_t ethervox_governor_update_config(ethervox_governor_t* governor,
                                                  const ethervox_governor_config_t* config);

/**
 * Get chat template from Governor instance
 *
 * @param governor Governor instance
 * @return Chat template pointer, or NULL if invalid
 */
const chat_template_t* ethervox_governor_get_chat_template(ethervox_governor_t* governor);

// ============================================================================
// Tool Manifest System Integration (NEW)
// ============================================================================

/**
 * Initialize governor with Tool Manifest System
 *
 * Implements complete initialization with 4-level fallback:
 * - Level 0: Optimized JSON prompts (~150 tokens)
 * - Level 1: Binary one-liners (~500 tokens)
 * - Level 2: LLM-only mode (0 tokens)
 * - Level 3: Emergency mode (/quit, /help only)
 *
 * @param governor Governor instance
 * @param model_path Path to GGUF model file
 * @param manifest_registry Output: Tool manifest registry
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_governor_init_with_manifest(ethervox_governor_t* governor,
                                                       const char* model_path,
                                                       tool_manifest_registry_t* manifest_registry);

/**
 * Setup manifest registry for a loaded governor model (convenience wrapper)
 *
 * This is a higher-level helper that allocates and initializes the manifest
 * registry, centralizing the pattern used by both desktop and Android platforms.
 *
 * @param governor Governor instance (must have model loaded)
 * @param model_path Path to model file (for finding manifest)
 * @param manifest_out Receives allocated manifest on success (caller must free)
 * @return 0=success, negative=error
 */
ethervox_result_t ethervox_governor_setup_manifest(ethervox_governor_t* governor,
                                                   const char* model_path,
                                                   tool_manifest_registry_t** manifest_out);

/**
 * Build system prompt using Tool Manifest System
 *
 * Generates minimal prompt (~150 tokens) instead of full schemas (~15K tokens).
 *
 * @param manifest_registry Tool manifest registry
 * @param chat_template Chat template for model-specific formatting
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Number of bytes written, or negative on error
 */
ethervox_result_t ethervox_governor_build_system_prompt_with_manifest(
    const tool_manifest_registry_t* manifest_registry,
    const chat_template_t* chat_template,
    char* output,
    size_t output_size);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_GOVERNOR_H
