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
#include "ethervox/config.h"
#include "ethervox/device_profile.h"
#include "ethervox/gguf_config_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef ETHERVOX_PLATFORM_ANDROID
#include <android/log.h>
#define LLAMA_LOG(...) __android_log_print(ANDROID_LOG_INFO, "EthervoxLlama", __VA_ARGS__)
#define LLAMA_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, "EthervoxLlama", __VA_ARGS__)
#else
#define LLAMA_LOG(...) printf(__VA_ARGS__); printf("\n")
#define LLAMA_ERROR(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#endif

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
// Include llama.cpp headers
#include <llama.h>
#define LLAMA_HEADER_AVAILABLE 1
#else
#define LLAMA_HEADER_AVAILABLE 0
#endif

// Default configuration values - ULTRA PERFORMANCE MODE
#define LLAMA_DEFAULT_CONTEXT_LENGTH 2048  // Maximum context for best responses
#define LLAMA_DEFAULT_MAX_TOKENS 4095  // Shorter responses for voice interactions
#define LLAMA_DEFAULT_TEMPERATURE 0.7f
#define LLAMA_DEFAULT_TOP_P 0.9f
#define LLAMA_DEFAULT_GPU_LAYERS 99  // Offload everything to GPU for maximum speed
#define LLAMA_DEFAULT_THREADS 8  // Optimal CPU threads (more isn't always better)
#define LLAMA_DEFAULT_BATCH_SIZE 2048  // Massive batch for maximum throughput
#define LLAMA_PROMPT_BATCH_SIZE 2048  // Maximum batch size for prompt processing
#define LLAMA_MAX_RESPONSE_LENGTH 4096

// Global backend state guard to prevent multiple initialization
static bool g_llama_backend_initialized = false;
static int g_llama_backend_refcount = 0;

// Llama backend context
typedef struct {
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
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
  volatile bool cancel_requested;  // Flag to cancel generation
  
} llama_backend_context_t;

// Helper function to detect optimal thread count
static uint32_t get_optimal_thread_count(void) {
  uint32_t cpu_count = 0;
  
#ifdef _WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  cpu_count = sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
  // POSIX and Android
  long nproc = sysconf(_SC_NPROCESSORS_ONLN);
  if (nproc > 0) {
    cpu_count = (uint32_t)nproc;
  }
#endif
  
  // Fallback to 4 threads if detection fails
  if (cpu_count == 0) {
    LLAMA_LOG("Failed to detect CPU count, using 4 threads");
    return 4;
  }
  
  // Use all available cores, but cap at 16 to avoid diminishing returns
  // For most LLM workloads, more than 16 threads doesn't help much
  uint32_t optimal = cpu_count;
  if (optimal > 16) {
    optimal = 16;
  }
  
  LLAMA_LOG("Detected CPU cores and threads: %u cores, %u threads", (unsigned int)cpu_count, (unsigned int)optimal);
  return optimal;
}

// Forward declarations
static ethervox_result_t ethervox_llama_backend_init(ethervox_llm_backend_t* backend, const ethervox_llm_config_t* config);
static void llama_backend_cleanup(ethervox_llm_backend_t* backend);
static ethervox_result_t llama_backend_load_model(ethervox_llm_backend_t* backend, const char* model_path);
static void llama_backend_unload_model(ethervox_llm_backend_t* backend);
static ethervox_result_t llama_backend_generate(ethervox_llm_backend_t* backend,
                                 const char* prompt,
                                 const char* language_code,
                                 ethervox_llm_response_t* response);
static ethervox_result_t llama_backend_generate_stream(ethervox_llm_backend_t* backend,
                                         const char* prompt,
                                         const char* language_code,
                                         void (*callback)(const char* token, void* user_data),
                                         void* user_data);
static ethervox_result_t llama_backend_get_capabilities(ethervox_llm_backend_t* backend,
                                        ethervox_llm_capabilities_t* capabilities);
static ethervox_result_t llama_backend_update_config(ethervox_llm_backend_t* backend,
                                       const ethervox_llm_config_t* config);

// Update runtime generation parameters (can be changed without reloading model)
static ethervox_result_t llama_backend_update_config(ethervox_llm_backend_t* backend,
                                       const ethervox_llm_config_t* config) {
  if (!backend || !backend->handle || !config) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  // Update runtime parameters (don't require model reload)
  if (config->max_tokens > 0) {
    ctx->n_predict = config->max_tokens;
  }
  if (config->temperature > 0.0f) {
    ctx->temperature = config->temperature;
  }
  if (config->top_p > 0.0f) {
    ctx->top_p = config->top_p;
  }
  
  ETHERVOX_LOG_INFO("Updated LLM params: max_tokens=%u, temp=%.2f, top_p=%.2f",
                    ctx->n_predict, ctx->temperature, ctx->top_p);
  
  return ETHERVOX_SUCCESS;
}

