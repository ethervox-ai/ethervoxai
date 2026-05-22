/**
 * @file dialogue.h
 * @brief Dialogue processing interface definitions for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 *
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */
#ifndef ETHERVOX_DIALOGUE_H
#define ETHERVOX_DIALOGUE_H

#include <stdbool.h>
#include <stddef.h>  // For size_t
#include <stdint.h>

#include "ethervox/config.h"
#include "ethervox/error.h"
#include "ethervox/governor.h"  // For ethervox_governor_progress_callback type

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct ethervox_tts_context ethervox_tts_context_t;

// Intent types
typedef enum {
  ETHERVOX_INTENT_UNKNOWN = 0,
  ETHERVOX_INTENT_GREETING,
  ETHERVOX_INTENT_QUESTION,
  ETHERVOX_INTENT_COMMAND,
  ETHERVOX_INTENT_REQUEST,
  ETHERVOX_INTENT_INFORMATION,
  ETHERVOX_INTENT_CONTROL,
  ETHERVOX_INTENT_GOODBYE,
  ETHERVOX_INTENT_COUNT
} ethervox_intent_type_t;

// Entity types
typedef enum {
  ETHERVOX_ENTITY_PERSON = 0,
  ETHERVOX_ENTITY_LOCATION,
  ETHERVOX_ENTITY_TIME,
  ETHERVOX_ENTITY_NUMBER,
  ETHERVOX_ENTITY_DEVICE,
  ETHERVOX_ENTITY_ACTION,
  ETHERVOX_ENTITY_COUNT
} ethervox_entity_type_t;

// Entity structure
typedef struct {
  ethervox_entity_type_t type;
  char* value;
  char* normalized_value;
  float confidence;
  uint32_t start_pos;
  uint32_t end_pos;
} ethervox_entity_t;

// Intent result
typedef struct {
  ethervox_intent_type_t type;
  char* raw_text;
  char* normalized_text;
  char language_code[ETHERVOX_LANG_CODE_LEN];
  float confidence;
  ethervox_entity_t* entities;
  uint32_t entity_count;
  void* context;  // Additional context data
} ethervox_intent_t;

// LLM model configuration
typedef struct {
  char* model_path;
  char* model_name;
  uint32_t max_tokens;
  uint32_t context_length;
  float temperature;
  float top_p;
  uint32_t seed;
  bool use_gpu;
  uint32_t gpu_layers;
  char* language_code;
} ethervox_llm_config_t;

// LLM response
typedef struct {
  char* text;
  char* user_prompt_punctuated;  // User's input with smart punctuation applied
  char language_code[ETHERVOX_LANG_CODE_LEN];
  float confidence;
  uint32_t processing_time_ms;
  uint32_t token_count;
  bool requires_external_llm;
  char* external_llm_prompt;
  char* model_name;
  bool truncated;
  char* finish_reason;
  size_t tokens_used;  // Add if missing
  bool conversation_ended;  // True when conversation should end (e.g., goodbye)
} ethervox_llm_response_t;

// Dialogue context
typedef struct {
  char* conversation_id;        // Internal unique ID
  char* display_name;           // User-friendly display name for UI
  char* user_id;
  char current_language[ETHERVOX_LANG_CODE_LEN];
  ethervox_intent_t* conversation_history;
  uint32_t history_count;
  uint32_t max_history;
  void* user_preferences;
  uint64_t last_interaction_time;
} ethervox_dialogue_context_t;

// External LLM callback function type
typedef int (*ethervox_external_llm_callback_t)(const char* prompt, const char* language_code,
                                                ethervox_llm_response_t* response, void* user_data);

// Dialogue engine structure
typedef struct {
  ethervox_llm_config_t llm_config;
  void* llm_model;  // Internal LLM model handle
  void* llm_backend; // LLM backend instance (ethervox_llm_backend_t*)
  bool use_llm_for_unknown; // Route unknown intents to LLM
  ethervox_dialogue_context_t* contexts;
  uint32_t max_contexts;
  uint32_t active_contexts;

  // External LLM integration
  ethervox_external_llm_callback_t external_llm_callback;
  void* external_llm_user_data;
  bool prefer_external_llm;

  // Governor integration (tool-aware LLM orchestration)
  void* governor;  // ethervox_governor_t*
  void* governor_tool_registry;  // ethervox_tool_registry_t*
  void* manifest_registry;  // tool_manifest_registry_t* (binary manifest + optimized prompts cache)
  bool use_governor;  // Enable Governor for complex queries

  // Intent parsing patterns
  void* intent_patterns;  // Platform-specific pattern storage

  // Callbacks
  void (*on_intent_detected)(const ethervox_intent_t* intent, void* user_data);
  void (*on_response_ready)(const ethervox_llm_response_t* response, void* user_data);
  void (*on_external_llm_required)(const char* prompt, const char* language, void* user_data);
  void* callback_user_data;

  bool is_initialized;
} ethervox_dialogue_engine_t;

