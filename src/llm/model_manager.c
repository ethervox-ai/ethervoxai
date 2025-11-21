/**
 * @file model_manager.c
 * @brief Model download and management implementation for EthervoxAI
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
#include "ethervox/model_manager.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
#include <unistd.h>
#include <sys/statvfs.h>
#endif

// Platform-specific HTTP download support
#ifdef _WIN32
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#define USE_WININET
#elif defined(__linux__) || defined(__APPLE__)
/* Check if curl/curl.h is available (may not be present in cross-compilation environments) */
#if __has_include(<curl/curl.h>)
#include <curl/curl.h>
#define USE_LIBCURL
#define CURL_AVAILABLE 1
#else
/* curl not available - HTTP download functionality will be disabled */
#ifdef _MSC_VER
#pragma message("curl/curl.h not found - HTTP download functionality disabled")
#else
#warning "curl/curl.h not found - HTTP download functionality disabled"
#endif
#define CURL_AVAILABLE 0
#endif
#endif

// Predefined model registry
const ethervox_model_info_t ETHERVOX_MODEL_TINYLLAMA_1B_Q4 = {
    .name = "TinyLlama-1.1B-Chat-Q4_K_M",
    .description = "TinyLlama 1.1B Chat model, Q4_K_M quantized (recommended for embedded)",
    .url = "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
    .filename = "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
    .sha256 = NULL,  // Optional: Add checksum for verification
    .size_bytes = 669000000,  // ~637 MB
    .format = "GGUF",
    .quantization = "Q4_K_M",
    .recommended_for_embedded = true,
    .min_ram_mb = 2048
};

const ethervox_model_info_t ETHERVOX_MODEL_PHI2_Q4 = {
    .name = "Phi-2-Q4_K_M",
    .description = "Microsoft Phi-2 2.7B model, Q4_K_M quantized",
    .url = "https://huggingface.co/TheBloke/phi-2-GGUF/resolve/main/phi-2.Q4_K_M.gguf",
    .filename = "phi-2.Q4_K_M.gguf",
    .sha256 = NULL,
    .size_bytes = 1600000000,  // ~1.6 GB
    .format = "GGUF",
    .quantization = "Q4_K_M",
    .recommended_for_embedded = false,
    .min_ram_mb = 4096
};

const ethervox_model_info_t ETHERVOX_MODEL_MISTRAL_7B_Q4 = {
    .name = "Mistral-7B-Instruct-Q4_K_M",
    .description = "Mistral 7B Instruct model, Q4_K_M quantized",
    .url = "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf",
    .filename = "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
    .sha256 = NULL,
    .size_bytes = 4370000000,  // ~4.1 GB
    .format = "GGUF",
    .quantization = "Q4_K_M",
    .recommended_for_embedded = false,
    .min_ram_mb = 8192
};

const ethervox_model_info_t ETHERVOX_MODEL_LLAMA2_7B_Q4 = {
    .name = "Llama-2-7B-Chat-Q4_K_M",
    .description = "Llama 2 7B Chat model, Q4_K_M quantized",
    .url = "https://huggingface.co/TheBloke/Llama-2-7B-Chat-GGUF/resolve/main/llama-2-7b-chat.Q4_K_M.gguf",
    .filename = "llama-2-7b-chat.Q4_K_M.gguf",
    .sha256 = NULL,
    .size_bytes = 4080000000,  // ~3.8 GB
    .format = "GGUF",
    .quantization = "Q4_K_M",
    .recommended_for_embedded = false,
    .min_ram_mb = 8192
};

// Model manager structure
struct ethervox_model_manager {
    char* models_dir;
    char* cache_dir;
    bool auto_download;
    bool verify_checksum;
    uint32_t max_retries;
    uint32_t timeout_seconds;
    ethervox_download_progress_callback_t progress_callback;
    void* callback_user_data;
};

// Download context for callbacks
typedef struct {
    ethervox_model_manager_t* manager;
    const ethervox_model_info_t* model_info;
    FILE* fp;
    uint64_t downloaded;
    uint64_t total;
    bool cancelled;
} download_context_t;

#ifdef USE_LIBCURL
// libcurl progress callback - renamed to avoid conflict with curl typedef
static int ethervox_curl_progress_cb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                     curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;
    
    download_context_t* ctx = (download_context_t*)clientp;
    
    if (ctx->manager->progress_callback && dltotal > 0) {
        float progress = (float)dlnow / (float)dltotal * 100.0f;
        ctx->manager->progress_callback(
            ctx->model_info->name,
            (uint64_t)dlnow,
            (uint64_t)dltotal,
            progress,
            ctx->manager->callback_user_data
        );
    }
    
    return ctx->cancelled ? 1 : 0;  // Return 1 to abort
}