/**
 * @brief Force reset llama backend global state (use after crashes/segfaults)
 */
void ethervox_llama_backend_force_reset(void) {
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  ETHERVOX_LOG_WARN("Force resetting llama backend state (refcount=%d)", g_llama_backend_refcount);
  if (g_llama_backend_initialized) {
    llama_backend_free();
    g_llama_backend_initialized = false;
  }
  g_llama_backend_refcount = 0;
  ETHERVOX_LOG_INFO("Llama backend state reset complete");
#endif
}

// Create Llama backend instance
ethervox_llm_backend_t* ethervox_llm_create_llama_backend(void) {
  ethervox_llm_backend_t* backend = (ethervox_llm_backend_t*)calloc(1, sizeof(ethervox_llm_backend_t));
  if (!backend) {
    ETHERVOX_LOG_ERROR("Failed to allocate Llama backend");
    return NULL;
  }
  
  backend->type = ETHERVOX_LLM_BACKEND_LLAMA;
  backend->name = "Llama.cpp";
  backend->init = ethervox_llama_backend_init;
  backend->cleanup = llama_backend_cleanup;
  backend->load_model = llama_backend_load_model;
  backend->unload_model = llama_backend_unload_model;
  backend->generate = llama_backend_generate;
  backend->generate_stream = llama_backend_generate_stream;
  backend->get_capabilities = llama_backend_get_capabilities;
  backend->update_config = llama_backend_update_config;
  backend->is_initialized = false;
  backend->is_loaded = false;
  
  return backend;
}

static ethervox_result_t ethervox_llama_backend_init(ethervox_llm_backend_t* backend, const ethervox_llm_config_t* config) {
  if (!backend) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  ETHERVOX_LOG_ERROR("Llama backend not available - llama.cpp library not compiled in or header not found");
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
    // GPU layers: if use_gpu is false, use 0 layers (CPU only), otherwise use specified or default
    ctx->n_gpu_layers = config->use_gpu ? (config->gpu_layers > 0 ? config->gpu_layers : LLAMA_DEFAULT_GPU_LAYERS) : 0;
    ctx->seed = config->seed > 0 ? config->seed : (uint32_t)time(NULL);
  } else {
    // No config provided - use defaults with GPU enabled
    ctx->n_ctx = LLAMA_DEFAULT_CONTEXT_LENGTH;
    ctx->n_predict = LLAMA_DEFAULT_MAX_TOKENS;
    ctx->temperature = LLAMA_DEFAULT_TEMPERATURE;
    ctx->top_p = LLAMA_DEFAULT_TOP_P;
    ctx->n_gpu_layers = LLAMA_DEFAULT_GPU_LAYERS;
    ctx->seed = (uint32_t)time(NULL);
  }
  
  ctx->n_threads = LLAMA_DEFAULT_THREADS;
  
  // Platform-specific memory settings for optimal performance
#if defined(ETHERVOX_PLATFORM_MACOS) || defined(ETHERVOX_PLATFORM_LINUX) || defined(ETHERVOX_PLATFORM_WINDOWS)
  // Desktop: Use mmap for fast startup, optional mlock if lots of RAM
  ctx->use_mmap = true;    // Memory-map file for fast lazy loading
  ctx->use_mlock = false;  // Don't lock (prevents swapping but needs lots of RAM)
#elif defined(ETHERVOX_PLATFORM_IOS)
  // iOS: mmap not supported, must preload model
  ctx->use_mmap = false;   // iOS doesn't support mmap for model files
  ctx->use_mlock = false;
#elif defined(ETHERVOX_PLATFORM_ANDROID)
  // Android: Use mmap for battery efficiency
  ctx->use_mmap = true;
  ctx->use_mlock = false;
#else
  // Embedded: Preload if enough RAM, otherwise use mmap
  ctx->use_mmap = true;
  ctx->use_mlock = false;
#endif
  
  ctx->cancel_requested = false;
  
  // Initialize device profiling for adaptive configuration
  ETHERVOX_LOG_INFO("[Adaptive Config] Initializing device profile...");
  ethervox_device_profile_init();
  ETHERVOX_LOG_INFO("[Adaptive Config] Device profile initialized successfully");
  
  // Initialize llama backend (global initialization) - only once
  if (!g_llama_backend_initialized) {
    ETHERVOX_LOG_INFO("Initializing llama.cpp backend (first time)");
    llama_backend_init();
    g_llama_backend_initialized = true;
  }
  g_llama_backend_refcount++;
  ETHERVOX_LOG_DEBUG("Llama backend refcount: %d", g_llama_backend_refcount);
  
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

