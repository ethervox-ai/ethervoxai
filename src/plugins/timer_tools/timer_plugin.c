/**
#include "ethervox/error.h"
 * @file timer_plugin.c
 * @brief Timer and alarm management tool implementation
 *
 * Provides timer/alarm functionality with support for:
 * - Creating timers with duration in seconds/minutes/hours
 * - Creating alarms with specific time
 * - Listing active timers
 * - Canceling timers
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/timer_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef ETHERVOX_PLATFORM_ANDROID
#include <android/log.h>
#define TIMER_LOG(...) __android_log_print(ANDROID_LOG_INFO, "EthervoxTimer", __VA_ARGS__)
#else
#define TIMER_LOG(...) printf("[Timer] " __VA_ARGS__); printf("\n")
#endif

// Maximum number of timers
#define MAX_TIMERS 10

typedef struct {
    int id;
    char label[128];
    time_t trigger_time;
    bool is_active;
    bool is_alarm;  // true = alarm (specific time), false = timer (duration)
} timer_entry_t;

static timer_entry_t timers[MAX_TIMERS];
static int next_timer_id = 1;

// JSON parsing helpers
static char* extract_json_string(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return NULL;
    
    const char* colon = strchr(key_pos, ':');
    if (!colon) return NULL;
    
    const char* value_start = strchr(colon, '"');
    if (!value_start) return NULL;
    value_start++;
    
    const char* value_end = strchr(value_start, '"');
    if (!value_end) return NULL;
    
    size_t len = value_end - value_start;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, value_start, len);
    result[len] = '\0';
    return result;
}

static int extract_json_int(const char* json, const char* key, int default_value) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return default_value;
    
    const char* colon = strchr(key_pos, ':');
    if (!colon) return default_value;
    
    int value;
    if (sscanf(colon + 1, "%d", &value) == 1) {
        return value;
    }
    
    return default_value;
}

/**
 * Timer Create Tool
 * JSON input: {"duration_seconds": 300, "label": "Pizza timer"}
 */
