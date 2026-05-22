/**
 * @file model_downloader.h
 * @brief Model download and status checking for all EthervoxAI models
 *
 * Provides unified interface for checking model availability and downloading
 * models for Governor LLM, Whisper STT, Vosk STT, and Piper TTS.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#ifndef ETHERVOX_MODEL_DOWNLOADER_H
#define ETHERVOX_MODEL_DOWNLOADER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Model types supported by EthervoxAI
 */
typedef enum {
    ETHERVOX_MODEL_TYPE_GOVERNOR,      // Governor LLM (GGUF format)
    ETHERVOX_MODEL_TYPE_WHISPER,       // Whisper STT (ggml format)
    ETHERVOX_MODEL_TYPE_VOSK,          // Vosk STT (model directory)
    ETHERVOX_MODEL_TYPE_PIPER,         // Piper TTS (.onnx format)
    ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE  // Wake word template (raw audio)
} ethervox_model_type_t;

/**
 * @brief Model status information
 */
typedef enum {
    ETHERVOX_MODEL_STATUS_NOT_FOUND = 0,    // Model not present
    ETHERVOX_MODEL_STATUS_FOUND,            // Model exists and valid
    ETHERVOX_MODEL_STATUS_CORRUPT,          // Model exists but corrupted
    ETHERVOX_MODEL_STATUS_DOWNLOADING,      // Currently downloading
    ETHERVOX_MODEL_STATUS_INCOMPLETE,       // Partial download
    ETHERVOX_MODEL_STATUS_UNKNOWN           // Cannot determine status
} ethervox_model_status_t;

/**
 * @brief Model information structure
 */
typedef struct {
    ethervox_model_type_t type;
    ethervox_model_status_t status;
    char path[512];                    // Full path to model
    char name[128];                    // Model name/identifier
    uint64_t size_bytes;               // Model size in bytes (0 if unknown)
    uint64_t downloaded_bytes;         // Bytes downloaded (for partial)
    float download_progress;           // 0.0 - 1.0 (for active downloads)
    bool is_default;                   // Is this the recommended default?
    char description[256];             // Human-readable description
    char url[512];                     // Download URL
} ethervox_model_info_t;

/**
 * @brief Download progress callback
 * 
 * @param bytes_downloaded Bytes downloaded so far
 * @param total_bytes Total size (0 if unknown)
 * @param user_data User-provided data
 */
typedef void (*ethervox_download_progress_callback_t)(
    uint64_t bytes_downloaded,
    uint64_t total_bytes,
    void* user_data
);

/**
 * @brief Get model base directory
 * 
 * Returns ~/.ethervox/models/ or platform-specific equivalent
 * 
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_model_get_base_dir(char* buffer, size_t buffer_size);

/**
 * @brief Check if a specific model exists and is valid
 * 
 * @param type Model type
 * @param model_name Model name/identifier (NULL for default)
 * @param info Output model information (optional)
 * @return Model status
 */
ethervox_model_status_t ethervox_model_check_status(
    ethervox_model_type_t type,
    const char* model_name,
    ethervox_model_info_t* info
);

/**
 * @brief Get recommended default model for a type
 * 
 * @param type Model type
 * @param info Output model information
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_model_get_default(
    ethervox_model_type_t type,
    ethervox_model_info_t* info
);

/**
 * @brief List available models of a specific type
 * 
 * @param type Model type
 * @param models Output array of model info (allocated by function, caller must free)
 * @param count Number of models found
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_model_list(
    ethervox_model_type_t type,
    ethervox_model_info_t** models,
    uint32_t* count
);

/**
 * @brief Download a model
 * 
 * @param type Model type
 * @param model_name Model name/identifier
 * @param progress_callback Optional progress callback
 * @param user_data User data for callback
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_model_download(
    ethervox_model_type_t type,
    const char* model_name,
    ethervox_download_progress_callback_t progress_callback,
    void* user_data
);

/**
 * @brief Cancel ongoing download
 * 
 * @param type Model type
 * @param model_name Model name
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_model_cancel_download(
    ethervox_model_type_t type,
    const char* model_name
);

/**
 * @brief Delete a model
 * 
 * @param type Model type
 * @param model_name Model name
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_model_delete(
    ethervox_model_type_t type,
    const char* model_name
);

/**
 * @brief Verify model integrity
 * 
 * Checks file size, format validity, etc.
 * 
 * @param type Model type
 * @param model_path Path to model
 * @return true if valid, false otherwise
 */
bool ethervox_model_verify(
    ethervox_model_type_t type,
    const char* model_path
);

// ============================================================================
// Convenience functions for specific model types
// ============================================================================

/**
 * @brief Check Governor LLM status
 * 
 * @param model_name Model name (e.g., "granite-3.0-2b-instruct-Q4_K_M.gguf")
 * @return Model status
 */
ethervox_model_status_t ethervox_model_governor_status(const char* model_name);

/**
 * @brief Check Whisper STT status
 * 
 * @param model_name Model name (e.g., "ggml-base.en.bin", NULL for default)
 * @return Model status
 */
ethervox_model_status_t ethervox_model_whisper_status(const char* model_name);

/**
 * @brief Check Vosk STT status
 * 
 * @param model_name Model name (e.g., "vosk-model-small-en-us-0.15", NULL for default)
 * @return Model status
 */
ethervox_model_status_t ethervox_model_vosk_status(const char* model_name);

/**
 * @brief Check Piper TTS status
 * 
 * @param model_name Model name (e.g., "en_US-lessac-medium.onnx", NULL for default)
 * @return Model status
 */
ethervox_model_status_t ethervox_model_piper_status(const char* model_name);

/**
 * @brief Check wake word template status
 * 
 * @param wake_word Wake word phrase (e.g., "hey ethervox")
 * @return Model status
 */
ethervox_model_status_t ethervox_model_wake_template_status(const char* wake_word);

/**
 * @brief Get human-readable status string
 * 
 * @param status Model status
 * @return Status string (static, don't free)
 */
const char* ethervox_model_status_string(ethervox_model_status_t status);

/**
 * @brief Get human-readable type string
 * 
 * @param type Model type
 * @return Type string (static, don't free)
 */
const char* ethervox_model_type_string(ethervox_model_type_t type);

/**
 * @brief Get total disk usage for all models
 * 
 * @param bytes_used Output total bytes used
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_model_get_disk_usage(uint64_t* bytes_used);

/**
 * @brief Check if sufficient disk space for model
 * 
 * @param type Model type
 * @param model_name Model name
 * @return true if sufficient space, false otherwise
 */
bool ethervox_model_check_disk_space(
    ethervox_model_type_t type,
    const char* model_name
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_MODEL_DOWNLOADER_H
