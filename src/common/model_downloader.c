/**
 * @file model_downloader.c
 * @brief Model download and status checking implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Proprietary and confidential. See LICENSE.
 */

#include "ethervox/model_downloader.h"
#include "ethervox/config.h"
#include "ethervox/logging.h"
#include "ethervox/platform.h"
#include "ethervox/platform_utils.h"
#include "ethervox/error.h"

#if HAVE_LIBCURL && ETHERVOX_FEATURE_HTTP
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
        "https://huggingface.co/ibm-granite/granite-4.0-h-1b-GGUF/resolve/main/granite-4.0-h-1b-Q4_K_M.gguf",
        901162208,  // ~901MB (official IBM model, matches asset pack)
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

// Granite Speech ASR models (BASE variant - Modes 1 & 4, punctuated ASR/AST).
// Ships as a (model, mmproj) GGUF pair - both entries marked is_default so the
// "download recommended models" flow fetches both in one pass, mirroring how
// PIPER_MODELS below already pairs an .onnx model with its .onnx.json config.
static const model_definition_t GRANITE_SPEECH_MODELS[] = {
    {
        "granite-speech-4.1-2b.Q4_K_M.gguf",
        "IBM Granite Speech 4.1 2B (Recommended) - punctuated ASR + translation",
        "https://huggingface.co/ibm-granite/granite-speech-4.1-2b-GGUF/resolve/main/granite-speech-4.1-2b.Q4_K_M.gguf",
        1200000000,  // ~1.2GB
        true
    },
    {
        "mmproj-granite-speech-4.1-2b-Q4_K_M.gguf",
        "IBM Granite Speech 4.1 2B mmproj (audio projector) - required companion file",
        "https://huggingface.co/ibm-granite/granite-speech-4.1-2b-GGUF/resolve/main/mmproj-granite-speech-4.1-2b-Q4_K_M.gguf",
        300000000,  // ~300MB
        true
    }
};