static int timer_create_execute(const char* args_json, char** result, char** error) {
    if (!args_json || !result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract parameters
    int duration = extract_json_int(args_json, "duration_seconds", 0);
    char* label = extract_json_string(args_json, "label");
    
    if (duration <= 0) {
        *error = strdup("Invalid duration - must be positive number of seconds");
        free(label);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].is_active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        *error = strdup("Maximum number of timers reached (10)");
        free(label);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Create timer
    timers[slot].id = next_timer_id++;
    timers[slot].trigger_time = time(NULL) + duration;
    timers[slot].is_active = true;
    timers[slot].is_alarm = false;
    
    if (label) {
        strncpy(timers[slot].label, label, sizeof(timers[slot].label) - 1);
        timers[slot].label[sizeof(timers[slot].label) - 1] = '\0';
        free(label);
    } else {
        snprintf(timers[slot].label, sizeof(timers[slot].label), "Timer %d", timers[slot].id);
    }
    
    TIMER_LOG("Created timer %d: %s (%d seconds)", 
              timers[slot].id, timers[slot].label, duration);
    
    // Return result
    char* result_json = malloc(256);
    if (!result_json) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    snprintf(result_json, 256, 
             "{\"success\": true, \"timer_id\": %d, \"label\": \"%s\", \"duration_seconds\": %d}",
             timers[slot].id, timers[slot].label, duration);
    *result = result_json;
    
    return ETHERVOX_SUCCESS;
}

/**
 * Timer Cancel Tool
 * JSON input: {"timer_id": 1}
 */
static int timer_cancel_execute(const char* args_json, char** result, char** error) {
    if (!args_json || !result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int timer_id = extract_json_int(args_json, "timer_id", -1);
    
    if (timer_id < 0) {
        *error = strdup("Invalid timer_id");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Find and cancel timer
    bool found = false;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].is_active && timers[i].id == timer_id) {
            timers[i].is_active = false;
            found = true;
            TIMER_LOG("Canceled timer %d: %s", timer_id, timers[i].label);
            break;
        }
    }
    
    if (!found) {
        *error = strdup("Timer not found or already completed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Return result
    char* result_json = malloc(128);
    if (!result_json) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    snprintf(result_json, 128, "{\"success\": true, \"message\": \"Timer canceled\"}");
    *result = result_json;
    
    return ETHERVOX_SUCCESS;
}

/**
 * Alarm Create Tool
 * JSON input: {"hour": 14, "minute": 30, "label": "Meeting reminder"}
 * Creates alarm for specific time today (or tomorrow if time has passed)
 */
static int alarm_create_execute(const char* args_json, char** result, char** error) {
    if (!args_json || !result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract parameters
    int hour = extract_json_int(args_json, "hour", -1);
    int minute = extract_json_int(args_json, "minute", -1);
    char* label = extract_json_string(args_json, "label");
    
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        *error = strdup("Invalid time - hour must be 0-23, minute must be 0-59");
        free(label);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].is_active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        *error = strdup("Maximum number of timers reached (10)");
        free(label);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Calculate trigger time (today or tomorrow)
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    struct tm tm_alarm = *tm_now;
    tm_alarm.tm_hour = hour;
    tm_alarm.tm_min = minute;
    tm_alarm.tm_sec = 0;
    
    time_t alarm_time = mktime(&tm_alarm);
    
    // If time has passed today, set for tomorrow
    if (alarm_time <= now) {
        tm_alarm.tm_mday += 1;
        alarm_time = mktime(&tm_alarm);
    }
    
    // Create alarm
    timers[slot].id = next_timer_id++;
    timers[slot].trigger_time = alarm_time;
    timers[slot].is_active = true;
    timers[slot].is_alarm = true;
    
    if (label) {
        strncpy(timers[slot].label, label, sizeof(timers[slot].label) - 1);
        timers[slot].label[sizeof(timers[slot].label) - 1] = '\0';
        free(label);
    } else {
        snprintf(timers[slot].label, sizeof(timers[slot].label), 
                 "Alarm %02d:%02d", hour, minute);
    }
    
    int hours_until = (int)difftime(alarm_time, now) / 3600;
    int mins_until = ((int)difftime(alarm_time, now) % 3600) / 60;
    
    TIMER_LOG("Created alarm %d: %s at %02d:%02d (in %dh %dm)", 
              timers[slot].id, timers[slot].label, hour, minute, hours_until, mins_until);
    
    // Return result
    char* result_json = malloc(256);
    if (!result_json) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    snprintf(result_json, 256, 
             "{\"success\": true, \"alarm_id\": %d, \"label\": \"%s\", \"time\": \"%02d:%02d\"}",
             timers[slot].id, timers[slot].label, hour, minute);
    *result = result_json;
    
    return ETHERVOX_SUCCESS;
}

/**
 * Timer List Tool
 * JSON input: {}
 */
static int timer_list_execute(const char* args_json, char** result, char** error) {
    if (!result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Build JSON array of active timers
    char* result_json = malloc(2048);
    if (!result_json) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    strcpy(result_json, "{\"timers\": [");
    bool first = true;
    time_t now = time(NULL);
    
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].is_active) {
            int remaining = (int)difftime(timers[i].trigger_time, now);
            if (remaining < 0) remaining = 0;
            
            char timer_json[256];
            snprintf(timer_json, sizeof(timer_json),
                    "%s{\"id\": %d, \"label\": \"%s\", \"remaining_seconds\": %d, \"type\": \"%s\"}",
                    first ? "" : ", ",
                    timers[i].id,
                    timers[i].label,
                    remaining,
                    timers[i].is_alarm ? "alarm" : "timer");
            
            strcat(result_json, timer_json);
            first = false;
        }
    }
    
    strcat(result_json, "]}");
    *result = result_json;
    
    return ETHERVOX_SUCCESS;
}

// Tool definitions
static ethervox_tool_t timer_create_tool = {
    .name = "timer_create",
    .description = "Create a timer with duration in seconds. Use for countdown timers (e.g., '5 minute timer', 'timer for 30 seconds')",
    .parameters_json_schema = 
        "{\"type\":\"object\","
        "\"properties\":{"
        "\"duration_seconds\":{\"type\":\"integer\",\"description\":\"Duration in seconds\"},"
        "\"label\":{\"type\":\"string\",\"description\":\"Optional label/name for the timer\"}"
        "},"
        "\"required\":[\"duration_seconds\"]}",
    .execute = timer_create_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = true,
    .estimated_latency_ms = 1.0f
};

static ethervox_tool_t timer_cancel_tool = {
    .name = "timer_cancel",
    .description = "Cancel an active timer by its ID",
    .parameters_json_schema = 
        "{\"type\":\"object\","
        "\"properties\":{"
        "\"timer_id\":{\"type\":\"integer\",\"description\":\"ID of timer to cancel\"}"
        "},"
        "\"required\":[\"timer_id\"]}",
    .execute = timer_cancel_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = true,
    .estimated_latency_ms = 1.0f
};

