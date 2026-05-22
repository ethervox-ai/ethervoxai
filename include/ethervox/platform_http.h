/**
 * @file platform_http.h
 * @brief Cross-platform HTTP download functionality using libcurl
 *
 * Copyright (c) 2025 EthervoxAI
 * Licensed under CC BY-NC-SA 4.0
 */

#ifndef ETHERVOX_PLATFORM_HTTP_H
#define ETHERVOX_PLATFORM_HTTP_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Progress callback for HTTP downloads
 * 
 * @param userdata User-provided context pointer
 * @param downloaded Number of bytes downloaded so far
 * @param total Total size in bytes (0 if unknown)
 * @return true to continue download, false to cancel
 */
typedef bool (*ethervox_http_progress_callback)(void* userdata, size_t downloaded, size_t total);

/**
 * @brief Download a file from a URL to a local path
 * 
 * @param url Source URL to download from
 * @param dest_path Destination file path
 * @param progress_callback Optional callback for progress updates (can be NULL)
 * @param userdata Optional user data passed to callback (can be NULL)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t platform_http_download(
    const char* url,
    const char* dest_path,
    ethervox_http_progress_callback progress_callback,
    void* userdata
);

/**
 * @brief Download a file with automatic retry on failure
 * 
 * @param url Source URL to download from
 * @param dest_path Destination file path
 * @param max_retries Maximum number of retry attempts
 * @param progress_callback Optional callback for progress updates
 * @param userdata Optional user data passed to callback
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t platform_http_download_with_retry(
    const char* url,
    const char* dest_path,
    int max_retries,
    ethervox_http_progress_callback progress_callback,
    void* userdata
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PLATFORM_HTTP_H