#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;  // Unload model if loaded
  if (ctx->ctx) {
    llama_free(ctx->ctx);
    ctx->ctx = NULL;
  }
  
  if (ctx->model) {
    llama_model_free(ctx->model);
    ctx->model = NULL;
  }
  
  if (ctx->loaded_model_path) {
    free(ctx->loaded_model_path);
    ctx->loaded_model_path = NULL;
  }
  
  // Cleanup llama backend - only free when last reference is gone
  g_llama_backend_refcount--;
  ETHERVOX_LOG_DEBUG("Llama backend refcount: %d", g_llama_backend_refcount);
  if (g_llama_backend_refcount <= 0) {
    ETHERVOX_LOG_INFO("Freeing llama.cpp backend (last reference)");
    llama_backend_free();
    g_llama_backend_initialized = false;
    g_llama_backend_refcount = 0;
  }
  
  free(ctx);
  backend->handle = NULL;
  
  ETHERVOX_LOG_INFO("Llama backend cleaned up");
#else
  // Just free the stub context
  free(backend->handle);
  backend->handle = NULL;
#endif
}

static ethervox_result_t llama_backend_load_model(ethervox_llm_backend_t* backend, const char* model_path) {
  if (!backend || !backend->handle || !model_path) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
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
  
  ETHERVOX_LOG_INFO("Loading model with %d GPU layers requested (mmap=%s, mlock=%s)", 
                    ctx->n_gpu_layers, 
                    ctx->use_mmap ? "true" : "false",
                    ctx->use_mlock ? "true" : "false");
  
  // Load model
  ctx->model = llama_model_load_from_file(model_path, ctx->model_params);
  if (!ctx->model) {
    ETHERVOX_LOG_ERROR("Failed to load model from: %s", model_path);
    return ETHERVOX_ERROR_FAILED;
  }
  
  // Check if GPU layers exceeds actual model layers and reload if necessary
  int32_t n_layer = llama_model_n_layer(ctx->model);
  ETHERVOX_LOG_INFO("Model reports %d layers total", n_layer);
  if (ctx->n_gpu_layers > n_layer) {
    ETHERVOX_LOG_WARN("GPU layers setting (%d) exceeds model layers (%d), reloading with corrected value %d", 
                      ctx->n_gpu_layers, n_layer, n_layer);
    
    // Free the model and reload with correct GPU layers
    llama_model_free(ctx->model);
    ctx->n_gpu_layers = n_layer;
    ctx->model_params.n_gpu_layers = n_layer;
    
    ctx->model = llama_model_load_from_file(model_path, ctx->model_params);
    if (!ctx->model) {
      ETHERVOX_LOG_ERROR("Failed to reload model from: %s", model_path);
      return ETHERVOX_ERROR_FAILED;
    }
  }
  
  ETHERVOX_LOG_INFO("Model has %d layers, using %d GPU layers", n_layer, ctx->n_gpu_layers);
  
  // === ADAPTIVE CONFIGURATION ===
  // Extract model metadata and calculate optimal settings based on device capabilities
  ETHERVOX_LOG_INFO("[Adaptive Config] Extracting model metadata...");
  
  ethervox_model_metadata_t metadata;
  bool adaptive_success = ethervox_extract_model_metadata(ctx->model, &metadata);
  
  if (adaptive_success) {
    ETHERVOX_LOG_INFO("[Adaptive Config] Model metadata extracted: context=%u, layers=%u, size=%zu bytes",
                      metadata.context_length_train, metadata.layer_count, metadata.model_size_bytes);
    
    // Calculate optimal configuration
    ethervox_runtime_config_t optimal;
    if (ethervox_calculate_optimal_config(&metadata, &optimal)) {
      ETHERVOX_LOG_INFO("[Adaptive Config] Optimal settings calculated:");
      ETHERVOX_LOG_INFO("  Context: %u (model trained: %u)", optimal.context_length, metadata.context_length_train);
      ETHERVOX_LOG_INFO("  Batch size: %u", optimal.batch_size);
      ETHERVOX_LOG_INFO("  Threads: %u", optimal.threads);
      ETHERVOX_LOG_INFO("  KV cache type: %d", optimal.kv_cache_type);
      ETHERVOX_LOG_INFO("  Flash attention: %s", optimal.use_flash_attention ? "enabled" : "disabled");
      ETHERVOX_LOG_INFO("  GPU layers: %u", optimal.gpu_layers);
      
      // Apply adaptive settings
      ctx->n_ctx = optimal.context_length;
      ctx->n_threads = optimal.threads;
      // Note: batch_size and cache_type applied below in ctx_params
      
      ETHERVOX_LOG_INFO("[Adaptive Config] ✓ Using adaptive configuration");
    } else {
      ETHERVOX_LOG_WARN("[Adaptive Config] Failed to calculate optimal config, using config.h defaults");
      adaptive_success = false;
    }
  } else {
    ETHERVOX_LOG_WARN("[Adaptive Config] Failed to extract model metadata, using config.h defaults");
  }
  
  // Fallback to config.h if adaptive configuration failed
  if (!adaptive_success) {
    ETHERVOX_LOG_INFO("[Adaptive Config] → FALLBACK: Using hardcoded config.h values");
    ETHERVOX_LOG_INFO("  Context: %u (from LLAMA_DEFAULT_CONTEXT_LENGTH)", ctx->n_ctx);
    ETHERVOX_LOG_INFO("  Threads: %u (from LLAMA_DEFAULT_THREADS)", ctx->n_threads);
    ETHERVOX_LOG_INFO("  Batch: %d (from LLAMA_PROMPT_BATCH_SIZE)", LLAMA_PROMPT_BATCH_SIZE);
  }
  
  // Initialize context parameters
  ctx->ctx_params = llama_context_default_params();
  ctx->ctx_params.n_ctx = ctx->n_ctx;
  ctx->ctx_params.n_threads = ctx->n_threads;
  
  // Use adaptive batch size if available, otherwise fall back to config.h default
  if (adaptive_success) {
    ethervox_runtime_config_t optimal;
    ethervox_extract_model_metadata(ctx->model, &metadata);
    ethervox_calculate_optimal_config(&metadata, &optimal);
    ctx->ctx_params.n_batch = optimal.batch_size;
    ETHERVOX_LOG_INFO("[Adaptive Config] Using adaptive batch size: %u", optimal.batch_size);
  } else {
    ctx->ctx_params.n_batch = LLAMA_PROMPT_BATCH_SIZE;
    ETHERVOX_LOG_INFO("[Adaptive Config] Using fallback batch size: %d", LLAMA_PROMPT_BATCH_SIZE);
  }
  
  ctx->ctx_params.n_threads_batch = ctx->n_threads;  // Use same threads for batch processing
  // Note: seed is now set via llama_sampler, not context params
  
  LLAMA_LOG("Context params: n_ctx=%u, n_batch=%u, n_threads=%u, n_threads_batch=%u",
            ctx->ctx_params.n_ctx, ctx->ctx_params.n_batch, 
            ctx->ctx_params.n_threads, ctx->ctx_params.n_threads_batch);
  
  // Create context
  ctx->ctx = llama_init_from_model(ctx->model, ctx->ctx_params);
  if (!ctx->ctx) {
    ETHERVOX_LOG_ERROR("Failed to create Llama context");
    llama_model_free(ctx->model);
    ctx->model = NULL;
    return ETHERVOX_ERROR_FAILED;
  }
  
  // Save model path
  ctx->loaded_model_path = strdup(model_path);
  backend->is_loaded = true;
  
  // Clear KV cache to ensure clean initial state
  llama_memory_t mem = llama_get_memory(ctx->ctx);
  llama_memory_seq_rm(mem, 0, -1, -1);
  ETHERVOX_LOG_INFO("KV cache initialized");
  
  ETHERVOX_LOG_INFO("Llama model loaded successfully");
  ETHERVOX_LOG_INFO("Context size: %u, GPU layers: %u", ctx->n_ctx, ctx->n_gpu_layers);
  
  return ETHERVOX_SUCCESS;
#endif
}

