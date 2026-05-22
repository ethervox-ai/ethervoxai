/**
 * @file platform_http.c
 * @brief Cross-platform HTTP download implementation using libcurl
 *
 * Copyright (c) 2025 EthervoxAI
 * Licensed under CC BY-NC-SA 4.0
 */

#include "ethervox/platform_http.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>  // For Sleep()
#else
#include <unistd.h>   // For sleep()
#endif

// Write callback for curl
typedef struct {
    FILE* file;
    ethervox_http_progress_callback progress_callback;
    void* userdata;
    size_t downloaded;
    size_t total;
} download_context_t;

static size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    download_context_t* ctx = (download_context_t*)userdata;
    size_t written = fwrite(ptr, size, nmemb, ctx->file);
    
    ctx->downloaded += written;
    
    // Call progress callback if provided
    if (ctx->progress_callback) {
        if (!ctx->progress_callback(ctx->userdata, ctx->downloaded, ctx->total)) {
            // User requested cancellation
            return 0;
        }
    }
    
    return written;
}

static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                            curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;
    
    download_context_t* ctx = (download_context_t*)clientp;
    ctx->total = (size_t)dltotal;
    
    // Return 0 to continue, non-zero to abort
    return 0;
}

ethervox_result_t platform_http_download(
    const char* url,
    const char* dest_path,
    ethervox_http_progress_callback progress_cb,
    void* userdata
) {
    ETHERVOX_CHECK_PTR(url);
    ETHERVOX_CHECK_PTR(dest_path);
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        ETHERVOX_LOG_ERROR("Failed to initialize libcurl");
        return ETHERVOX_ERROR_NETWORK;
    }
    
    FILE* file = fopen(dest_path, "wb");
    if (!file) {
        ETHERVOX_LOG_ERROR("Failed to open destination file: %s", dest_path);
        curl_easy_cleanup(curl);
        return ETHERVOX_ERROR_FILE_WRITE;
    }
    
    download_context_t ctx = {
        .file = file,
        .progress_callback = progress_cb,
        .userdata = userdata,
        .downloaded = 0,
        .total = 0
    };
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "EthervoxAI/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);  // 5 minute timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);  // 30 second connection timeout
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, progress_cb ? 0L : 1L);
    
    if (progress_cb) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    fclose(file);
    
    if (res != CURLE_OK) {
        ETHERVOX_LOG_ERROR("Download failed: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        remove(dest_path);  // Clean up partial download
        
        if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST) {
            return ETHERVOX_ERROR_NETWORK_CONNECTION_FAILED;
        }
        return ETHERVOX_ERROR_DOWNLOAD_FAILED;
    }
    
    // Check HTTP response code
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code >= 400) {
        ETHERVOX_LOG_ERROR("HTTP error %ld downloading %s", response_code, url);
        curl_easy_cleanup(curl);
        remove(dest_path);
        return ETHERVOX_ERROR_DOWNLOAD_FAILED;
    }
    
    curl_easy_cleanup(curl);
    ETHERVOX_LOG_INFO("Successfully downloaded %s to %s", url, dest_path);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t platform_http_download_with_retry(
    const char* url,
    const char* dest_path,
    int max_retries,
    ethervox_http_progress_callback progress_cb,
    void* userdata
) {
    ETHERVOX_CHECK_PTR(url);
    ETHERVOX_CHECK_PTR(dest_path);
    
    if (max_retries < 1) {
        max_retries = 1;
    }
    
    ethervox_result_t result = ETHERVOX_ERROR_DOWNLOAD_FAILED;
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        ETHERVOX_LOG_INFO("Download attempt %d/%d: %s", attempt, max_retries, url);
        
        result = platform_http_download(url, dest_path, progress_cb, userdata);
        
        if (result == ETHERVOX_SUCCESS) {
            return ETHERVOX_SUCCESS;
        }
        
        if (attempt < max_retries) {
            ETHERVOX_LOG_WARN("Download failed, retrying...");
            // Simple exponential backoff: 1s, 2s, 4s, etc.
            #ifdef _WIN32
            Sleep((1 << (attempt - 1)) * 1000);
            #else
            sleep(1 << (attempt - 1));
            #endif
        }
    }
    
    ETHERVOX_LOG_ERROR("Download failed after %d attempts", max_retries);
    return result;
}
