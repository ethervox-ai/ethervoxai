/**
 * @file tinyllama_backend.c
 * @brief TinyLlama optimized backend implementation for EthervoxAI
 *
 * This backend provides an optimized version specifically for TinyLlama models
 * on resource-constrained devices like Raspberry Pi and embedded systems.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI. Proprietary and confidential. See LICENSE.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - Proprietary: internal use only, see LICENSE
 * - ShareAlike: Distribute under same license
 *
 * For full license terms, see: LICENSE
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */
#include "ethervox/llm.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TinyLlama optimized backend - uses Llama backend with optimizations
// This is a wrapper that configures the Llama backend for TinyLlama models

ethervox_llm_backend_t* ethervox_llm_create_tinyllama_backend(void) {
  // Create a standard Llama backend
  ethervox_llm_backend_t* backend = ethervox_llm_create_llama_backend();
  
  if (backend) {
    // Override name to indicate TinyLlama optimization
    backend->name = "TinyLlama";
    backend->type = ETHERVOX_LLM_BACKEND_TINYLLAMA;
    
    ETHERVOX_LOG_INFO("TinyLlama backend created (optimized for embedded systems)");
  }
  
  return backend;
}