// libcurl write callback - renamed to avoid conflict with curl typedef
static size_t ethervox_curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    download_context_t* ctx = (download_context_t*)userdata;
    size_t written = fwrite(ptr, size, nmemb, ctx->fp);
    ctx->downloaded += written;
    return written;
}
#endif

ethervox_model_manager_config_t ethervox_model_manager_get_default_config(void) {
    ethervox_model_manager_config_t config;
    config.models_dir = "models";
    config.cache_dir = "models/.cache";
    config.auto_download = true;
    config.verify_checksum = false;  // Checksums not implemented yet
    config.max_retries = 3;
    config.timeout_seconds = 3600;  // 1 hour
    config.progress_callback = NULL;
    config.callback_user_data = NULL;
    return config;
}

ethervox_model_manager_t* ethervox_model_manager_create(
    const ethervox_model_manager_config_t* config) {
    
    ethervox_model_manager_t* manager = (ethervox_model_manager_t*)calloc(
        1, sizeof(ethervox_model_manager_t));
    
    if (!manager) {
        ETHERVOX_LOG_ERROR("Failed to allocate model manager");
        return NULL;
    }
    
    // Set configuration
    if (config) {
        manager->models_dir = config->models_dir ? strdup(config->models_dir) : strdup("models");
        manager->cache_dir = config->cache_dir ? strdup(config->cache_dir) : strdup("models/.cache");
        manager->auto_download = config->auto_download;
        manager->verify_checksum = config->verify_checksum;
        manager->max_retries = config->max_retries > 0 ? config->max_retries : 3;
        manager->timeout_seconds = config->timeout_seconds > 0 ? config->timeout_seconds : 3600;
        manager->progress_callback = config->progress_callback;
        manager->callback_user_data = config->callback_user_data;
    } else {
        ethervox_model_manager_config_t default_config = ethervox_model_manager_get_default_config();
        manager->models_dir = strdup(default_config.models_dir);
        manager->cache_dir = strdup(default_config.cache_dir);
        manager->auto_download = default_config.auto_download;
        manager->verify_checksum = default_config.verify_checksum;
        manager->max_retries = default_config.max_retries;
        manager->timeout_seconds = default_config.timeout_seconds;
        manager->progress_callback = NULL;
        manager->callback_user_data = NULL;
    }
    
    // Create directories if they don't exist
    if (mkdir(manager->models_dir, 0755) != 0 && errno != EEXIST) {
        ETHERVOX_LOG_ERROR("Failed to create models directory '%s': %s", manager->models_dir, strerror(errno));
        free(manager->models_dir);
        free(manager->cache_dir);
        free(manager);
        return NULL;
    }
    if (mkdir(manager->cache_dir, 0755) != 0 && errno != EEXIST) {
        ETHERVOX_LOG_ERROR("Failed to create cache directory '%s': %s", manager->cache_dir, strerror(errno));
        free(manager->models_dir);
        free(manager->cache_dir);
        free(manager);
        return NULL;
    }
    
    ETHERVOX_LOG_INFO("Model manager created (models_dir=%s)", manager->models_dir);
    
    return manager;
}

void ethervox_model_manager_destroy(ethervox_model_manager_t* manager) {
    if (!manager) {
        return;
    }
    
    free(manager->models_dir);
    free(manager->cache_dir);
    free(manager);
}

