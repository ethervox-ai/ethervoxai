/**
 * @file bug_reporter.h
 * @brief Anonymous bug and feature request reporting to GitHub Issues
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_BUG_REPORTER_H
#define ETHERVOX_BUG_REPORTER_H

#include <stdbool.h>
#include <stddef.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Report type
 */
typedef enum {
    ETHERVOX_REPORT_BUG,
    ETHERVOX_REPORT_FEATURE
} ethervox_report_type_t;

/**
 * @brief Bug/feature report result
 */
typedef struct {
    bool success;
    char issue_url[512];      /**< GitHub issue URL if successful */
    char error_message[256];  /**< Error message if failed */
    int http_status;          /**< HTTP status code */
} ethervox_report_result_t;

/**
 * @brief Submit a bug report or feature request to GitHub
 * 
 * @param type Report type (bug or feature)
 * @param title Brief title (max 100 chars recommended)
 * @param description Detailed description (markdown supported)
 * @param include_system_info Include device/system information
 * @param result Output result structure
 * @return 0 on success, -1 on error
 */
ethervox_result_t ethervox_report_submit(
    ethervox_report_type_t type,
    const char* title,
    const char* description,
    bool include_system_info,
    ethervox_report_result_t* result
);

/**
 * @brief Get system information string for reports
 * 
 * Includes:
 * - OS, architecture, machine type, hostname
 * - App version, git branch, git commit
 * - Whisper STT configuration (model, language, temperature, etc.)
 * - Conversation settings (timeouts, thresholds, filters)
 * - Wake word configuration (phrase, detection threshold, etc.)
 * 
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written
 */
ethervox_result_t ethervox_report_get_system_info(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_BUG_REPORTER_H
