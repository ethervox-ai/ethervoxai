/**
 * @file llm_core.c
 * @brief Core LLM backend management for EthervoxAI
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

const char* ethervox_llm_backend_type_to_string(ethervox_llm_backend_type_t type) {
  switch (type) {
    case ETHERVOX_LLM_BACKEND_NONE:
      return "None";
    case ETHERVOX_LLM_BACKEND_LLAMA:
      return "Llama";
    case ETHERVOX_LLM_BACKEND_TINYLLAMA:
      return "TinyLlama";
    case ETHERVOX_LLM_BACKEND_EXTERNAL:
      return "External";
    default:
      return "Unknown";
  }
}

bool ethervox_llm_backend_is_available(ethervox_llm_backend_type_t type) {
  // Check if backend is compiled in
  switch (type) {
    case ETHERVOX_LLM_BACKEND_LLAMA:
#ifdef ETHERVOX_WITH_LLAMA
      return true;
#else
      return false;
#endif
    
    case ETHERVOX_LLM_BACKEND_TINYLLAMA:
#ifdef ETHERVOX_WITH_TINYLLAMA
      return true;
#else
      return false;
#endif
    
    case ETHERVOX_LLM_BACKEND_EXTERNAL:
      return true;  // Always available
    
    default:
      return false;
  }
}

int ethervox_llm_backend_init(ethervox_llm_backend_t* backend, const ethervox_llm_config_t* config) {
  if (!backend) {
    ETHERVOX_LOG_ERROR("Backend is NULL");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  if (!backend->init) {
    ETHERVOX_LOG_ERROR("Backend init function not implemented");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
  }
  
  return backend->init(backend, config);
}

void ethervox_llm_backend_cleanup(ethervox_llm_backend_t* backend) {
  if (!backend) {
    return;
  }
  
  if (backend->cleanup) {
    backend->cleanup(backend);
  }
  
  backend->is_initialized = false;
  backend->is_loaded = false;
}

void ethervox_llm_backend_free(ethervox_llm_backend_t* backend) {
  if (!backend) {
    return;
  }
  
  ethervox_llm_backend_cleanup(backend);
  free(backend);
}

int ethervox_llm_backend_load_model(ethervox_llm_backend_t* backend, const char* model_path) {
  if (!backend) {
    ETHERVOX_LOG_ERROR("Backend is NULL");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  if (!model_path || !*model_path) {
    ETHERVOX_LOG_ERROR("Invalid model path");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  if (!backend->load_model) {
    ETHERVOX_LOG_ERROR("Backend load_model function not implemented");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
  }
  
  ETHERVOX_LOG_INFO("Loading model from: %s", model_path);
  return backend->load_model(backend, model_path);
}

void ethervox_llm_backend_unload_model(ethervox_llm_backend_t* backend) {
  if (!backend) {
    return;
  }
  
  if (backend->unload_model) {
    backend->unload_model(backend);
  }
  
  backend->is_loaded = false;
}

int ethervox_llm_backend_generate(ethervox_llm_backend_t* backend,
                                 const char* prompt,
                                 const char* language_code,
                                 ethervox_llm_response_t* response) {
  if (!backend) {
    ETHERVOX_LOG_ERROR("Backend is NULL");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  if (!prompt || !response) {
    ETHERVOX_LOG_ERROR("Invalid arguments");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  if (!backend->is_loaded) {
    ETHERVOX_LOG_ERROR("Model not loaded");
    return ETHERVOX_ERROR_NOT_INITIALIZED;
  }
  
  if (!backend->generate) {
    ETHERVOX_LOG_ERROR("Backend generate function not implemented");
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
  }
  
  return backend->generate(backend, prompt, language_code, response);
}
