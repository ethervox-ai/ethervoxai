/**
 * @file llm.h
 * @brief LLM backend interface definitions for EthervoxAI
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
#ifndef ETHERVOX_LLM_H
#define ETHERVOX_LLM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ethervox/config.h"
#include "ethervox/dialogue.h"

#ifdef __cplusplus
extern "C" {
#endif

// LLM backend types
typedef enum {
  ETHERVOX_LLM_BACKEND_NONE = 0,
  ETHERVOX_LLM_BACKEND_LLAMA,      // llama.cpp backend
  ETHERVOX_LLM_BACKEND_TINYLLAMA,  // TinyLlama optimized
  ETHERVOX_LLM_BACKEND_EXTERNAL,   // External API (OpenAI, etc)
  ETHERVOX_LLM_BACKEND_COUNT
} ethervox_llm_backend_type_t;

// LLM backend capabilities
typedef struct {
  bool supports_streaming;
  bool supports_gpu;
  bool supports_quantization;
  bool supports_context_caching;
  uint32_t max_context_length;
  uint32_t recommended_context_length;
  uint32_t max_batch_size;
  const char* model_format;  // "GGUF", "GGML", "API", etc.
} ethervox_llm_capabilities_t;

// LLM backend interface
typedef struct ethervox_llm_backend {
  ethervox_llm_backend_type_t type;
  const char* name;
  void* handle;  // Backend-specific handle
  
  // Function pointers for backend operations
  int (*init)(struct ethervox_llm_backend* backend, const ethervox_llm_config_t* config);
  void (*cleanup)(struct ethervox_llm_backend* backend);
  
  int (*load_model)(struct ethervox_llm_backend* backend, const char* model_path);
  void (*unload_model)(struct ethervox_llm_backend* backend);
  
  int (*generate)(struct ethervox_llm_backend* backend, 
                  const char* prompt,
                  const char* language_code,
                  ethervox_llm_response_t* response);
  
  int (*get_capabilities)(struct ethervox_llm_backend* backend,
                         ethervox_llm_capabilities_t* capabilities);
  
  // Optional streaming support
  int (*generate_stream)(struct ethervox_llm_backend* backend,
                        const char* prompt,
                        const char* language_code,
                        void (*callback)(const char* token, void* user_data),
                        void* user_data);
  
  bool is_loaded;
  bool is_initialized;
} ethervox_llm_backend_t;

// Backend creation functions
ethervox_llm_backend_t* ethervox_llm_create_llama_backend(void);
ethervox_llm_backend_t* ethervox_llm_create_tinyllama_backend(void);
ethervox_llm_backend_t* ethervox_llm_create_external_backend(void);

// Backend management
int ethervox_llm_backend_init(ethervox_llm_backend_t* backend, const ethervox_llm_config_t* config);
void ethervox_llm_backend_cleanup(ethervox_llm_backend_t* backend);
void ethervox_llm_backend_free(ethervox_llm_backend_t* backend);

// Model operations
int ethervox_llm_backend_load_model(ethervox_llm_backend_t* backend, const char* model_path);
void ethervox_llm_backend_unload_model(ethervox_llm_backend_t* backend);

// Generation
int ethervox_llm_backend_generate(ethervox_llm_backend_t* backend,
                                 const char* prompt,
                                 const char* language_code,
                                 ethervox_llm_response_t* response);

// Utility functions
const char* ethervox_llm_backend_type_to_string(ethervox_llm_backend_type_t type);
bool ethervox_llm_backend_is_available(ethervox_llm_backend_type_t type);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_LLM_H