static ethervox_tool_t timer_list_tool = {
    .name = "timer_list",
    .description = "List all active timers and their remaining time",
    .parameters_json_schema = 
        "{\"type\":\"object\","
        "\"properties\":{}}",
    .execute = timer_list_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = true,
    .estimated_latency_ms = 1.0f
};

static ethervox_tool_t alarm_create_tool = {
    .name = "alarm_create",
    .description = "Create an alarm for a specific time (24-hour format). Use for wake-up alarms or reminders at specific times (e.g., 'wake me at 7am', 'alarm for 2:30pm')",
    .parameters_json_schema = 
        "{\"type\":\"object\","
        "\"properties\":{"
        "\"hour\":{\"type\":\"integer\",\"description\":\"Hour in 24-hour format (0-23)\"},"
        "\"minute\":{\"type\":\"integer\",\"description\":\"Minute (0-59)\"},"
        "\"label\":{\"type\":\"string\",\"description\":\"Optional label/name for the alarm\"}"
        "},"
        "\"required\":[\"hour\",\"minute\"]}",
    .execute = alarm_create_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = true,
    .estimated_latency_ms = 1.0f
};

// Public API
const ethervox_tool_t* ethervox_tool_timer_create(void) {
    return &timer_create_tool;
}

const ethervox_tool_t* ethervox_tool_timer_cancel(void) {
    return &timer_cancel_tool;
}

const ethervox_tool_t* ethervox_tool_timer_list(void) {
    return &timer_list_tool;
}

const ethervox_tool_t* ethervox_tool_alarm_create(void) {
    return &alarm_create_tool;
}

/**
 * Check for triggered timers (called periodically by system)
 * Returns array of triggered timer IDs, caller must free
 */
int* ethervox_timer_check_triggered(int* count) {
    if (!count) return NULL;
    
    time_t now = time(NULL);
    int triggered[MAX_TIMERS];
    int triggered_count = 0;
    
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].is_active && difftime(now, timers[i].trigger_time) >= 0) {
            triggered[triggered_count++] = timers[i].id;
            timers[i].is_active = false;  // Deactivate
            TIMER_LOG("Timer %d triggered: %s", timers[i].id, timers[i].label);
        }
    }
    
    if (triggered_count == 0) {
        *count = 0;
        return NULL;
    }
    
    int* result = malloc(triggered_count * sizeof(int));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    memcpy(result, triggered, triggered_count * sizeof(int));
    *count = triggered_count;
    return result;
}

/**
 * Get timer details by ID
 */
ethervox_result_t ethervox_timer_get_info(int timer_id, char* label_out, size_t label_size, int* remaining_seconds) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].is_active && timers[i].id == timer_id) {
            if (label_out && label_size > 0) {
                strncpy(label_out, timers[i].label, label_size - 1);
                label_out[label_size - 1] = '\0';
            }
            if (remaining_seconds) {
                time_t now = time(NULL);
                *remaining_seconds = (int)difftime(timers[i].trigger_time, now);
            }
            return ETHERVOX_SUCCESS;
        }
    }
    return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Not found
}

/**
 * Get first active timer status (for UI display)
 * Returns JSON string with timer info or empty if no active timer
 */
char* ethervox_timer_get_active_status(void) {
    time_t now = time(NULL);
    
    // Find first active timer (prioritize regular timers over alarms)
    int timer_idx = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].is_active && !timers[i].is_alarm) {
            timer_idx = i;
            break;
        }
    }
    
    // If no timer, look for alarm
    if (timer_idx == -1) {
        for (int i = 0; i < MAX_TIMERS; i++) {
            if (timers[i].is_active && timers[i].is_alarm) {
                timer_idx = i;
                break;
            }
        }
    }
    
    char* result = malloc(512);
    if (!result) return NULL;
    
    if (timer_idx == -1) {
        strcpy(result, "{\"has_timer\": false}");
        return result;
    }
    
    int remaining = (int)difftime(timers[timer_idx].trigger_time, now);
    if (remaining < 0) remaining = 0;
    
    snprintf(result, 512,
            "{\"has_timer\": true, \"id\": %d, \"label\": \"%s\", \"remaining_seconds\": %d, \"is_alarm\": %s}",
            timers[timer_idx].id,
            timers[timer_idx].label,
            remaining,
            timers[timer_idx].is_alarm ? "true" : "false");
    
    return result;
}

