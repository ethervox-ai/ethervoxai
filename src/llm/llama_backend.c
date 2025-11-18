/**
 * @file llama_backend.c
 * @brief Llama.cpp backend implementation for EthervoxAI
 *
 * This backend integrates llama.cpp for running GGUF models locally.
 * Supports quantized models, GPU acceleration, and context caching.
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
#include "ethervox/llm.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef ETHERVOX_WITH_LLAMA
// Include llama.cpp headers if available
#include <llama.h>
#endif

// Default configuration values
#define LLAMA_DEFAULT_CONTEXT_LENGTH 2048
#define LLAMA_DEFAULT_MAX_TOKENS 512
#define LLAMA_DEFAULT_TEMPERATURE 0.7f
#define LLAMA_DEFAULT_TOP_P 0.9f
#define LLAMA_DEFAULT_GPU_LAYERS 0
#define LLAMA_DEFAULT_THREADS 4
#define LLAMA_MAX_RESPONSE_LENGTH 4096

// Llama backend context
typedef struct {
#ifdef ETHERVOX_WITH_LLAMA
  struct llama_model* model;
  struct llama_context* ctx;
  struct llama_context_params ctx_params;
  struct llama_model_params model_params;
#else
  void* model;  // Placeholder when llama.cpp not available
  void* ctx;
#endif
  
  // Configuration
  uint32_t n_ctx;
  uint32_t n_predict;
  float temperature;
  float top_p;
  uint32_t n_gpu_layers;
  uint32_t n_threads;
  uint32_t seed;
  
  // State
  char* loaded_model_path;
  bool use_mlock;
  bool use_mmap;
  
} llama_backend_context_t;

// Forward declarations
static int llama_backend_init(ethervox_llm_backend_t* backend, const ethervox_llm_config_t* config);
static void llama_backend_cleanup(ethervox_llm_backend_t* backend);
static int llama_backend_load_model(ethervox_llm_backend_t* backend, const char* model_path);
static void llama_backend_unload_model(ethervox_llm_backend_t* backend);
static int llama_backend_generate(ethervox_llm_backend_t* backend,
                                 const char* prompt,
                                 const char* language_code,
                                 ethervox_llm_response_t* response);
static int llama_backend_get_capabilities(ethervox_llm_backend_t* backend,
                                        ethervox_llm_capabilities_t* capabilities);

// Create Llama backend instance
ethervox_llm_backend_t* ethervox_llm_create_llama_backend(void) {
  ethervox_llm_backend_t* backend = (ethervox_llm_backend_t*)calloc(1, sizeof(ethervox_llm_backend_t));
  if (!backend) {
    ETHERVOX_LOG_ERROR("Failed to allocate Llama backend");
    return NULL;
  }
  
  backend->type = ETHERVOX_LLM_BACKEND_LLAMA;
  backend->name = "Llama.cpp";
  backend->init = llama_backend_init;
  backend->cleanup = llama_backend_cleanup;
  backend->load_model = llama_backend_load_model;
  backend->unload_model = llama_backend_unload_model;
  backend->generate = llama_backend_generate;
  backend->get_capabilities = llama_backend_get_capabilities;
  backend->is_initialized = false;
  backend->is_loaded = false;
  
  return backend;
}

static int llama_backend_init(ethervox_llm_backend_t* backend, const ethervox_llm_config_t* config) {
  if (!backend) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
#ifndef ETHERVOX_WITH_LLAMA
  ETHERVOX_LOG_ERROR("Llama backend not compiled in (missing ETHERVOX_WITH_LLAMA)");
  return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
  
  // Allocate context
  llama_backend_context_t* ctx = (llama_backend_context_t*)calloc(1, sizeof(llama_backend_context_t));
  if (!ctx) {
    ETHERVOX_LOG_ERROR("Failed to allocate Llama context");
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }
  
  // Set configuration
  if (config) {
    ctx->n_ctx = config->context_length > 0 ? config->context_length : LLAMA_DEFAULT_CONTEXT_LENGTH;
    ctx->n_predict = config->max_tokens > 0 ? config->max_tokens : LLAMA_DEFAULT_MAX_TOKENS;
    ctx->temperature = config->temperature > 0.0f ? config->temperature : LLAMA_DEFAULT_TEMPERATURE;
    ctx->top_p = config->top_p > 0.0f ? config->top_p : LLAMA_DEFAULT_TOP_P;
    ctx->n_gpu_layers = config->use_gpu ? config->gpu_layers : LLAMA_DEFAULT_GPU_LAYERS;
    ctx->seed = config->seed > 0 ? config->seed : (uint32_t)time(NULL);
  } else {
    ctx->n_ctx = LLAMA_DEFAULT_CONTEXT_LENGTH;
    ctx->n_predict = LLAMA_DEFAULT_MAX_TOKENS;
    ctx->temperature = LLAMA_DEFAULT_TEMPERATURE;
    ctx->top_p = LLAMA_DEFAULT_TOP_P;
    ctx->n_gpu_layers = LLAMA_DEFAULT_GPU_LAYERS;
    ctx->seed = (uint32_t)time(NULL);
  }
  
  ctx->n_threads = LLAMA_DEFAULT_THREADS;
  ctx->use_mlock = false;
  ctx->use_mmap = true;
  
  // Initialize llama backend
  llama_backend_init();
  
  backend->handle = ctx;
  backend->is_initialized = true;
  
  ETHERVOX_LOG_INFO("Llama backend initialized (ctx=%u, predict=%u, temp=%.2f)",
                    ctx->n_ctx, ctx->n_predict, ctx->temperature);
  
  return ETHERVOX_SUCCESS;
#endif
}

static void llama_backend_cleanup(ethervox_llm_backend_t* backend) {
  if (!backend || !backend->handle) {
    return;
  }
  
#ifdef ETHERVOX_WITH_LLAMA
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  // Unload model if loaded
  if (ctx->ctx) {
    llama_free(ctx->ctx);
    ctx->ctx = NULL;
  }
  
  if (ctx->model) {
    llama_free_model(ctx->model);
    ctx->model = NULL;
  }
  
  if (ctx->loaded_model_path) {
    free(ctx->loaded_model_path);
    ctx->loaded_model_path = NULL;
  }
  
  // Cleanup llama backend
  llama_backend_free();
  
  free(ctx);
  backend->handle = NULL;
  
  ETHERVOX_LOG_INFO("Llama backend cleaned up");
#endif
}

static int llama_backend_load_model(ethervox_llm_backend_t* backend, const char* model_path) {
  if (!backend || !backend->handle || !model_path) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
#ifndef ETHERVOX_WITH_LLAMA
  ETHERVOX_LOG_ERROR("Llama backend not available");
  return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
  
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  // Unload existing model
  if (ctx->model) {
    llama_backend_unload_model(backend);
  }
  
  ETHERVOX_LOG_INFO("Loading Llama model: %s", model_path);
  
  // Initialize model parameters
  ctx->model_params = llama_model_default_params();
  ctx->model_params.n_gpu_layers = ctx->n_gpu_layers;
  ctx->model_params.use_mlock = ctx->use_mlock;
  ctx->model_params.use_mmap = ctx->use_mmap;
  
  // Load model
  ctx->model = llama_load_model_from_file(model_path, ctx->model_params);
  if (!ctx->model) {
    ETHERVOX_LOG_ERROR("Failed to load model from: %s", model_path);
    return ETHERVOX_ERROR_FAILED;
  }
  
  // Initialize context parameters
  ctx->ctx_params = llama_context_default_params();
  ctx->ctx_params.n_ctx = ctx->n_ctx;
  ctx->ctx_params.n_threads = ctx->n_threads;
  ctx->ctx_params.seed = ctx->seed;
  
  // Create context
  ctx->ctx = llama_new_context_with_model(ctx->model, ctx->ctx_params);
  if (!ctx->ctx) {
    ETHERVOX_LOG_ERROR("Failed to create Llama context");
    llama_free_model(ctx->model);
    ctx->model = NULL;
    return ETHERVOX_ERROR_FAILED;
  }
  
  // Save model path
  ctx->loaded_model_path = strdup(model_path);
  backend->is_loaded = true;
  
  ETHERVOX_LOG_INFO("Llama model loaded successfully");
  ETHERVOX_LOG_INFO("Context size: %u, GPU layers: %u", ctx->n_ctx, ctx->n_gpu_layers);
  
  return ETHERVOX_SUCCESS;
#endif
}

static void llama_backend_unload_model(ethervox_llm_backend_t* backend) {
  if (!backend || !backend->handle) {
    return;
  }
  
#ifdef ETHERVOX_WITH_LLAMA
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  if (ctx->ctx) {
    llama_free(ctx->ctx);
    ctx->ctx = NULL;
  }
  
  if (ctx->model) {
    llama_free_model(ctx->model);
    ctx->model = NULL;
  }
  
  if (ctx->loaded_model_path) {
    free(ctx->loaded_model_path);
    ctx->loaded_model_path = NULL;
  }
  
  backend->is_loaded = false;
  
  ETHERVOX_LOG_INFO("Llama model unloaded");
#endif
}

static int llama_backend_generate(ethervox_llm_backend_t* backend,
                                 const char* prompt,
                                 const char* language_code,
                                 ethervox_llm_response_t* response) {
  if (!backend || !backend->handle || !prompt || !response) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
#ifndef ETHERVOX_WITH_LLAMA
  ETHERVOX_LOG_ERROR("Llama backend not available");
  return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
  
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  if (!ctx->ctx || !ctx->model) {
    ETHERVOX_LOG_ERROR("Model not loaded");
    return ETHERVOX_ERROR_NOT_INITIALIZED;
  }
  
  clock_t start_time = clock();
  
  // Tokenize prompt
  const int n_prompt_tokens = -llama_tokenize(ctx->model, prompt, (int)strlen(prompt),
                                              NULL, 0, true, true);
  
  if (n_prompt_tokens < 0) {
    ETHERVOX_LOG_ERROR("Failed to tokenize prompt");
    return ETHERVOX_ERROR_FAILED;
  }
  
  llama_token* prompt_tokens = (llama_token*)malloc(n_prompt_tokens * sizeof(llama_token));
  if (!prompt_tokens) {
    ETHERVOX_LOG_ERROR("Failed to allocate prompt tokens");
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }
  
  int n_tokens = llama_tokenize(ctx->model, prompt, (int)strlen(prompt),
                                prompt_tokens, n_prompt_tokens, true, true);
  
  if (n_tokens < 0 || n_tokens > n_prompt_tokens) {
    ETHERVOX_LOG_ERROR("Tokenization failed");
    free(prompt_tokens);
    return ETHERVOX_ERROR_FAILED;
  }
  
  // Allocate response buffer
  char* response_text = (char*)malloc(LLAMA_MAX_RESPONSE_LENGTH);
  if (!response_text) {
    ETHERVOX_LOG_ERROR("Failed to allocate response buffer");
    free(prompt_tokens);
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }
  
  response_text[0] = '\0';
  size_t response_len = 0;
  
  // Evaluate prompt
  if (llama_decode(ctx->ctx, llama_batch_get_one(prompt_tokens, n_tokens, 0, 0)) != 0) {
    ETHERVOX_LOG_ERROR("Failed to evaluate prompt");
    free(prompt_tokens);
    free(response_text);
    return ETHERVOX_ERROR_FAILED;
  }
  
  free(prompt_tokens);
  
  // Generate tokens
  int n_generated = 0;
  bool finished = false;
  
  for (int i = 0; i < (int)ctx->n_predict && !finished; i++) {
    // Sample next token
    llama_token new_token = llama_sampler_sample(
      llama_sampler_chain_default_params(),
      ctx->ctx,
      0
    );
    
    // Check for end of generation
    if (llama_token_is_eog(ctx->model, new_token)) {
      finished = true;
      break;
    }
    
    // Decode token to text
    char piece[256];
    int n_piece = llama_token_to_piece(ctx->model, new_token, piece, sizeof(piece), false);
    
    if (n_piece > 0) {
      if (response_len + n_piece < LLAMA_MAX_RESPONSE_LENGTH - 1) {
        memcpy(response_text + response_len, piece, n_piece);
        response_len += n_piece;
        response_text[response_len] = '\0';
      }
    }
    
    // Evaluate next token
    if (llama_decode(ctx->ctx, llama_batch_get_one(&new_token, 1, n_tokens + i, 0)) != 0) {
      ETHERVOX_LOG_WARN("Failed to evaluate token at position %d", i);
      break;
    }
    
    n_generated++;
  }
  
  clock_t end_time = clock();
  uint32_t processing_time = (uint32_t)(((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000);
  
  // Fill response structure
  response->text = response_text;
  response->confidence = 0.9f;  // High confidence for local model
  response->processing_time_ms = processing_time;
  response->token_count = n_generated;
  response->requires_external_llm = false;
  response->external_llm_prompt = NULL;
  response->model_name = strdup(ctx->loaded_model_path ? ctx->loaded_model_path : "llama");
  response->truncated = !finished;
  response->finish_reason = finished ? strdup("stop") : strdup("length");
  
  if (language_code) {
    strncpy(response->language_code, language_code, ETHERVOX_LANG_CODE_LEN - 1);
    response->language_code[ETHERVOX_LANG_CODE_LEN - 1] = '\0';
  } else {
    strncpy(response->language_code, "en", ETHERVOX_LANG_CODE_LEN - 1);
  }
  
  ETHERVOX_LOG_INFO("Generated %d tokens in %u ms", n_generated, processing_time);
  
  return ETHERVOX_SUCCESS;
#endif
}

static int llama_backend_get_capabilities(ethervox_llm_backend_t* backend,
                                        ethervox_llm_capabilities_t* capabilities) {
  if (!backend || !capabilities) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  capabilities->supports_streaming = false;  // TODO: Implement streaming
  capabilities->supports_gpu = true;
  capabilities->supports_quantization = true;
  capabilities->supports_context_caching = true;
  capabilities->max_context_length = ctx ? ctx->n_ctx : LLAMA_DEFAULT_CONTEXT_LENGTH;
  capabilities->recommended_context_length = 2048;
  capabilities->max_batch_size = 512;
  capabilities->model_format = "GGUF";
  
  return ETHERVOX_SUCCESS;
}