const char* ethervox_model_status_to_string(ethervox_model_status_t status) {
    switch (status) {
        case ETHERVOX_MODEL_STATUS_NOT_FOUND: return "Not Found";
        case ETHERVOX_MODEL_STATUS_DOWNLOADING: return "Downloading";
        case ETHERVOX_MODEL_STATUS_AVAILABLE: return "Available";
        case ETHERVOX_MODEL_STATUS_CORRUPTED: return "Corrupted";
        case ETHERVOX_MODEL_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

ethervox_model_status_t ethervox_model_manager_get_status(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info) {
    
    if (!manager || !model_info) {
        return ETHERVOX_MODEL_STATUS_ERROR;
    }
    
    // Build full path
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", manager->models_dir, model_info->filename);
    
    // Check if file exists
    struct stat st;
    if (stat(path, &st) != 0) {
        return ETHERVOX_MODEL_STATUS_NOT_FOUND;
    }
    
    // Verify it's a regular file
    if (!S_ISREG(st.st_mode)) {
        return ETHERVOX_MODEL_STATUS_ERROR;
    }
    
    // Check file size (basic corruption detection)
    if (model_info->size_bytes > 0) {
        if ((uint64_t)st.st_size < model_info->size_bytes * 0.95) {  // Allow 5% variance
            ETHERVOX_LOG_WARN("Model file size mismatch (expected ~%llu, got %llu)",
                             (unsigned long long)model_info->size_bytes,
                             (unsigned long long)st.st_size);
            return ETHERVOX_MODEL_STATUS_CORRUPTED;
        }
    }
    
    return ETHERVOX_MODEL_STATUS_AVAILABLE;
}

int ethervox_model_manager_get_path(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info,
    char* path_buffer,
    size_t buffer_size) {
    
    if (!manager || !model_info || !path_buffer || buffer_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int written = snprintf(path_buffer, buffer_size, "%s/%s", 
                          manager->models_dir, model_info->filename);
    
    if (written < 0 || (size_t)written >= buffer_size) {
        ETHERVOX_LOG_ERROR("Path buffer too small (need %d bytes, have %zu)", written + 1, buffer_size);
        return ETHERVOX_ERROR_FAILED;
    }
    
    return ETHERVOX_SUCCESS;
}

bool ethervox_model_manager_is_available(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info) {
    
    ethervox_model_status_t status = ethervox_model_manager_get_status(manager, model_info);
    return status == ETHERVOX_MODEL_STATUS_AVAILABLE;
}

#ifdef USE_LIBCURL
static int download_with_curl(ethervox_model_manager_t* manager,
                              const ethervox_model_info_t* model_info,
                              const char* dest_path) {
    CURL* curl;
    CURLcode res;
    
    curl = curl_easy_init();
    if (!curl) {
        ETHERVOX_LOG_ERROR("Failed to initialize CURL");
        return ETHERVOX_ERROR_FAILED;
    }
    
    // Open file for writing (portable)
    FILE* fp = fopen(dest_path, "wb");
    if (!fp) {
        ETHERVOX_LOG_ERROR("Failed to create file for writing: %s", dest_path);
        curl_easy_cleanup(curl);
        return ETHERVOX_ERROR_FAILED;
    }
    
    // Setup download context
    download_context_t ctx;
    ctx.manager = manager;
    ctx.model_info = model_info;
    ctx.fp = fp;
    ctx.downloaded = 0;
    ctx.total = model_info->size_bytes;
    ctx.cancelled = false;
    
    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, model_info->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ethervox_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ethervox_curl_progress_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)manager->timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "EthervoxAI-ModelManager/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    ETHERVOX_LOG_INFO("Starting download: %s", model_info->url);
    
    // Perform download
    res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        ETHERVOX_LOG_ERROR("Download failed: %s", curl_easy_strerror(res));
        unlink(dest_path);  // Delete partial file
        return ETHERVOX_ERROR_FAILED;
    }
    
    ETHERVOX_LOG_INFO("Download completed successfully");
    return ETHERVOX_SUCCESS;
}
#elif defined(USE_WININET)
static int download_with_wininet(ethervox_model_manager_t* manager,
                                 const ethervox_model_info_t* model_info,
                                 const char* dest_path) {
    HINTERNET hInternet = NULL;
    HINTERNET hUrl = NULL;
    FILE* fp = NULL;
    int result = ETHERVOX_ERROR_FAILED;
    
    // Initialize WinINet
    hInternet = InternetOpenA("EthervoxAI-ModelManager/1.0",
                             INTERNET_OPEN_TYPE_PRECONFIG,
                             NULL, NULL, 0);
    if (!hInternet) {
        ETHERVOX_LOG_ERROR("Failed to initialize WinINet");
        return ETHERVOX_ERROR_FAILED;
    }
    
    // Open URL
    hUrl = InternetOpenUrlA(hInternet, model_info->url, NULL, 0,
                           INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        ETHERVOX_LOG_ERROR("Failed to open URL: %s", model_info->url);
        InternetCloseHandle(hInternet);
        return ETHERVOX_ERROR_FAILED;
    }
    
    // Open file for writing
    fp = fopen(dest_path, "wb");
    if (!fp) {
        ETHERVOX_LOG_ERROR("Failed to open file for writing: %s", dest_path);
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return ETHERVOX_ERROR_FAILED;
    }
    
    ETHERVOX_LOG_INFO("Starting download: %s", model_info->url);
    
    // Download in chunks
    char buffer[8192];
    DWORD bytes_read;
    uint64_t total_downloaded = 0;
    
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        fwrite(buffer, 1, bytes_read, fp);
        total_downloaded += bytes_read;
        
        // Report progress
        if (manager->progress_callback && model_info->size_bytes > 0) {
            float progress = (float)total_downloaded / (float)model_info->size_bytes * 100.0f;
            manager->progress_callback(
                model_info->name,
                total_downloaded,
                model_info->size_bytes,
                progress,
                manager->callback_user_data
            );
        }
    }
    
    fclose(fp);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    
    ETHERVOX_LOG_INFO("Download completed: %llu bytes", (unsigned long long)total_downloaded);
    result = ETHERVOX_SUCCESS;
    
    return result;
}
#endif

