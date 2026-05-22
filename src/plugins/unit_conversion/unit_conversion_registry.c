// SPDX-License-Identifier: MIT
#include "ethervox/error.h"
// Copyright (c) 2025 Tim Kos

/**
 * @file unit_conversion_registry.c
 * @brief Tool registry integration for unit conversion
 */

#include "ethervox/unit_conversion.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Helper: Parse JSON number field
static int parse_json_number(const char* json, const char* key, double* out) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) {
        // Try with spaces
        snprintf(search_key, sizeof(search_key), "\"%s\" :", key);
        pos = strstr(json, search_key);
        if (!pos) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    pos += strlen(search_key);
    while (*pos && isspace(*pos)) pos++;
    
    char* end;
    *out = strtod(pos, &end);
    
    if (pos == end) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    return ETHERVOX_SUCCESS;
}

// Helper: Parse JSON string field
static int parse_json_string(const char* json, const char* key, char* out, size_t out_size) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    
    const char* pos = strstr(json, search_key);
    if (!pos) {
        // Try with spaces
        snprintf(search_key, sizeof(search_key), "\"%s\" : \"", key);
        pos = strstr(json, search_key);
        if (!pos) {
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
    }
    
    pos += strlen(search_key);
    
    const char* end = pos;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end + 1)) {
            end += 2;
        } else {
            end++;
        }
    }
    
    size_t len = end - pos;
    if (len >= out_size) {
        len = out_size - 1;
    }
    
    strncpy(out, pos, len);
    out[len] = '\0';
    
    return ETHERVOX_SUCCESS;
}

/**
 * @brief Tool wrapper for unit_convert
 */
static int tool_unit_convert_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    double value = 0.0;
    char from_unit[128] = {0};
    char to_unit[128] = {0};
    
    // Parse parameters
    if (parse_json_number(args_json, "value", &value) != 0) {
        *error = strdup("Missing or invalid 'value' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (parse_json_string(args_json, "from", from_unit, sizeof(from_unit)) != 0 &&
        parse_json_string(args_json, "from_unit", from_unit, sizeof(from_unit)) != 0) {
        *error = strdup("Missing 'from' or 'from_unit' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (parse_json_string(args_json, "to", to_unit, sizeof(to_unit)) != 0 &&
        parse_json_string(args_json, "to_unit", to_unit, sizeof(to_unit)) != 0) {
        *error = strdup("Missing 'to' or 'to_unit' parameter");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Perform conversion
    double converted = 0.0;
    char* err_msg = NULL;
    int ret = ethervox_unit_convert(value, from_unit, to_unit, &converted, &err_msg);
    
    if (ret != 0) {
        *error = err_msg ? err_msg : strdup("Conversion failed");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Format result
    char result_buf[512];
    snprintf(result_buf, sizeof(result_buf),
            "{\"success\":true,\"value\":%.10g,\"from\":\"%s\",\"to\":\"%s\",\"result\":%.10g}",
            value, from_unit, to_unit, converted);
    
    *result = strdup(result_buf);
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Converted %.6f %s to %.6f %s", value, from_unit, converted, to_unit);
    
    return ETHERVOX_SUCCESS;
}

/**
 * @brief Register unit conversion tool with the tool registry
 */
ethervox_result_t ethervox_unit_conversion_register(void* registry_ptr) {
    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)registry_ptr;
    
    if (!registry) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Cannot register unit conversion: NULL registry");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_tool_t tool = {
        .name = "unit_convert",
        .description = 
            "Convert between scientific and engineering units. "
            "Supports: temperature (celsius/fahrenheit/kelvin/rankine), "
            "length (meter/km/mile/foot/inch/yard/etc), "
            "mass (kg/gram/pound/ounce/ton/stone/etc), "
            "volume (liter/gallon/quart/pint/cup/ml/etc), "
            "speed (m/s/km/h/mph/knot/mach/etc), "
            "pressure (pascal/bar/psi/atm/torr/mmHg/etc), "
            "energy (joule/calorie/kWh/BTU/eV/etc), "
            "power (watt/hp/BTU/h/etc), "
            "area (m²/km²/acre/hectare/sq ft/etc), "
            "data (byte/KB/MB/GB/bit/etc). "
            "Example: Convert 100 celsius to fahrenheit, or 5 miles to kilometers. "
            "Always provide value, from_unit, and to_unit.",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"value\":{\"type\":\"number\",\"description\":\"The numeric value to convert\"},"
            "\"from\":{\"type\":\"string\",\"description\":\"Source unit (e.g., 'celsius', 'mile', 'kg')\"},"
            "\"from_unit\":{\"type\":\"string\",\"description\":\"Source unit (alternative to 'from')\"},"
            "\"to\":{\"type\":\"string\",\"description\":\"Target unit (e.g., 'fahrenheit', 'km', 'pound')\"},"
            "\"to_unit\":{\"type\":\"string\",\"description\":\"Target unit (alternative to 'to')\"}"
            "},\"required\":[\"value\"]}",
        .execute = tool_unit_convert_wrapper,
        .is_deterministic = true,         // Same inputs always produce same outputs
        .requires_confirmation = false,    // Silent conversion
        .is_stateful = false,             // No state modification
        .estimated_latency_ms = 1.0f      // Very fast
    };
    
    ethervox_result_t ret = ethervox_tool_registry_add(registry, &tool);
    
    if (ethervox_is_success(ret)) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Registered unit_convert tool");
    } else {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to register unit_convert tool");
    }
    
    return ret;
}
