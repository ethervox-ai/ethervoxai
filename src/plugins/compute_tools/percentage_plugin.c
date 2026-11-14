/**
#include "ethervox/error.h"
 * @file percentage_plugin.c
 * @brief Percentage calculator compute tool
 *
 * Handles percentage calculations for tips, tax, discounts, etc.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/compute_tools.h"
#include "ethervox/tool_catalogue.h"
#include "compute_tools_catalogue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double extract_json_number(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return 0.0;
    
    const char* colon = strchr(key_pos, ':');
    if (!colon) return 0.0;
    
    return atof(colon + 1);
}

static int extract_json_int(const char* json, const char* key, int default_value) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char* key_pos = strstr(json, search);
    if (!key_pos) return default_value;
    
    const char* colon = strchr(key_pos, ':');
    if (!colon) return default_value;
    
    return atoi(colon + 1);
}

static char* extract_json_string_value(const char* json, const char* key) {
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

static int percentage_execute(const char* args_json, char** result, char** error) {
    if (!args_json || !result || !error) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    double value = extract_json_number(args_json, "value");
    double percentage = extract_json_number(args_json, "percentage");
    char* operation = extract_json_string_value(args_json, "operation");
    int decimal_places = extract_json_int(args_json, "decimal_places", 2);
    
    if (!operation) {
        *error = strdup("Missing 'operation' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (decimal_places < 0) decimal_places = 0;
    if (decimal_places > 15) decimal_places = 15;  // Limit precision
    
    double calc_result = 0.0;
    
    if (strcmp(operation, "of") == 0) {
        // Calculate percentage of value
        calc_result = value * (percentage / 100.0);
    } else if (strcmp(operation, "increase") == 0) {
        // Increase value by percentage
        calc_result = value * (1.0 + percentage / 100.0);
    } else if (strcmp(operation, "decrease") == 0) {
        // Decrease value by percentage
        calc_result = value * (1.0 - percentage / 100.0);
    } else if (strcmp(operation, "is_what_percent") == 0) {
        // value is what percent of percentage?
        if (percentage != 0.0) {
            calc_result = (value / percentage) * 100.0;
        } else {
            free(operation);
            *error = strdup("Division by zero: percentage cannot be zero for is_what_percent operation");
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    } else {
        free(operation);
        char* err_msg = malloc(256);
        snprintf(err_msg, 256, "Unknown operation: %s. Valid operations: of, increase, decrease, is_what_percent", operation);
        *error = err_msg;
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Return result as JSON with tool name and operation
    char* result_json = malloc(256);
    if (!result_json) {
        free(operation);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    snprintf(result_json, 256, "{\"result\": %.*f, \"tool\": \"percentage_calculate\", \"operation\": \"%s\"}", 
             decimal_places, calc_result, operation);
    *result = result_json;
    
    free(operation);
    return ETHERVOX_SUCCESS;
}

static ethervox_tool_t percentage_tool = {
    .test_scenario = "What's 15% of 200?",
        .execute = percentage_execute,
    .is_deterministic = true,
    .requires_confirmation = false,
    .is_stateful = false,
    .estimated_latency_ms = 0.3f
};
static bool percentage_tool_loaded = false;

const ethervox_tool_t* ethervox_tool_percentage(void) {
    if (!percentage_tool_loaded) {
        ethervox_result_t r = ethervox_tool_catalogue_load(
            ETHERVOX_CATALOGUE_COMPUTE_TOOLS_JSON, "percentage_calculate",
            ethervox_tool_catalogue_build_profile(), &percentage_tool);
        percentage_tool_loaded = ethervox_is_success(r);
    }
    return &percentage_tool;
}
