/**
 * @file model_router_example.c
 * @brief Source file for EthervoxAI
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ethervox_sdk.h"

// Example Model Router: Multi-Model LLM Router
// Routes requests to different LLM models based on complexity and requirements

// Custom model router implementation
typedef struct {
  float complexity_threshold_gpt4;    // Threshold for GPT-4 routing
  float creativity_threshold_claude;  // Threshold for Claude routing
  bool prefer_local_models;           // Prefer local models when possible
  uint32_t max_retries;               // Maximum retry attempts
} router_config_t;

// Simulate complexity analysis of a request
static float analyze_request_complexity(const ethervox_llm_request_t* request) {
  float complexity = 0.0f;

  // Simple heuristic based on prompt length
  size_t prompt_length = strlen(request->prompt);
  complexity += prompt_length / 1000.0f;  // Longer prompts are more complex

  // Check for complex keywords
  const char* complex_keywords[] = {"analyze",     "compare", "explain",     "reasoning", "logic",
                                    "mathematics", "code",    "programming", "algorithm"};

  for (int i = 0; i < 9; i++) {
    if (strstr(request->prompt, complex_keywords[i])) {
      complexity += 0.2f;
    }
  }

  // Factor in creativity requirement
  complexity += request->creativity_level * 0.3f;

  return complexity;
}

// Simulate model inference (placeholder implementation)
static int simulate_model_inference(const ethervox_llm_request_t* request,
                                    ethervox_llm_response_t* response,
                                    const ethervox_model_config_t* model) {
  // Simulate different response times and capabilities for different models
  uint32_t base_time = 500;  // Base response time in ms
  const char* model_response = "I understand your request. ";

  switch (model->type) {
    case ETHERVOX_MODEL_TYPE_OPENAI_GPT:
      if (strstr(model->model_name, "gpt-4")) {
        base_time = 2000;  // GPT-4 is slower but more capable
        model_response = "Based on detailed analysis, I can provide a comprehensive response. ";
      } else {
        base_time = 800;  // GPT-3.5 is faster
        model_response = "I can help you with that. ";
      }
      break;

    case ETHERVOX_MODEL_TYPE_LOCAL_LLM:
      base_time = 300;  // Local models are fastest
      model_response = "Using local processing, here's my response: ";
      break;

    case ETHERVOX_MODEL_TYPE_HUGGINGFACE:
      base_time = 1200;  // HuggingFace API has variable latency
      model_response = "Using specialized model, I can assist with: ";
      break;

    default:
      base_time = 1000;
      model_response = "Response from model: ";
      break;
  }

  // Simulate processing time based on prompt length
  uint32_t processing_time = base_time + (strlen(request->prompt) * 2);

  // Create response
  snprintf(response->response, sizeof(response->response), "%s%s [Simulated response from %s]",
           model_response, request->prompt, model->model_name);

  response->is_complete = true;
  response->confidence = 0.85f + (rand() % 15) / 100.0f;  // 0.85-1.00
  response->processing_time_ms = processing_time;
  response->token_count = strlen(response->response) / 4;  // Rough estimate
  strcpy(response->model_used, model->model_name);

  // Simulate occasional failures
  if (rand() % 20 == 0) {  // 5% failure rate
    return -1;
  }

  return 0;
}

// Smart model routing function
static int smart_model_route(const ethervox_llm_request_t* request,
                             ethervox_llm_response_t* response,
                             const ethervox_model_config_t* config) {
  // This function is called for each available model
  // The actual routing logic is in the router itself
  return simulate_model_inference(request, response, config);
}

// Custom model router with smart routing logic
static int multi_model_route(ethervox_model_router_t* router, const ethervox_llm_request_t* request,
                             ethervox_llm_response_t* response) {
  if (!router || !request || !response || router->model_count == 0) {
    return -1;
  }

  router_config_t* config = (router_config_t*)router;  // Cast to our config

  // Analyze request complexity
  float complexity = analyze_request_complexity(request);

  printf("Request complexity: %.2f\n", complexity);

  // Select model based on complexity and requirements
  int selected_model = -1;

  // Rule 1: High complexity -> GPT-4
  if (complexity > config->complexity_threshold_gpt4) {
    for (uint32_t i = 0; i < router->model_count; i++) {
      if (router->models[i].type == ETHERVOX_MODEL_TYPE_OPENAI_GPT &&
          strstr(router->models[i].model_name, "gpt-4")) {
        selected_model = i;
        break;
      }
    }
  }

  // Rule 2: High creativity -> Claude (if available)
  if (selected_model == -1 && request->creativity_level > config->creativity_threshold_claude) {
    for (uint32_t i = 0; i < router->model_count; i++) {
      if (strstr(router->models[i].model_name, "claude")) {
        selected_model = i;
        break;
      }
    }
  }

  // Rule 3: Prefer local models if configured
  if (selected_model == -1 && config->prefer_local_models) {
    for (uint32_t i = 0; i < router->model_count; i++) {
      if (router->models[i].type == ETHERVOX_MODEL_TYPE_LOCAL_LLM) {
        selected_model = i;
        break;
      }
    }
  }

  // Rule 4: Default to first available model
  if (selected_model == -1) {
    selected_model = 0;
  }

  // Try selected model with retries
  for (uint32_t retry = 0; retry < config->max_retries; retry++) {
    printf("Trying model: %s (attempt %d)\n", router->models[selected_model].model_name, retry + 1);

    router->total_requests++;

    int result = smart_model_route(request, response, &router->models[selected_model]);
    if (result == 0) {
      router->successful_requests++;
      router->active_model_index = selected_model;

      // Update statistics
      router->average_response_time_ms =
          (router->average_response_time_ms * (router->successful_requests - 1) +
           response->processing_time_ms) /
          router->successful_requests;

      return 0;
    }

    printf("Model failed, trying fallback...\n");

    // Try next available model as fallback
    selected_model = (selected_model + 1) % router->model_count;
  }

  return -1;  // All models failed
}

// Create multi-model router
ethervox_model_router_t* create_multi_model_router(void) {
  // Allocate router with extended config
  router_config_t* router = (router_config_t*)calloc(1, sizeof(router_config_t));
  if (!router)
    return NULL;

  // Cast to base type for compatibility
  ethervox_model_router_t* base_router = (ethervox_model_router_t*)router;

  // Set router name
  strcpy(base_router->name, "Multi-Model Smart Router");

  // Configure routing thresholds
  router->complexity_threshold_gpt4 = 0.7f;
  router->creativity_threshold_claude = 0.6f;
  router->prefer_local_models = false;
  router->max_retries = 3;

  // Set routing function
  base_router->route = smart_model_route;  // Individual model function

  return base_router;
}

// Example usage
int main() {
  printf("=== EtherVox SDK Model Router Example ===\n\n");

  // Initialize SDK
  ethervox_sdk_t sdk;
  if (ethervox_sdk_init(&sdk) != 0) {
    printf("Failed to initialize SDK\n");
    return 1;
  }

  // Create model router
  ethervox_model_router_t* router = create_multi_model_router();
  if (!router) {
    printf("Failed to create model router\n");
    ethervox_sdk_cleanup(&sdk);
    return 1;
  }

  // Add model configurations
  ethervox_model_config_t models[] = {{.type = ETHERVOX_MODEL_TYPE_LOCAL_LLM,
                                       .model_name = "llama-2-7b",
                                       .endpoint = "http://localhost:8080",
                                       .is_local = true,
                                       .max_tokens = 2048,
                                       .temperature = 0.7f,
                                       .timeout_ms = 5000},
                                      {.type = ETHERVOX_MODEL_TYPE_OPENAI_GPT,
                                       .model_name = "gpt-3.5-turbo",
                                       .endpoint = "https://api.openai.com/v1/chat/completions",
                                       .is_local = false,
                                       .max_tokens = 4096,
                                       .temperature = 0.8f,
                                       .timeout_ms = 10000},
                                      {.type = ETHERVOX_MODEL_TYPE_OPENAI_GPT,
                                       .model_name = "gpt-4",
                                       .endpoint = "https://api.openai.com/v1/chat/completions",
                                       .is_local = false,
                                       .max_tokens = 8192,
                                       .temperature = 0.9f,
                                       .timeout_ms = 30000}};

  for (int i = 0; i < 3; i++) {
    strcpy(models[i].api_key, "sk-example-key");
    router->models[router->model_count] = models[i];
    router->model_count++;

    printf("Added model: %s (%s)\n", models[i].model_name,
           ethervox_model_type_to_string(models[i].type));
  }

  ethervox_sdk_set_model_router(&sdk, router);

  printf("\nTesting model routing...\n\n");

  // Test different types of requests
  ethervox_llm_request_t requests[] = {
      {.prompt = "Hello, how are you?",
       .max_response_length = 512,
       .creativity_level = 0.3f,
       .stream_response = false},
      {.prompt = "Explain the mathematical concept of derivatives and provide examples with "
                 "step-by-step solutions",
       .max_response_length = 2048,
       .creativity_level = 0.5f,
       .stream_response = false},
      {.prompt = "Write a creative short story about a robot learning to paint",
       .max_response_length = 1024,
       .creativity_level = 0.9f,
       .stream_response = false}};

  strcpy(requests[0].language, "en");
  strcpy(requests[1].language, "en");
  strcpy(requests[2].language, "en");

  for (int i = 0; i < 3; i++) {
    printf("Request %d: \"%.50s%s\"\n", i + 1, requests[i].prompt,
           strlen(requests[i].prompt) > 50 ? "..." : "");
    printf("Creativity level: %.1f\n", requests[i].creativity_level);

    ethervox_llm_response_t response;

    // Use the custom routing logic
    int result = multi_model_route(router, &requests[i], &response);

    if (result == 0) {
      printf("  Model used: %s\n", response.model_used);
      printf("  Processing time: %d ms\n", response.processing_time_ms);
      printf("  Confidence: %.2f\n", response.confidence);
      printf("  Token count: %d\n", response.token_count);
      printf("  Response: %.100s%s\n", response.response,
             strlen(response.response) > 100 ? "..." : "");
    } else {
      printf("  Failed to get response from any model\n");
    }
    printf("\n");
  }

  // Print router statistics
  printf("Router Statistics:\n");
  printf("  Total requests: %llu\n", router->total_requests);
  printf("  Successful requests: %llu\n", router->successful_requests);
  printf("  Success rate: %.1f%%\n",
         router->total_requests > 0
             ? (100.0f * router->successful_requests / router->total_requests)
             : 0.0f);
  printf("  Average response time: %.2f ms\n", router->average_response_time_ms);
  printf("  Active model: %s\n", router->active_model_index < router->model_count
                                     ? router->models[router->active_model_index].model_name
                                     : "None");

  // Cleanup
  ethervox_sdk_cleanup(&sdk);
  free(router);

  return 0;
}