typedef struct {
  const char* text;
  const char* language_code;
} ethervox_dialogue_intent_request_t;

typedef struct {
  const char* user_id;
  const char* language_code;
} ethervox_dialogue_context_request_t;

// Public API functions
ethervox_result_t ethervox_dialogue_init(ethervox_dialogue_engine_t* engine, const ethervox_llm_config_t* config);
void ethervox_dialogue_cleanup(ethervox_dialogue_engine_t* engine);

// Intent processing
ethervox_result_t ethervox_dialogue_parse_intent(ethervox_dialogue_engine_t* engine,
                                   const ethervox_dialogue_intent_request_t* request,
                                   ethervox_intent_t* intent);

// LLM processing
ethervox_result_t ethervox_dialogue_process_llm(ethervox_dialogue_engine_t* engine,
                                  const ethervox_intent_t* intent, const char* context_id,
                                  ethervox_llm_response_t* response);

// LLM processing with streaming
ethervox_result_t ethervox_dialogue_process_llm_stream(ethervox_dialogue_engine_t* engine,
                                         const ethervox_intent_t* intent,
                                         const ethervox_dialogue_context_t* context,
                                         void (*token_callback)(const char* token, void* user_data),
                                         void* user_data,
                                         bool* conversation_ended,
                                         ethervox_governor_progress_callback governor_progress_callback);

// Context management
ethervox_result_t ethervox_dialogue_create_context(ethervox_dialogue_engine_t* engine,
                                     const ethervox_dialogue_context_request_t* request,
                                     char** context_id);
ethervox_result_t ethervox_dialogue_get_context(ethervox_dialogue_engine_t* engine, const char* context_id,
                                  ethervox_dialogue_context_t** context);
void ethervox_dialogue_destroy_context(ethervox_dialogue_engine_t* engine, const char* context_id);

/**
 * Get the display name for a conversation context
 * @param engine Dialogue engine
 * @param context_id Conversation context ID
 * @return Display name string (owned by context, do not free), or NULL if not found
 */
const char* ethervox_dialogue_get_display_name(ethervox_dialogue_engine_t* engine, 
                                               const char* context_id);

/**
 * Rename a conversation context
 * @param engine Dialogue engine
 * @param context_id Conversation context ID
 * @param new_name New display name for the conversation
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_dialogue_rename_context(ethervox_dialogue_engine_t* engine,
                                                   const char* context_id,
                                                   const char* new_name);

ethervox_result_t ethervox_dialogue_set_language(ethervox_dialogue_engine_t* engine, const char* language_code);
const char* ethervox_dialogue_get_language(const ethervox_dialogue_engine_t* engine);

// External LLM integration
void ethervox_dialogue_set_external_llm_callback(ethervox_dialogue_engine_t* engine,
                                                 ethervox_external_llm_callback_t callback,
                                                 void* user_data);

// Memory store integration (forward declaration)
struct ethervox_memory_store_t;
void ethervox_dialogue_set_memory_store(struct ethervox_memory_store_t* store);

// Tool manifest integration (forward declaration)
struct tool_manifest_registry;
ethervox_result_t ethervox_dialogue_reload_manifest(ethervox_dialogue_engine_t* engine, const char* model_path);

// Utility functions
ethervox_llm_config_t ethervox_dialogue_get_default_llm_config(void);
const char* ethervox_dialogue_detect_system_language(void);
void ethervox_intent_free(ethervox_intent_t* intent);
void ethervox_llm_response_free(ethervox_llm_response_t* response);
const char* ethervox_intent_type_to_string(ethervox_intent_type_t type);
const char* ethervox_entity_type_to_string(ethervox_entity_type_t type);

// Language support
bool ethervox_dialogue_is_language_supported(const char* language_code);
ethervox_result_t ethervox_dialogue_add_language_support(ethervox_dialogue_engine_t* engine,
                                           const char* language_code);
const char** ethervox_dialogue_get_supported_languages(void);
/**
 * Get the global TTS instance (used by speak tool and conversation)
 * @return Global TTS context (may be NULL if not initialized)
 */
ethervox_tts_context_t* ethervox_get_global_tts(void);

/**
 * Reload the global TTS instance with new settings
 * @param tts_settings New TTS settings to apply
 * @param chunk_callback Optional audio chunk callback for streaming (NULL to disable)
 * @param callback_user_data User data to pass to callback
 * @return 0 on success, -1 on failure
 */
ethervox_result_t ethervox_reload_global_tts(const void* tts_settings,
                               void (*chunk_callback)(const float*, size_t, void*),
                               void* callback_user_data);
#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_DIALOGUE_H