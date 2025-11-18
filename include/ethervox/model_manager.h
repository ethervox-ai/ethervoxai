/**
 * @file model_manager.h
 * @brief Model download and management interface for EthervoxAI
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
#ifndef ETHERVOX_MODEL_MANAGER_H
#define ETHERVOX_MODEL_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Model download status
typedef enum {
  ETHERVOX_MODEL_STATUS_NOT_FOUND = 0,
  ETHERVOX_MODEL_STATUS_DOWNLOADING,
  ETHERVOX_MODEL_STATUS_AVAILABLE,
  ETHERVOX_MODEL_STATUS_CORRUPTED,
  ETHERVOX_MODEL_STATUS_ERROR
} ethervox_model_status_t;

// Download progress callback
typedef void (*ethervox_download_progress_callback_t)(
    const char* model_name,
    uint64_t downloaded_bytes,
    uint64_t total_bytes,
    float progress_percent,
    void* user_data);

// Model information
typedef struct {
  const char* name;
  const char* description;
  const char* url;
  const char* filename;
  const char* sha256;  // Optional checksum
  uint64_t size_bytes;
  const char* format;  // "GGUF", "GGML", etc.
  const char* quantization;  // "Q4_K_M", "Q5_K_M", etc.
  bool recommended_for_embedded;
  uint32_t min_ram_mb;
} ethervox_model_info_t;

// Model manager configuration
typedef struct {
  const char* models_dir;  // Directory for downloaded models
  const char* cache_dir;   // Cache directory for partial downloads
  bool auto_download;      // Automatically download missing models
  bool verify_checksum;    // Verify SHA256 checksums
  uint32_t max_retries;    // Max download retry attempts
  uint32_t timeout_seconds; // Download timeout
  ethervox_download_progress_callback_t progress_callback;
  void* callback_user_data;
} ethervox_model_manager_config_t;

// Model manager instance
typedef struct ethervox_model_manager ethervox_model_manager_t;

// Predefined model registry
extern const ethervox_model_info_t ETHERVOX_MODEL_TINYLLAMA_1B_Q4;
extern const ethervox_model_info_t ETHERVOX_MODEL_PHI2_Q4;
extern const ethervox_model_info_t ETHERVOX_MODEL_MISTRAL_7B_Q4;
extern const ethervox_model_info_t ETHERVOX_MODEL_LLAMA2_7B_Q4;

// Initialize model manager
ethervox_model_manager_t* ethervox_model_manager_create(
    const ethervox_model_manager_config_t* config);

void ethervox_model_manager_destroy(ethervox_model_manager_t* manager);

// Get default configuration
ethervox_model_manager_config_t ethervox_model_manager_get_default_config(void);

// Model operations
ethervox_model_status_t ethervox_model_manager_get_status(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info);

int ethervox_model_manager_download(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info);

int ethervox_model_manager_get_path(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info,
    char* path_buffer,
    size_t buffer_size);

bool ethervox_model_manager_is_available(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info);

int ethervox_model_manager_ensure_available(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info);

// Model discovery
int ethervox_model_manager_list_available(
    ethervox_model_manager_t* manager,
    ethervox_model_info_t*** models,
    size_t* count);

// Cleanup
int ethervox_model_manager_delete_model(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info);

int ethervox_model_manager_clean_cache(ethervox_model_manager_t* manager);

// Utility functions
const char* ethervox_model_status_to_string(ethervox_model_status_t status);
uint64_t ethervox_model_manager_get_available_space(const char* path);
bool ethervox_model_manager_has_enough_space(const char* path, uint64_t required_bytes);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_MODEL_MANAGER_H
