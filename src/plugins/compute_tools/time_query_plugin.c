/**
#include "ethervox/error.h"
 * @file time_query_plugin.c
 * @brief Time and date query tools
 *
 * Provides tools to get current time, date, day of week, and week number
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/compute_tools.h"
#include "ethervox/tool_catalogue.h"
#include "compute_tools_catalogue.h"
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
    .test_scenario = "What time is it?",
        .execute = time_get_current_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};
static bool time_get_current_tool_loaded = false;

const ethervox_tool_t* ethervox_tool_time_get_current(void) {
    if (!time_get_current_tool_loaded) {
        ethervox_result_t r = ethervox_tool_catalogue_load(
            ETHERVOX_CATALOGUE_COMPUTE_TOOLS_JSON, "get_time",
            ethervox_tool_catalogue_build_profile(), &time_get_current_tool);
        time_get_current_tool_loaded = ethervox_is_success(r);
    }
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
    .test_scenario = "What's today's date?",
        .execute = time_get_date_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};
static bool time_get_date_tool_loaded = false;

const ethervox_tool_t* ethervox_tool_time_get_date(void) {
    if (!time_get_date_tool_loaded) {
        ethervox_result_t r = ethervox_tool_catalogue_load(
            ETHERVOX_CATALOGUE_COMPUTE_TOOLS_JSON, "get_date",
            ethervox_tool_catalogue_build_profile(), &time_get_date_tool);
        time_get_date_tool_loaded = ethervox_is_success(r);
    }
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
    .test_scenario = "What day is it?",
        .execute = time_get_day_of_week_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};
static bool time_get_day_of_week_tool_loaded = false;

const ethervox_tool_t* ethervox_tool_time_get_day_of_week(void) {
    if (!time_get_day_of_week_tool_loaded) {
        ethervox_result_t r = ethervox_tool_catalogue_load(
            ETHERVOX_CATALOGUE_COMPUTE_TOOLS_JSON, "get_day",
            ethervox_tool_catalogue_build_profile(), &time_get_day_of_week_tool);
        time_get_day_of_week_tool_loaded = ethervox_is_success(r);
    }
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
    .test_scenario = "What week of the year is it?",
        .execute = time_get_week_number_execute,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 1.0f
};
static bool time_get_week_number_tool_loaded = false;

const ethervox_tool_t* ethervox_tool_time_get_week_number(void) {
    if (!time_get_week_number_tool_loaded) {
        ethervox_result_t r = ethervox_tool_catalogue_load(
            ETHERVOX_CATALOGUE_COMPUTE_TOOLS_JSON, "time_get_week_number",
            ethervox_tool_catalogue_build_profile(), &time_get_week_number_tool);
        time_get_week_number_tool_loaded = ethervox_is_success(r);
    }
    return &time_get_week_number_tool;
}
