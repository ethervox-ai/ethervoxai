/**
 * @file timer_tools.h
 * @brief Timer and alarm management tools header
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_TIMER_TOOLS_H
#define ETHERVOX_TIMER_TOOLS_H

#include "ethervox/governor.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get timer creation tool
 * Creates countdown timers with specified duration
 */
const ethervox_tool_t* ethervox_tool_timer_create(void);

/**
 * Get timer cancel tool
 * Cancels an active timer by ID
 */
const ethervox_tool_t* ethervox_tool_timer_cancel(void);

/**
 * Get timer list tool
 * Lists all active timers with remaining time
 */
const ethervox_tool_t* ethervox_tool_timer_list(void);

/**
 * Get alarm creation tool
 * Creates alarms for specific times (24-hour format)
 */
const ethervox_tool_t* ethervox_tool_alarm_create(void);

/**
 * Check for triggered timers (system polling)
 * @param count Output parameter for number of triggered timers
 * @return Array of triggered timer IDs, caller must free. NULL if none triggered.
 */
int* ethervox_timer_check_triggered(int* count);

/**
 * Get timer information
 * @param timer_id Timer ID to query
 * @param label_out Buffer for timer label (can be NULL)
 * @param label_size Size of label buffer
 * @param remaining_seconds Output for remaining seconds (can be NULL)
 * @return 0 on success, -1 if timer not found
 */
int ethervox_timer_get_info(int timer_id, char* label_out, size_t label_size, int* remaining_seconds);

/**
 * Get first active timer status as JSON (for UI display)
 * @return JSON string with timer info, caller must free. Returns {\"has_timer\": false} if no active timer.
 */
char* ethervox_timer_get_active_status(void);

#ifdef __cplusplus
}
#endif

#endif  // ETHERVOX_TIMER_TOOLS_H
