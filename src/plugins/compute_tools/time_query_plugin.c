/**
#include "ethervox/error.h"
 * @file time_query_plugin.c
 * @brief Time and date query tools
 *
 * Provides tools to get current time, date, day of week, and week number
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/compute_tools.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// Current Time Tool
// ============================================================================

static int time_get_current_execute(const char* args_json, char** result, char** error) {
    (void)args_json;  // No parameters needed
    
    if (!result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    
    if (!local) {
        *error = strdup("Failed to get local time");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char result_buffer[256];
    char time_12hr[64];
    char time_24hr[64];
    
    // Format time as HH:MM AM/PM
    int hour = local->tm_hour;
    int minute = local->tm_min;
    const char* period = "AM";
    
    if (hour >= 12) {
        period = "PM";
        if (hour > 12) hour -= 12;
    }
    if (hour == 0) hour = 12;
    
    snprintf(time_12hr, sizeof(time_12hr), "%d:%02d %s", hour, minute, period);
    snprintf(time_24hr, sizeof(time_24hr), "%02d:%02d", local->tm_hour, local->tm_min);
    
    snprintf(result_buffer, sizeof(result_buffer),
        "{\"time_12hr\": \"%s\", \"time_24hr\": \"%s\"}",
        time_12hr, time_24hr);
    
    *result = strdup(result_buffer);
    if (!*result) {
        *error = strdup("Memory allocation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    return ETHERVOX_SUCCESS;
}

static ethervox_tool_t time_get_current_tool = {
    .name = "get_time",
    .description = "Get current time. Use when user asks 'what time is it' or needs to know the current time.",
    .parameters_json_schema = "{}",
    .execute = time_get_current_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};

const ethervox_tool_t* ethervox_tool_time_get_current(void) {
    return &time_get_current_tool;
}

// ============================================================================
// Current Date Tool
// ============================================================================

static int time_get_date_execute(const char* args_json, char** result, char** error) {
    (void)args_json;
    
    if (!result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    
    if (!local) {
        *error = strdup("Failed to get local time");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char* months[] = {"January", "February", "March", "April", "May", "June",
                           "July", "August", "September", "October", "November", "December"};
    
    char result_buffer[256];
    snprintf(result_buffer, sizeof(result_buffer),
        "{\"date\": \"%s, %s %d, %d\", \"day_name\": \"%s\", \"month\": \"%s\", \"day\": %d, \"year\": %d}",
        days[local->tm_wday], months[local->tm_mon], local->tm_mday, 1900 + local->tm_year,
        days[local->tm_wday], months[local->tm_mon], local->tm_mday, 1900 + local->tm_year);
    
    *result = strdup(result_buffer);
    if (!*result) {
        *error = strdup("Memory allocation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    return ETHERVOX_SUCCESS;
}

static ethervox_tool_t time_get_date_tool = {
    .name = "get_date",
    .description = "Get current date (day/month/year). Use to get today's date. Use when user asks 'what's the date', needs today's date for calculations, or asks about days until/since an event.",
    .parameters_json_schema = "{}",
    .execute = time_get_date_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};

const ethervox_tool_t* ethervox_tool_time_get_date(void) {
    return &time_get_date_tool;
}

// ============================================================================
// Day of Week Tool
// ============================================================================

static int time_get_day_of_week_execute(const char* args_json, char** result, char** error) {
    (void)args_json;
    
    if (!result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    
    if (!local) {
        *error = strdup("Failed to get local time");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char* days_short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    char result_buffer[256];
    snprintf(result_buffer, sizeof(result_buffer),
        "{\"day\": \"%s\", \"day_short\": \"%s\", \"day_number\": %d}",
        days[local->tm_wday],
        days_short[local->tm_wday],
        local->tm_wday);
    
    *result = strdup(result_buffer);
    if (!*result) {
        *error = strdup("Memory allocation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    return ETHERVOX_SUCCESS;
}

static ethervox_tool_t time_get_day_of_week_tool = {
    .name = "get_day",
    .description = "Get the current day of the week. Use when user asks 'what day is it' or 'what day of the week'.",
    .parameters_json_schema = "{}",
    .execute = time_get_day_of_week_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};

const ethervox_tool_t* ethervox_tool_time_get_day_of_week(void) {
    return &time_get_day_of_week_tool;
}

// ============================================================================
// Week Number Tool
// ============================================================================

static int time_get_week_number_execute(const char* args_json, char** result, char** error) {
    (void)args_json;
    
    if (!result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    
    if (!local) {
        *error = strdup("Failed to get local time");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char week_str[8];
    strftime(week_str, sizeof(week_str), "%V", local);  // ISO 8601 week number (Monday as first day)
    int week_num = atoi(week_str);
    
    char result_buffer[256];
    snprintf(result_buffer, sizeof(result_buffer),
        "{\"week_number\": %d, \"year\": %d, \"day_of_year\": %d}",
        week_num,
        1900 + local->tm_year,
        local->tm_yday + 1);
    
    *result = strdup(result_buffer);
    if (!*result) {
        *error = strdup("Memory allocation failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    return ETHERVOX_SUCCESS;
}

static ethervox_tool_t time_get_week_number_tool = {
    .name = "time_get_week_number",
    .description = "Get the current week number of the year. Use when user asks 'what week is it' or 'what week number'.",
    .parameters_json_schema = "{}",
    .execute = time_get_week_number_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};

const ethervox_tool_t* ethervox_tool_time_get_week_number(void) {
    return &time_get_week_number_tool;
}
