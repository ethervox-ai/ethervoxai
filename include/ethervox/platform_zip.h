/**
 * @file platform_zip.h
 * @brief Cross-platform ZIP extraction functionality using miniz
 *
 * Copyright (c) 2025 EthervoxAI
 * Licensed under CC BY-NC-SA 4.0
 */

#ifndef ETHERVOX_PLATFORM_ZIP_H
#define ETHERVOX_PLATFORM_ZIP_H

#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extract a ZIP archive to a directory
 * 
 * @param zip_path Path to ZIP file
 * @param extract_dir Directory to extract contents to
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t platform_zip_extract(
    const char* zip_path,
    const char* extract_dir
);

/**
 * @brief Extract a single file from a ZIP archive
 * 
 * @param zip_path Path to ZIP file
 * @param filename Name of file inside ZIP to extract
 * @param dest_path Destination path for extracted file
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t platform_zip_extract_file(
    const char* zip_path,
    const char* filename,
    const char* dest_path
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PLATFORM_ZIP_H