static void llama_backend_unload_model(ethervox_llm_backend_t* backend) {
  if (!backend || !backend->handle) {
    return;
  }
  
#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  if (ctx->ctx) {
    llama_free(ctx->ctx);
    ctx->ctx = NULL;
  }
  
  if (ctx->model) {
    llama_model_free(ctx->model);
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

static ethervox_result_t llama_backend_generate(ethervox_llm_backend_t* backend,
                                 const char* prompt,
                                 const char* language_code,
                                 ethervox_llm_response_t* response) {
  if (!backend || !backend->handle || !prompt || !response) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  ETHERVOX_LOG_ERROR("Llama backend not available");
  return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
  
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  if (!ctx->ctx || !ctx->model) {
    ETHERVOX_LOG_ERROR("Model not loaded");
    return ETHERVOX_ERROR_NOT_INITIALIZED;
  }
  
  LLAMA_LOG("Starting LLM generation for prompt: %.50s...", prompt);
  clock_t start_time = clock();
  
  // Get model vocab for tokenization
  const struct llama_vocab * vocab = llama_model_get_vocab(ctx->model);
  LLAMA_LOG("Got vocab from model");
  
  // Tokenize prompt
  // llama_tokenize returns the negation of the required token count when called with
  // a NULL output buffer and zero size. Negate the result to get the positive token count.
  const int n_prompt_tokens = -llama_tokenize(vocab, prompt, (int)strlen(prompt),
                                              NULL, 0, true, true);
  LLAMA_LOG("Tokenized prompt: %d tokens needed", n_prompt_tokens);
  
  if (n_prompt_tokens < 0) {
    ETHERVOX_LOG_ERROR("Failed to tokenize prompt");
    return ETHERVOX_ERROR_FAILED;
  }
  
  llama_token* prompt_tokens = (llama_token*)malloc(n_prompt_tokens * sizeof(llama_token));
  if (!prompt_tokens) {
    ETHERVOX_LOG_ERROR("Failed to allocate prompt tokens");
    return ETHERVOX_ERROR_OUT_OF_MEMORY;
  }
  
  int n_tokens = llama_tokenize(vocab, prompt, (int)strlen(prompt),
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
  
  LLAMA_LOG("Evaluating prompt with %d tokens", n_tokens);
  // Evaluate prompt
  if (llama_decode(ctx->ctx, llama_batch_get_one(prompt_tokens, n_tokens)) != 0) {
    ETHERVOX_LOG_ERROR("Failed to evaluate prompt");
    free(prompt_tokens);
    free(response_text);
    return ETHERVOX_ERROR_FAILED;
  }
  LLAMA_LOG("Prompt evaluated successfully");
  
  free(prompt_tokens);
  
  LLAMA_LOG("Creating sampler chain");
  // Create sampler chain for token generation
  struct llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
  struct llama_sampler * sampler = llama_sampler_chain_init(sparams);
  if (!sampler) {
    ETHERVOX_LOG_ERROR("Failed to create sampler chain");
    free(response_text);
    return ETHERVOX_ERROR_FAILED;
  }
  
  // Add sampling strategies to the chain (same as streaming for consistency)
  llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
  llama_sampler_chain_add(sampler, llama_sampler_init_top_p(ctx->top_p, 1));
  llama_sampler_chain_add(sampler, llama_sampler_init_penalties(128, 1.2f, 0.0f, 0.0f));  // Stronger repeat penalty (128 lookback, 1.2 strength)
  llama_sampler_chain_add(sampler, llama_sampler_init_temp(ctx->temperature));
  llama_sampler_chain_add(sampler, llama_sampler_init_dist(0));
  LLAMA_LOG("Sampler chain created, starting token generation (max %d tokens)", ctx->n_predict);
  
  // Generate tokens
  int n_generated = 0;
  bool finished = false;
  
  for (int i = 0; i < (int)ctx->n_predict && !finished; i++) {
    if (i % 10 == 0 && i > 0) {
      LLAMA_LOG("Generated %d tokens so far...", i);
    }
    // Sample next token
    llama_token new_token = llama_sampler_sample(sampler, ctx->ctx, -1);
    
    // Check for end of generation
    if (llama_vocab_is_eog(vocab, new_token)) {
      finished = true;
      break;
    }
    
    // Decode token to text
    char piece[256];
    int n_piece = llama_token_to_piece(vocab, new_token, piece, sizeof(piece), 0, false);
    
    if (n_piece > 0) {
      if (response_len + n_piece < LLAMA_MAX_RESPONSE_LENGTH - 1) {
        memcpy(response_text + response_len, piece, n_piece);
        response_len += n_piece;
        response_text[response_len] = '\0';
      }
    }
    
    // Evaluate next token
    if (llama_decode(ctx->ctx, llama_batch_get_one(&new_token, 1)) != 0) {
      ETHERVOX_LOG_WARN("Failed to evaluate token at position %d", i);
      break;
    }
    
    n_generated++;
  }
  
  // Calculate processing time
  clock_t end_time = clock();
  uint32_t processing_time = (uint32_t)(((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000);
  
  // Free sampler
  llama_sampler_free(sampler);
  LLAMA_LOG("Generation complete: %d tokens in %u ms", n_generated, processing_time);
  
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

// Generate response with streaming token callback
static ethervox_result_t llama_backend_generate_stream(ethervox_llm_backend_t* backend,
                                         const char* prompt,
                                         const char* language_code,
                                         void (*callback)(const char* token, void* user_data),
                                         void* user_data) {
  if (!backend || !prompt || !callback) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

#if !defined(ETHERVOX_WITH_LLAMA) || !LLAMA_HEADER_AVAILABLE
  return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#else
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  if (!ctx || !ctx->model || !ctx->ctx) {
    LLAMA_ERROR("Model not loaded");
    return ETHERVOX_ERROR_NOT_INITIALIZED;
  }

  LLAMA_LOG("Starting streaming generation for prompt: %s", prompt);
  clock_t start_time = clock();

  // Note: We don't try to cancel mid-generation as it causes state corruption.
  // Instead, Kotlin layer uses processing IDs to ignore results from old generations.
  ctx->cancel_requested = false;
  
  // Synchronize to ensure any previous computation is complete
  llama_synchronize(ctx->ctx);
  
  // Check if KV cache is getting full and clear if needed
  llama_memory_t mem = llama_get_memory(ctx->ctx);
  llama_pos max_pos = llama_memory_seq_pos_max(mem, 0);
  if (max_pos > (int32_t)(ctx->n_ctx * 0.85)) {  // Clear if >85% full
    LLAMA_LOG("KV cache nearly full (pos=%d, limit=%u), clearing", max_pos, ctx->n_ctx);
    llama_memory_seq_rm(mem, 0, -1, -1);
    max_pos = -1;  // Reset after clearing
  } else if (max_pos >= 0) {
    LLAMA_LOG("KV cache usage: %d / %u tokens", max_pos, ctx->n_ctx);
  }

  const struct llama_vocab* vocab = llama_model_get_vocab(ctx->model);
  
  // Get special token IDs for debugging
  llama_token bos_token = llama_vocab_bos(vocab);
  llama_token eos_token = llama_vocab_eos(vocab);
  LLAMA_LOG("Special tokens - BOS: %d, EOS: %d", bos_token, eos_token);
  
  // Detect model type from loaded model path for correct prompt formatting
  bool is_qwen = (ctx->loaded_model_path && strstr(ctx->loaded_model_path, "qwen") != NULL);
  bool is_tinyllama = (ctx->loaded_model_path && strstr(ctx->loaded_model_path, "tinyllama") != NULL);
  bool is_deepseek = (ctx->loaded_model_path && strstr(ctx->loaded_model_path, "deepseek") != NULL);
  
  LLAMA_LOG("Model type detection: Qwen=%d, TinyLlama=%d, DeepSeek=%d", is_qwen, is_tinyllama, is_deepseek);
  
  // Check if user is explicitly requesting brevity
  bool brevity_requested = (strstr(prompt, "concise") != NULL || 
                           strstr(prompt, "brief") != NULL ||
                           strstr(prompt, "short") != NULL ||
                           strstr(prompt, "quick") != NULL ||
                           strstr(prompt, "one sentence") != NULL ||
                           strstr(prompt, "shorter") != NULL);
  
  // Format prompt based on model type
  char formatted_prompt[2048];
  int written;
  
  if (is_qwen) {
    // Qwen2 uses ChatML format
    if (brevity_requested) {
      written = snprintf(formatted_prompt, sizeof(formatted_prompt),
        "<|im_start|>system\n"
        "You are a helpful AI voice assistant. The user wants EXTREMELY BRIEF answers. "
        "Respond in ONE SHORT SENTENCE only (10-15 words max). Be direct and concise.<|im_end|>\n"
        "<|im_start|>user\n%s<|im_end|>\n"
        "<|im_start|>assistant\n",
        prompt);
    } else {
      written = snprintf(formatted_prompt, sizeof(formatted_prompt),
        "<|im_start|>system\n"
        "You are a helpful AI voice assistant. IMPORTANT: Keep responses SHORT and CONCISE (2-3 sentences max). "
        "Be direct and to the point. Avoid long explanations or examples unless specifically asked.<|im_end|>\n"
        "<|im_start|>user\n%s<|im_end|>\n"
        "<|im_start|>assistant\n",
        prompt);
    }
  } else if (is_tinyllama) {
    // TinyLlama uses Zephyr format (similar to ChatML but different tags)
    if (brevity_requested) {
      written = snprintf(formatted_prompt, sizeof(formatted_prompt),
        "<|system|>\nYou are a helpful AI voice assistant. Respond in ONE SHORT SENTENCE only (10-15 words max).</s>\n"
        "<|user|>\n%s</s>\n"
        "<|assistant|>\n",
        prompt);
    } else {
      written = snprintf(formatted_prompt, sizeof(formatted_prompt),
        "<|system|>\nYou are a helpful AI voice assistant. Keep responses SHORT and CONCISE (2-3 sentences max).</s>\n"
        "<|user|>\n%s</s>\n"
        "<|assistant|>\n",
        prompt);
    }
  } else if (is_deepseek) {
    // DeepSeek uses simple format without special tokens
    if (brevity_requested) {
      written = snprintf(formatted_prompt, sizeof(formatted_prompt),
        "User: %s\nAssistant (respond in 1 short sentence):",
        prompt);
    } else {
      written = snprintf(formatted_prompt, sizeof(formatted_prompt),
        "User: %s\nAssistant:",
        prompt);
    }
  } else {
    // Fallback: minimal format for unknown models
    written = snprintf(formatted_prompt, sizeof(formatted_prompt), "%s\n", prompt);
  }
  
  if (written >= (int)sizeof(formatted_prompt)) {
    LLAMA_ERROR("Formatted prompt too long, truncated");
  }
  
  const char* model_type = is_qwen ? "Qwen/ChatML" : (is_tinyllama ? "TinyLlama/Zephyr" : (is_deepseek ? "DeepSeek/Simple" : "Generic"));
  LLAMA_LOG("Prompt prepared with %s format (cache pos: %d, brevity mode: %s)", 
            model_type, max_pos, brevity_requested ? "ULTRA" : "NORMAL");
  
  // Tokenize formatted prompt
  int n_prompt_tokens = -llama_tokenize(vocab, formatted_prompt, strlen(formatted_prompt), NULL, 0, true, false);
  if (n_prompt_tokens <= 0 || n_prompt_tokens > (int)ctx->n_ctx - 10) {
    LLAMA_ERROR("Invalid token count: %d (context size: %u)", n_prompt_tokens, ctx->n_ctx);
    return ETHERVOX_ERROR_FAILED;
  }
  
  llama_token* prompt_tokens = (llama_token*)malloc(n_prompt_tokens * sizeof(llama_token));
  if (!prompt_tokens) {
    LLAMA_ERROR("Failed to allocate memory for prompt tokens");
    return ETHERVOX_ERROR_FAILED;
  }
  
  llama_tokenize(vocab, formatted_prompt, strlen(formatted_prompt), prompt_tokens, n_prompt_tokens, true, false);
  LLAMA_LOG("Prompt tokenized: %d tokens", n_prompt_tokens);

  // Evaluate prompt in larger batches for faster parallel processing
  // Use bigger batch size for prompt evaluation since it's embarrassingly parallel
  clock_t prompt_eval_start = clock();
  int batch_size = LLAMA_PROMPT_BATCH_SIZE;
  LLAMA_LOG("Starting prompt evaluation with batch_size=%d, n_threads=%u", batch_size, ctx->n_threads);
  
  for (int i = 0; i < n_prompt_tokens; i += batch_size) {
    int chunk_size = (i + batch_size > n_prompt_tokens) ? (n_prompt_tokens - i) : batch_size;
    clock_t chunk_start = clock();
    if (llama_decode(ctx->ctx, llama_batch_get_one(&prompt_tokens[i], chunk_size)) != 0) {
      LLAMA_ERROR("Failed to evaluate prompt at position %d", i);
      free(prompt_tokens);
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_LLM_INFERENCE_FAILED, "Failed to decode prompt batch");
    }
    uint32_t chunk_time = (uint32_t)(((double)(clock() - chunk_start) / CLOCKS_PER_SEC) * 1000);
    LLAMA_LOG("Batch %d/%d (%d tokens) processed in %u ms", 
              (i/batch_size)+1, (n_prompt_tokens+batch_size-1)/batch_size, chunk_size, chunk_time);
  }
  free(prompt_tokens);
  
  uint32_t prompt_eval_time = (uint32_t)(((double)(clock() - prompt_eval_start) / CLOCKS_PER_SEC) * 1000);
  LLAMA_LOG("Prompt evaluated successfully in %u ms (%.1f tokens/sec)", 
            prompt_eval_time, (float)n_prompt_tokens / (prompt_eval_time / 1000.0f));

  // Create sampler chain with proper controls to prevent wild/repetitive outputs
  struct llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
  
  // Add sampling strategies in order:
  // 1. top_k - limit to top 40 most probable tokens (prevents unlikely/random tokens)
  llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
  
  // 2. top_p - nucleus sampling with configured threshold (prevents low-probability tangents)
  llama_sampler_chain_add(sampler, llama_sampler_init_top_p(ctx->top_p, 1));
  
  // 3. repetition penalty - discourage repeating patterns (prevents training-data-style repetition)
  llama_sampler_chain_add(sampler, llama_sampler_init_penalties(
    128,    // penalty_last_n: look back 128 tokens (wider context)
    1.2f,   // penalty_repeat: stronger penalty to break training patterns
    0.0f,   // penalty_freq: no frequency penalty
    0.0f    // penalty_present: no presence penalty
  ));
  
  // 4. temperature - controls randomness/creativity
  llama_sampler_chain_add(sampler, llama_sampler_init_temp(ctx->temperature));
  
  // 5. dist - final sampling from the filtered distribution
  llama_sampler_chain_add(sampler, llama_sampler_init_dist(0));
  
  LLAMA_LOG("Sampler chain created (temp=%.2f, top_p=%.2f, top_k=40, repeat_penalty=1.2), starting streaming token generation (max %u tokens)", 
            (double)ctx->temperature, (double)ctx->top_p, (unsigned int)ctx->n_predict);

  // Generate tokens and stream them
  int n_generated = 0;
  bool finished = false;
  
  // Buffer to accumulate recent tokens for end marker detection
  char recent_text[128] = {0};
  int recent_text_len = 0;

  for (int i = 0; i < (int)ctx->n_predict && !finished; i++) {
    if (i % 10 == 0 && i > 0) {
      LLAMA_LOG("Generated %d tokens so far...", i);
    }
    
    // Sample next token
    llama_token new_token = llama_sampler_sample(sampler, ctx->ctx, -1);

    // Check for end of generation first (before decoding)
    if (llama_vocab_is_eog(vocab, new_token)) {
      finished = true;
      LLAMA_LOG("EOG token detected at position %d (token_id=%d)", n_generated, new_token);
      break;
    }

    // Log token details for debugging
    if (n_generated < 5) {
      LLAMA_LOG("Sampled token %d: id=%d, is_eog=%d", n_generated, new_token, llama_vocab_is_eog(vocab, new_token));
    }

    // Decode token to text
    char piece[256];
    int n_piece = llama_token_to_piece(vocab, new_token, piece, sizeof(piece), 0, false);

    if (n_piece > 0) {
      piece[n_piece] = '\0';
      
      // Log first few tokens to debug
      if (n_generated < 5) {
        LLAMA_LOG("Token %d: id=%d, text='%s' (len=%d)", n_generated, new_token, piece, n_piece);
      }
      
      // Accumulate recent text for end marker detection (keep last 50 chars)
      if (recent_text_len + n_piece >= (int)sizeof(recent_text) - 1) {
        // Shift buffer left to make room
        int shift = (recent_text_len + n_piece) - (int)sizeof(recent_text) + 1;
        memmove(recent_text, recent_text + shift, recent_text_len - shift);
        recent_text_len -= shift;
      }
      memcpy(recent_text + recent_text_len, piece, n_piece);
      recent_text_len += n_piece;
      recent_text[recent_text_len] = '\0';
      
      // Check for end markers based on model type in accumulated text
      bool found_end_marker = false;
      if (is_qwen) {
        // Qwen ChatML markers
        if (strstr(recent_text, "<|im_end|>") != NULL || strstr(recent_text, "<|endoftext|>") != NULL) {
          found_end_marker = true;
        }
      } else if (is_tinyllama) {
        // TinyLlama Zephyr markers
        if (strstr(recent_text, "</s>") != NULL || strstr(recent_text, "<|assistant|>") != NULL) {
          found_end_marker = true;
        }
      }
      // DeepSeek and generic models rely on EOG token detection only
      
      if (found_end_marker) {
        finished = true;
        LLAMA_LOG("End marker detected for %s model, stopping generation", model_type);
        // Don't send this token or any accumulated marker to output
        break;
      }
      
      // Send token to callback (only if no end marker detected)
      callback(piece, user_data);
    }

    // Evaluate next token
    if (llama_decode(ctx->ctx, llama_batch_get_one(&new_token, 1)) != 0) {
      ETHERVOX_LOG_WARN("Failed to evaluate token at position %d", i);
      break;
    }

    n_generated++;
  }

  clock_t end_time = clock();
  uint32_t processing_time = (uint32_t)(((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000);

  llama_sampler_free(sampler);
  
  LLAMA_LOG("Streaming generation complete: %d tokens in %u ms", n_generated, processing_time);

  return ETHERVOX_SUCCESS;
#endif
}

static ethervox_result_t llama_backend_get_capabilities(ethervox_llm_backend_t* backend,
                                        ethervox_llm_capabilities_t* capabilities) {
  if (!backend || !capabilities) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  llama_backend_context_t* ctx = (llama_backend_context_t*)backend->handle;
  
  capabilities->supports_streaming = true;  // Streaming now supported!
  capabilities->supports_gpu = true;
  capabilities->supports_quantization = true;
  capabilities->supports_context_caching = true;
  capabilities->max_context_length = ctx ? ctx->n_ctx : LLAMA_DEFAULT_CONTEXT_LENGTH;
  capabilities->recommended_context_length = 2048;
  capabilities->max_batch_size = 512;
  capabilities->model_format = "GGUF";
  
  return ETHERVOX_SUCCESS;
}
