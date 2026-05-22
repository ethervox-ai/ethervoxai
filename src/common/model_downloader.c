/**
 * @file model_downloader.c
 * @brief Model download and status checking implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#include "ethervox/model_downloader.h"
#include "ethervox/config.h"
#include "ethervox/logging.h"
#include "ethervox/platform.h"
#include "ethervox/platform_utils.h"
#include "ethervox/error.h"

#if HAVE_LIBCURL
#include "ethervox/platform_http.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>  // for _mkdir
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#include <errno.h>

// ============================================================================
// Model Definitions
// ============================================================================

typedef struct {
    const char* name;
    const char* description;
    const char* url;
    uint64_t size_bytes;
    bool is_default;
} model_definition_t;

// Governor LLM models
static const model_definition_t GOVERNOR_MODELS[] = {
    {
        "granite-4.0-h-1b-Q4_K_M.gguf",
        "IBM Granite 4.0 1B (Recommended) - 4-bit quantized, very fast, compact",
        "https://huggingface.co/bartowski/granite-4.0-h-1b-GGUF/resolve/main/granite-4.0-h-1b-Q4_K_M.gguf",
        900000000,  // ~900MB
        true
    },
    {
        "granite-3.0-2b-instruct-Q4_K_M.gguf",
        "IBM Granite 3.0 2B - Good balance of speed and quality",
        "https://huggingface.co/second-state/Granite-3.0-2B-Instruct-GGUF/resolve/main/granite-3.0-2b-instruct-Q4_K_M.gguf",
        1536000000,  // ~1.5GB
        false
    },
    {
        "granite-3.0-8b-instruct-Q4_K_M.gguf",
        "IBM Granite 3.0 8B - Higher quality, slower",
        "https://huggingface.co/second-state/Granite-3.0-8B-Instruct-GGUF/resolve/main/granite-3.0-8b-instruct-Q4_K_M.gguf",
        5120000000,  // ~5GB
        false
    }
};

// Whisper STT models
static const model_definition_t WHISPER_MODELS[] = {
    {
        "ggml-tiny.bin",
        "Whisper Tiny Multilingual (Default) - Very fast, compact, 99 languages",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin",
        74000000,  // ~74MB
        true
    },
    {
        "ggml-base.en.bin",
        "Whisper Base English - Fast, accurate for English",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin",
        74000000,  // ~74MB
        false
    },
    {
        "ggml-small.en.bin",
        "Whisper Small English - Better accuracy, slower",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin",
        244000000,  // ~244MB
        false
    },
    {
        "ggml-base.bin",
        "Whisper Base Multilingual - Supports 99 languages",
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
        74000000,  // ~74MB
        false
    }
};

// Vosk STT models
static const model_definition_t VOSK_MODELS[] = {
    {
        "vosk-model-small-en-us-0.15",
        "Vosk Small English US (Recommended) - Fast, lightweight",
        "https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip",
        40000000,  // ~40MB
        true
    },
    {
        "vosk-model-en-us-0.22",
        "Vosk Large English US - Best accuracy, larger size",
        "https://alphacephei.com/vosk/models/vosk-model-en-us-0.22.zip",
        1800000000,  // ~1.8GB
        false
    }
};

// Piper TTS models
static const model_definition_t PIPER_MODELS[] = {
    {
        "en_US-lessac-medium.onnx",
        "Piper Lessac Medium (Recommended) - Natural, clear voice",
        "https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx",
        17000000,  // ~17MB
        true
    },
    {
        "en_US-lessac-medium.onnx.json",
        "Piper Lessac Medium Config - Required for model",
        "https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json",
        5000,  // ~5KB
        true
    }
};

#define GOVERNOR_MODEL_COUNT (sizeof(GOVERNOR_MODELS) / sizeof(GOVERNOR_MODELS[0]))
#define WHISPER_MODEL_COUNT (sizeof(WHISPER_MODELS) / sizeof(WHISPER_MODELS[0]))
#define VOSK_MODEL_COUNT (sizeof(VOSK_MODELS) / sizeof(VOSK_MODELS[0]))
#define PIPER_MODEL_COUNT (sizeof(PIPER_MODELS) / sizeof(PIPER_MODELS[0]))

// ============================================================================
// Helper Functions
// ============================================================================

static const model_definition_t* get_model_definition(
    ethervox_model_type_t type,
    const char* model_name,
    uint32_t* index_out
) {
    const model_definition_t* models = NULL;
    uint32_t count = 0;
    
    switch (type) {
        case ETHERVOX_MODEL_TYPE_GOVERNOR:
            models = GOVERNOR_MODELS;
            count = GOVERNOR_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_WHISPER:
            models = WHISPER_MODELS;
            count = WHISPER_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_VOSK:
            models = VOSK_MODELS;
            count = VOSK_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_PIPER:
            models = PIPER_MODELS;
            count = PIPER_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE:
            return NULL;  // Wake templates don't have predefined models
    }
    
    if (!model_name) {
        // Return default model
        for (uint32_t i = 0; i < count; i++) {
            if (models[i].is_default) {
                if (index_out) *index_out = i;
                return &models[i];
            }
        }
        // Fallback to first model
        if (index_out) *index_out = 0;
        return &models[0];
    }
    
    // Find by name
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(models[i].name, model_name) == 0) {
            if (index_out) *index_out = i;
            return &models[i];
        }
    }
    
    return NULL;
}

static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static uint64_t get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (uint64_t)st.st_size;
    }
    return 0;
}

static uint64_t get_dir_size(const char* path) {
    return platform_get_directory_size(path);
}

// ============================================================================
// Public API Implementation
// ============================================================================

ethervox_result_t ethervox_model_get_base_dir(char* buffer, size_t buffer_size) {
    ETHERVOX_CHECK_PTR(buffer);
    if (buffer_size == 0) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT, "Buffer size is zero");
    }
    
#ifdef __ANDROID__
    // On Android, use the files directory set by Java
    const char* android_dir = ethervox_get_android_files_dir();
    if (!android_dir || android_dir[0] == '\0') {
        ETHERVOX_LOG_ERROR("Android files directory not set");
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_PLATFORM_INIT, "Android files directory not set");
    }
    
    int written = snprintf(buffer, buffer_size, "%s/models", android_dir);
    if (written < 0 || (size_t)written >= buffer_size) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_BUFFER_TOO_SMALL, "Buffer too small for Android model path");
    }
    
    // Ensure directory exists
#ifdef _WIN32
    _mkdir(buffer);
#else
    mkdir(buffer, 0755);
#endif
    ETHERVOX_LOG_DEBUG("Android base directory: %s", buffer);
    
    return ETHERVOX_SUCCESS;
#else
    // Use platform-specific local app data directory for models
    // Windows: %LOCALAPPDATA%\EthervoxAI
    // macOS: ~/Library/Application Support/EthervoxAI
    // Linux: ~/.ethervox
    char base_dir[512];
    ethervox_result_t result = platform_get_local_app_data_dir(base_dir, sizeof(base_dir));
    if (result != ETHERVOX_SUCCESS) {
        ETHERVOX_LOG_ERROR("Failed to get local app data directory");
        return result;
    }
    
    int written = snprintf(buffer, buffer_size, "%s/models", base_dir);
    if (written < 0 || (size_t)written >= buffer_size) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_BUFFER_TOO_SMALL, "Buffer too small for model path");
    }
    
    // Ensure directory exists (base directory is created by platform_get_local_app_data_dir)
#ifdef _WIN32
    _mkdir(buffer);
#else
    mkdir(buffer, 0755);
#endif
    
    ETHERVOX_LOG_DEBUG("Model directory: %s", buffer);
    
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_model_status_t ethervox_model_check_status(
    ethervox_model_type_t type,
    const char* model_name,
    ethervox_model_info_t* info
) {
    ETHERVOX_LOG_DEBUG("ethervox_model_check_status: type=%d, model_name=%s", type, model_name ? model_name : "NULL");
    
    // Always set the type first, before any early returns
    if (info) {
        info->type = type;
        info->status = ETHERVOX_MODEL_STATUS_UNKNOWN;
    }
    
    char base_dir[512];
    if (ethervox_is_error(ethervox_model_get_base_dir(base_dir, sizeof(base_dir)))) {
        ETHERVOX_LOG_ERROR("Failed to get base directory");
        return ETHERVOX_MODEL_STATUS_UNKNOWN;
    }
    ETHERVOX_LOG_DEBUG("Base directory: %s", base_dir);
    
    const model_definition_t* def = get_model_definition(type, model_name, NULL);
    ETHERVOX_LOG_DEBUG("Model definition: %p", (void*)def);
    
    // Build model path
    char model_path[1024];
    const char* subdir = "";
    
    switch (type) {
        case ETHERVOX_MODEL_TYPE_GOVERNOR:
            subdir = ETHERVOX_GOVERNOR_SUBDIR;
            break;
        case ETHERVOX_MODEL_TYPE_WHISPER:
            subdir = ETHERVOX_WHISPER_SUBDIR;
            break;
        case ETHERVOX_MODEL_TYPE_VOSK:
            subdir = ETHERVOX_VOSK_SUBDIR;
            break;
        case ETHERVOX_MODEL_TYPE_PIPER:
            subdir = ETHERVOX_PIPER_SUBDIR;
            break;
        case ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE:
            subdir = ETHERVOX_WAKE_TEMPLATE_SUBDIR;
            break;
    }
    
    if (model_name) {
        snprintf(model_path, sizeof(model_path), "%s/%s/%s", base_dir, subdir, model_name);
        ETHERVOX_LOG_DEBUG("Built model path from name: %s", model_path);
    } else if (def) {
        snprintf(model_path, sizeof(model_path), "%s/%s/%s", base_dir, subdir, def->name);
        ETHERVOX_LOG_DEBUG("Built model path from default: %s", model_path);
    } else {
        ETHERVOX_LOG_WARN("No model name and no default for type %d", type);
        // No model name and no default - fill info with NOT_FOUND status
        if (info) {
            memset(info, 0, sizeof(*info));
            info->type = type;
            info->status = ETHERVOX_MODEL_STATUS_NOT_FOUND;
        }
        return ETHERVOX_MODEL_STATUS_NOT_FOUND;
    }
    
    // Check if model exists
    bool exists = false;
    uint64_t size = 0;
    
    if (type == ETHERVOX_MODEL_TYPE_VOSK) {
        // Vosk models are directories
        exists = dir_exists(model_path);
        if (exists) {
            size = get_dir_size(model_path);
        }
    } else {
        // Other models are files
        exists = file_exists(model_path);
        if (exists) {
            size = get_file_size(model_path);
        }
    }
    
    ETHERVOX_LOG_DEBUG("Model check: path=%s, exists=%d, size=%llu", model_path, exists, (unsigned long long)size);
    
    if (!exists) {
        ETHERVOX_LOG_INFO("Model not found: %s", model_path);
        if (info) {
            memset(info, 0, sizeof(*info));
            info->type = type;
            info->status = ETHERVOX_MODEL_STATUS_NOT_FOUND;
            if (def) {
                strncpy(info->name, def->name, sizeof(info->name) - 1);
                strncpy(info->description, def->description, sizeof(info->description) - 1);
                strncpy(info->url, def->url, sizeof(info->url) - 1);
                info->size_bytes = def->size_bytes;
                info->is_default = def->is_default;
            } else if (model_name) {
                strncpy(info->name, model_name, sizeof(info->name) - 1);
            }
        }
        return ETHERVOX_MODEL_STATUS_NOT_FOUND;
    }
    
    // Check if size matches expected (within 10% tolerance for zip extraction)
    ethervox_model_status_t status = ETHERVOX_MODEL_STATUS_FOUND;
    if (def && def->size_bytes > 0) {
        uint64_t min_size = (uint64_t)(def->size_bytes * 0.9);
        uint64_t max_size = (uint64_t)(def->size_bytes * 1.1);
        
        if (size < min_size) {
            status = ETHERVOX_MODEL_STATUS_INCOMPLETE;
        } else if (size > max_size && type != ETHERVOX_MODEL_TYPE_VOSK) {
            // Vosk directories can be larger due to extraction
            status = ETHERVOX_MODEL_STATUS_CORRUPT;
        }
    }
    
    // Fill info structure
    if (info) {
        ETHERVOX_LOG_INFO("Model found: %s (type=%d, status=%d, size=%llu)", 
                         model_path, type, status, (unsigned long long)size);
        memset(info, 0, sizeof(*info));
        info->type = type;
        info->status = status;
        strncpy(info->path, model_path, sizeof(info->path) - 1);
        info->size_bytes = size;
        info->downloaded_bytes = size;
        info->download_progress = 1.0f;
        
        if (def) {
            strncpy(info->name, def->name, sizeof(info->name) - 1);
            strncpy(info->description, def->description, sizeof(info->description) - 1);
            strncpy(info->url, def->url, sizeof(info->url) - 1);
            info->is_default = def->is_default;
        } else if (model_name) {
            strncpy(info->name, model_name, sizeof(info->name) - 1);
        }
    }
    
    return status;
}

ethervox_result_t ethervox_model_get_default(
    ethervox_model_type_t type,
    ethervox_model_info_t* info
) {
    ETHERVOX_CHECK_PTR(info);
    
    ethervox_model_check_status(type, NULL, info);
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_model_list(
    ethervox_model_type_t type,
    ethervox_model_info_t** models,
    uint32_t* count
) {
    ETHERVOX_CHECK_PTR(models);
    ETHERVOX_CHECK_PTR(count);
    
    const model_definition_t* defs = NULL;
    uint32_t def_count = 0;
    
    switch (type) {
        case ETHERVOX_MODEL_TYPE_GOVERNOR:
            defs = GOVERNOR_MODELS;
            def_count = GOVERNOR_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_WHISPER:
            defs = WHISPER_MODELS;
            def_count = WHISPER_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_VOSK:
            defs = VOSK_MODELS;
            def_count = VOSK_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_PIPER:
            defs = PIPER_MODELS;
            def_count = PIPER_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE:
            *models = NULL;
            *count = 0;
            return ETHERVOX_SUCCESS;  // Wake templates are custom
    }
    
    ethervox_model_info_t* result = calloc(def_count, sizeof(ethervox_model_info_t));
    if (!result) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate model list");
    }
    
    for (uint32_t i = 0; i < def_count; i++) {
        ethervox_model_check_status(type, defs[i].name, &result[i]);
    }
    
    *models = result;
    *count = def_count;
    
    return ETHERVOX_SUCCESS;
}

int ethervox_model_download(
    ethervox_model_type_t type,
    const char* model_name,
    ethervox_download_progress_callback_t progress_callback,
    void* user_data
) {
    (void)progress_callback;
    (void)user_data;
    
    const model_definition_t* def = get_model_definition(type, model_name, NULL);
    if (!def) {
        ETHERVOX_LOG_ERROR("Unknown model: %s", model_name ? model_name : "(default)");
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_FOUND, "Model not found");
    }
    
    char base_dir[512];
    ethervox_result_t result = ethervox_model_get_base_dir(base_dir, sizeof(base_dir));
    if (ethervox_is_error(result)) {
        ETHERVOX_LOG_ERROR("Failed to get model base directory");
        return result;
    }
    
    // Create subdirectory
    const char* subdir = "";
    switch (type) {
        case ETHERVOX_MODEL_TYPE_GOVERNOR: subdir = "governor"; break;
        case ETHERVOX_MODEL_TYPE_WHISPER: subdir = "whisper"; break;
        case ETHERVOX_MODEL_TYPE_VOSK: subdir = "vosk"; break;
        case ETHERVOX_MODEL_TYPE_PIPER: subdir = "piper"; break;
        case ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE: subdir = "wake_templates"; break;
    }
    
    char model_dir[512];
    snprintf(model_dir, sizeof(model_dir), "%s/%s", base_dir, subdir);
#ifdef _WIN32
    _mkdir(model_dir);
#else
    mkdir(model_dir, 0755);
#endif
    
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/%s", model_dir, def->name);
    
    // Check if this is a Vosk ZIP model (not yet supported)
    if (type == ETHERVOX_MODEL_TYPE_VOSK && strstr(def->url, ".zip")) {
        ETHERVOX_LOG_ERROR("Vosk models require manual extraction (ZIP support not yet implemented)");
        ETHERVOX_LOG_INFO("Please download and extract manually:");
        ETHERVOX_LOG_INFO("  1. Download: %s", def->url);
        ETHERVOX_LOG_INFO("  2. Extract to: %s/", model_dir);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_IMPLEMENTED, "ZIP extraction not implemented - please extract manually");
    }
    
#if HAVE_LIBCURL
    // Use native C HTTP download
    ETHERVOX_LOG_INFO("Downloading %s...", def->name);
    ETHERVOX_LOG_DEBUG("URL: %s", def->url);
    ETHERVOX_LOG_DEBUG("Destination: %s", output_path);
    
    ethervox_result_t download_result = platform_http_download(
        def->url,
        output_path,
        NULL,  // No progress callback for now
        NULL   // No user data
    );
    
    if (ethervox_is_error(download_result)) {
        ETHERVOX_LOG_ERROR("Download failed");
        return download_result;
    }
#else
    // Fallback: Inform user to download manually
    ETHERVOX_LOG_ERROR("libcurl not available - manual download required");
    ETHERVOX_LOG_INFO("Please download manually:");
    ETHERVOX_LOG_INFO("  URL: %s", def->url);
    ETHERVOX_LOG_INFO("  Save to: %s", output_path);
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_IMPLEMENTED, "libcurl not available - please download manually");
#endif
    
    ETHERVOX_LOG_INFO("Download complete: %s", def->name);
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_model_cancel_download(
    ethervox_model_type_t type,
    const char* model_name
) {
    (void)type;
    (void)model_name;
    
    // TODO: Implement download cancellation
    // Would require tracking active downloads in a global registry
    ETHERVOX_LOG_WARN("Download cancellation not yet implemented");
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_IMPLEMENTED, "Download cancellation not implemented");
}

ethervox_result_t ethervox_model_delete(
    ethervox_model_type_t type,
    const char* model_name
) {
    char base_dir[512];
    ethervox_result_t result = ethervox_model_get_base_dir(base_dir, sizeof(base_dir));
    if (ethervox_is_error(result)) {
        return result;
    }
    
    const char* subdir = "";
    switch (type) {
        case ETHERVOX_MODEL_TYPE_GOVERNOR: subdir = "governor"; break;
        case ETHERVOX_MODEL_TYPE_WHISPER: subdir = "whisper"; break;
        case ETHERVOX_MODEL_TYPE_VOSK: subdir = "vosk"; break;
        case ETHERVOX_MODEL_TYPE_PIPER: subdir = "piper"; break;
        case ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE: subdir = "wake_templates"; break;
    }
    
    char model_path[1024];
    snprintf(model_path, sizeof(model_path), "%s/%s/%s", base_dir, subdir, model_name);
    
    if (type == ETHERVOX_MODEL_TYPE_VOSK) {
        // Delete directory recursively using C function
        ethervox_result_t result = platform_rmdir_recursive(model_path);
        if (ethervox_is_error(result)) {
            ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_DELETE_FAILED, "Failed to delete model directory");
        }
        return ETHERVOX_SUCCESS;
    } else {
        // Delete file
        if (unlink(model_path) != 0) {
            ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_DELETE_FAILED, "Failed to delete model file");
        }
        return ETHERVOX_SUCCESS;
    }
}

bool ethervox_model_verify(
    ethervox_model_type_t type,
    const char* model_path
) {
    if (!model_path) {
        return false;
    }
    
    // Basic verification: check if file/directory exists and has reasonable size
    struct stat st;
    if (stat(model_path, &st) != 0) {
        return false;
    }
    
    if (type == ETHERVOX_MODEL_TYPE_VOSK) {
        // Vosk models should be directories with specific structure
        if (!S_ISDIR(st.st_mode)) {
            return false;
        }
        
        // Check for required files
        char conf_path[1024];
        snprintf(conf_path, sizeof(conf_path), "%s/conf/mfcc.conf", model_path);
        if (!file_exists(conf_path)) {
            return false;
        }
    } else {
        // Other models should be files
        if (!S_ISREG(st.st_mode)) {
            return false;
        }
        
        // Check minimum size (1KB)
        if (st.st_size < 1024) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Convenience Functions
// ============================================================================

ethervox_model_status_t ethervox_model_governor_status(const char* model_name) {
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_GOVERNOR, model_name, NULL);
}

ethervox_model_status_t ethervox_model_whisper_status(const char* model_name) {
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_WHISPER, model_name, NULL);
}

ethervox_model_status_t ethervox_model_vosk_status(const char* model_name) {
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_VOSK, model_name, NULL);
}

ethervox_model_status_t ethervox_model_piper_status(const char* model_name) {
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_PIPER, model_name, NULL);
}

ethervox_model_status_t ethervox_model_wake_template_status(const char* wake_word) {
    if (!wake_word) {
        return ETHERVOX_MODEL_STATUS_NOT_FOUND;
    }
    
    // Convert wake word to filename (replace spaces with underscores)
    char filename[128];
    snprintf(filename, sizeof(filename), "%s.raw", wake_word);
    for (char* p = filename; *p; p++) {
        if (*p == ' ') *p = '_';
    }
    
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE, filename, NULL);
}

const char* ethervox_model_status_string(ethervox_model_status_t status) {
    switch (status) {
        case ETHERVOX_MODEL_STATUS_NOT_FOUND: return "Not Found";
        case ETHERVOX_MODEL_STATUS_FOUND: return "Found";
        case ETHERVOX_MODEL_STATUS_CORRUPT: return "Corrupt";
        case ETHERVOX_MODEL_STATUS_DOWNLOADING: return "Downloading";
        case ETHERVOX_MODEL_STATUS_INCOMPLETE: return "Incomplete";
        case ETHERVOX_MODEL_STATUS_UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}

const char* ethervox_model_type_string(ethervox_model_type_t type) {
    switch (type) {
        case ETHERVOX_MODEL_TYPE_GOVERNOR: return "Governor LLM";
        case ETHERVOX_MODEL_TYPE_WHISPER: return "Whisper STT";
        case ETHERVOX_MODEL_TYPE_VOSK: return "Vosk STT";
        case ETHERVOX_MODEL_TYPE_PIPER: return "Piper TTS";
        case ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE: return "Wake Template";
        default: return "Unknown";
    }
}

ethervox_result_t ethervox_model_get_disk_usage(uint64_t* bytes_used) {
    ETHERVOX_CHECK_PTR(bytes_used);
    
    char base_dir[512];
    ethervox_result_t result = ethervox_model_get_base_dir(base_dir, sizeof(base_dir));
    if (ethervox_is_error(result)) {
        return result;
    }
    
    *bytes_used = get_dir_size(base_dir);
    return ETHERVOX_SUCCESS;
}

bool ethervox_model_check_disk_space(
    ethervox_model_type_t type,
    const char* model_name
) {
    const model_definition_t* def = get_model_definition(type, model_name, NULL);
    if (!def || def->size_bytes == 0) {
        return true;  // Unknown size, assume OK
    }
    
    char base_dir[512];
    if (ethervox_is_error(ethervox_model_get_base_dir(base_dir, sizeof(base_dir)))) {
        return false;
    }
    
    // Use cross-platform utility
    uint64_t available = 0;
    if (ethervox_is_error(platform_get_disk_space(base_dir, &available))) {
        return false;
    }
    
    // Require 20% extra space as safety margin
    uint64_t required = (uint64_t)(def->size_bytes * 1.2);
    return available >= required;
}