// Granite Speech PLUS models (Mode 2 - Speaker-Attributed ASR, replaces
// Whisper AND any separate diarization heuristic). Same (model, mmproj) pair
// pattern as GRANITE_SPEECH_MODELS above.
static const model_definition_t GRANITE_SPEECH_PLUS_MODELS[] = {
    {
        // Remote name is dash-separated ("...-plus-Q4_K_M.gguf") - see docs/MODELS.md §3, the
        // filename trap. Local name (this struct's first field) stays dot-separated to match the
        // hardcoded paths this same file, voice_tools.c and ethervox_android_core.c all use.
        "granite-speech-4.1-2b-plus.Q4_K_M.gguf",
        "IBM Granite Speech 4.1 2B Plus (Recommended) - speaker-attributed ASR (SAA)",
        "https://huggingface.co/ibm-granite/granite-speech-4.1-2b-plus-GGUF/resolve/main/granite-speech-4.1-2b-plus-Q4_K_M.gguf?download=true",
        1024458752,  // 977 MB - see docs/MODELS.md §2 and ModelConfig.kt's sizeMB
        true
    },
    {
        // Remote name is the repo's actual "mmproj-model-f16.gguf", not a per-model name - see
        // docs/MODELS.md §3. Local name (this struct's first field) is our own convention.
        "mmproj-granite-speech-4.1-2b-plus-Q4_K_M.gguf",
        "IBM Granite Speech 4.1 2B Plus mmproj (audio projector) - required companion file",
        "https://huggingface.co/ibm-granite/granite-speech-4.1-2b-plus-GGUF/resolve/main/mmproj-model-f16.gguf?download=true",
        1168113664,  // 1114 MB - larger than the model itself, see docs/MODELS.md §2
        true
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
#define GRANITE_SPEECH_MODEL_COUNT (sizeof(GRANITE_SPEECH_MODELS) / sizeof(GRANITE_SPEECH_MODELS[0]))
#define GRANITE_SPEECH_PLUS_MODEL_COUNT (sizeof(GRANITE_SPEECH_PLUS_MODELS) / sizeof(GRANITE_SPEECH_PLUS_MODELS[0]))
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
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH:
            models = GRANITE_SPEECH_MODELS;
            count = GRANITE_SPEECH_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS:
            models = GRANITE_SPEECH_PLUS_MODELS;
            count = GRANITE_SPEECH_PLUS_MODEL_COUNT;
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
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH:
            subdir = ETHERVOX_GRANITE_SPEECH_SUBDIR;
            break;
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS:
            subdir = ETHERVOX_GRANITE_SPEECH_SUBDIR;  // Shares a subdir with BASE (see config.h)
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
    
    // Check if model exists (all current model types are single files -
    // Vosk's directory-based models were the only exception and have been
    // removed along with Vosk itself)
    bool exists = file_exists(model_path);
    uint64_t size = exists ? get_file_size(model_path) : 0;
    
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
    
    // Check if size matches expected (within 10% tolerance)
    ethervox_model_status_t status = ETHERVOX_MODEL_STATUS_FOUND;
    if (def && def->size_bytes > 0) {
        uint64_t min_size = (uint64_t)(def->size_bytes * 0.9);
        uint64_t max_size = (uint64_t)(def->size_bytes * 1.1);
        
        if (size < min_size) {
            status = ETHERVOX_MODEL_STATUS_INCOMPLETE;
        } else if (size > max_size) {
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
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH:
            defs = GRANITE_SPEECH_MODELS;
            def_count = GRANITE_SPEECH_MODEL_COUNT;
            break;
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS:
            defs = GRANITE_SPEECH_PLUS_MODELS;
            def_count = GRANITE_SPEECH_PLUS_MODEL_COUNT;
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

#if !ETHERVOX_FEATURE_DOWNLOADER
    ETHERVOX_LOG_ERROR("Model downloading is disabled in this build (ETHERVOX_FEATURE_DOWNLOADER=OFF)");
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FEATURE_DISABLED, "Model downloading is disabled in this build");
#endif

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
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH: subdir = ETHERVOX_GRANITE_SPEECH_SUBDIR; break;
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS: subdir = ETHERVOX_GRANITE_SPEECH_SUBDIR; break;
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
    
#if HAVE_LIBCURL && ETHERVOX_FEATURE_HTTP
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
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH: subdir = ETHERVOX_GRANITE_SPEECH_SUBDIR; break;
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS: subdir = ETHERVOX_GRANITE_SPEECH_SUBDIR; break;
        case ETHERVOX_MODEL_TYPE_PIPER: subdir = "piper"; break;
        case ETHERVOX_MODEL_TYPE_WAKE_TEMPLATE: subdir = "wake_templates"; break;
    }
    
    char model_path[1024];
    snprintf(model_path, sizeof(model_path), "%s/%s/%s", base_dir, subdir, model_name);
    
    // All current model types are single files (Vosk's directory-based
    // models were the only exception and have been removed with Vosk itself)
    if (unlink(model_path) != 0) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_DELETE_FAILED, "Failed to delete model file");
    }
    return ETHERVOX_SUCCESS;
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
    
    (void)type;
    // All current model types are single files (Vosk's directory-based
    // models were the only exception and have been removed with Vosk itself)
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    
    // Check minimum size (1KB)
    if (st.st_size < 1024) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Convenience Functions
// ============================================================================

ethervox_model_status_t ethervox_model_governor_status(const char* model_name) {
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_GOVERNOR, model_name, NULL);
}

ethervox_model_status_t ethervox_model_granite_speech_status(const char* model_name) {
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_GRANITE_SPEECH, model_name, NULL);
}

ethervox_model_status_t ethervox_model_granite_speech_plus_status(const char* model_name) {
    return ethervox_model_check_status(ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS, model_name, NULL);
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
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH: return "Granite Speech (BASE)";
        case ETHERVOX_MODEL_TYPE_GRANITE_SPEECH_PLUS: return "Granite Speech (PLUS)";
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