int ethervox_model_manager_download(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info) {
    
    if (!manager || !model_info) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if already available
    if (ethervox_model_manager_is_available(manager, model_info)) {
        ETHERVOX_LOG_INFO("Model already available: %s", model_info->name);
        return ETHERVOX_SUCCESS;
    }
    
    // Check available disk space
    if (model_info->size_bytes > 0) {
        if (!ethervox_model_manager_has_enough_space(manager->models_dir, model_info->size_bytes)) {
            ETHERVOX_LOG_ERROR("Insufficient disk space for model download");
            return ETHERVOX_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // Build destination path
    char dest_path[1024];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", manager->models_dir, model_info->filename);
    
    ETHERVOX_LOG_INFO("Downloading model: %s", model_info->name);
    ETHERVOX_LOG_INFO("Size: %.2f MB", (float)model_info->size_bytes / 1024.0f / 1024.0f);
    ETHERVOX_LOG_INFO("Destination: %s", dest_path);
    
#if defined(USE_LIBCURL)
    return download_with_curl(manager, model_info, dest_path);
#elif defined(USE_WININET)
    return download_with_wininet(manager, model_info, dest_path);
#else
    ETHERVOX_LOG_ERROR("No HTTP download support available (missing libcurl or WinINet)");
    ETHERVOX_LOG_INFO("Please manually download from: %s", model_info->url);
    ETHERVOX_LOG_INFO("Save to: %s", dest_path);
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
#endif
}

int ethervox_model_manager_ensure_available(
    ethervox_model_manager_t* manager,
    const ethervox_model_info_t* model_info) {
    
    if (!manager || !model_info) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if already available
    if (ethervox_model_manager_is_available(manager, model_info)) {
        return ETHERVOX_SUCCESS;
    }
    
    // Auto-download if enabled
    if (manager->auto_download) {
        ETHERVOX_LOG_INFO("Model not found, auto-downloading: %s", model_info->name);
        return ethervox_model_manager_download(manager, model_info);
    }
    
    ETHERVOX_LOG_WARN("Model not available and auto-download disabled: %s", model_info->name);
    return ETHERVOX_ERROR_NOT_FOUND;
}

uint64_t ethervox_model_manager_get_available_space(const char* path) {
#ifdef _WIN32
    ULARGE_INTEGER free_bytes;
    if (GetDiskFreeSpaceExA(path, &free_bytes, NULL, NULL)) {
        return (uint64_t)free_bytes.QuadPart;
    }
    return 0;
#else
    struct statvfs stat;
    if (statvfs(path, &stat) == 0) {
        return (uint64_t)stat.f_bavail * stat.f_frsize;
    }
    return 0;
#endif
}

bool ethervox_model_manager_has_enough_space(const char* path, uint64_t required_bytes) {
    uint64_t available = ethervox_model_manager_get_available_space(path);
    // Add 10% buffer to required space for safety.
    // Rationale: Filesystem overhead, fragmentation, and other uncertainties may cause actual usage to exceed the nominal model size.
    // The overflow check ensures that multiplying by 11 does not exceed UINT64_MAX, which would cause integer overflow.
    uint64_t required_with_buffer = (required_bytes > UINT64_MAX / 11)
        ? UINT64_MAX
        : (required_bytes * 11) / 10;
    return available >= required_with_buffer;
}

int ethervox_model_manager_delete_model(
    ethervox_model_manager_t* g_manager,
    const ethervox_model_info_t* model_info) {
    
    if (!g_manager || !model_info) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char path[1024];
    int result = ethervox_model_manager_get_path(g_manager, model_info, path, sizeof(path));
    if (result != ETHERVOX_SUCCESS) {
        return result;
    }
    
    if (unlink(path) != 0) {
        ETHERVOX_LOG_ERROR("Failed to delete model: %s", strerror(errno));
        return ETHERVOX_ERROR_FAILED;
    }
    
    ETHERVOX_LOG_INFO("Model deleted: %s", model_info->name);
    return ETHERVOX_SUCCESS;
}

int ethervox_model_manager_clean_cache(ethervox_model_manager_t* manager) {
    if (!manager) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // TODO: Implement cache cleaning
    ETHERVOX_LOG_INFO("Cache cleaned: %s", manager->cache_dir);
    return ETHERVOX_SUCCESS;
}
