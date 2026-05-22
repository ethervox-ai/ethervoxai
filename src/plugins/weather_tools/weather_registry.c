/**
 * @file weather_registry.c
 * @brief LLM tool wrapper and registration for weather forecast functionality
 * 
 * Provides the ethervox_weather_tools_register() function to register the weather
 * forecast tool with the Governor system.
 */

#include "ethervox/weather_tools.h"
#include "ethervox/governor.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Initialize weather subsystem on first use
static int weather_initialized = 0;

// Helper to parse JSON string parameter
static char* parse_json_string(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    // Simple JSON string extraction: look for "key":"value"
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char* start = strstr(json, search);
    if (!start) return NULL;
    
    start = strchr(start + strlen(search), '"');
    if (!start) return NULL;
    start++; // Skip opening quote
    
    const char* end = strchr(start, '"');
    if (!end) return NULL;
    
    size_t len = end - start;
    char* value = malloc(len + 1);
    if (!value) return NULL;
    
    strncpy(value, start, len);
    value[len] = '\0';
    return value;
}

// Helper to parse JSON integer parameter
static int parse_json_int(const char* json, const char* key, int default_value) {
    if (!json || !key) return default_value;
    
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char* start = strstr(json, search);
    if (!start) return default_value;
    
    start += strlen(search);
    // Skip whitespace
    while (*start && isspace(*start)) start++;
    
    return atoi(start);
}

// Helper to check if a string looks like coordinates (e.g., "37.7749,-122.4194")
static int is_coordinate_format(const char* location) {
    if (!location) return 0;
    
    // Look for pattern: number, comma, number
    const char* comma = strchr(location, ',');
    if (!comma) return 0;
    
    // Check if before comma looks like a number
    const char* p = location;
    if (*p == '-' || *p == '+') p++;
    while (p < comma && (isdigit(*p) || *p == '.')) p++;
    if (p != comma) return 0; // Non-numeric characters before comma
    
    // Check if after comma looks like a number
    p = comma + 1;
    if (*p == '-' || *p == '+') p++;
    int has_digits = 0;
    while (*p && (isdigit(*p) || *p == '.')) {
        if (isdigit(*p)) has_digits = 1;
        p++;
    }
    
    return has_digits;
}

// Helper to parse coordinates from string "lat,lon"
static int parse_coordinates(const char* location, double* lat_out, double* lon_out) {
    if (!location || !lat_out || !lon_out) return 0;
    
    const char* comma = strchr(location, ',');
    if (!comma) return 0;
    
    *lat_out = atof(location);
    *lon_out = atof(comma + 1);
    
    return 1;
}

// Helper to parse forecast type string
static ethervox_weather_forecast_type_t parse_forecast_type(const char* type_str) {
    if (!type_str) return ETHERVOX_WEATHER_CURRENT;
    
    if (strcasecmp(type_str, "current") == 0) {
        return ETHERVOX_WEATHER_CURRENT;
    } else if (strcasecmp(type_str, "hourly") == 0) {
        return ETHERVOX_WEATHER_HOURLY;
    } else if (strcasecmp(type_str, "daily") == 0) {
        return ETHERVOX_WEATHER_DAILY;
    } else if (strcasecmp(type_str, "7-day") == 0 || strcasecmp(type_str, "7day") == 0 || strcasecmp(type_str, "week") == 0) {
        return ETHERVOX_WEATHER_7DAY;
    }
    
    return ETHERVOX_WEATHER_CURRENT; // Default
}

// Format weather response as JSON string for LLM
static char* format_weather_response(const ethervox_weather_response_t* response) {
    if (!response) return NULL;
    
    // Allocate buffer for JSON response (make it large enough for all forecasts)
    size_t buffer_size = 4096 + (response->forecast_count * 512);
    char* json = malloc(buffer_size);
    if (!json) return NULL;
    
    int offset = snprintf(json, buffer_size,
        "{\"location\":{\"latitude\":%.4f,\"longitude\":%.4f,\"timezone\":\"%s\"},",
        response->latitude, response->longitude,
        response->timezone ? response->timezone : "UTC");
    
    offset += snprintf(json + offset, buffer_size - offset,
        "\"from_cache\":%s,\"generated_at\":%ld,\"forecasts\":[",
        response->from_cache ? "true" : "false",
        (long)response->generated_at);
    
    for (size_t i = 0; i < response->forecast_count; i++) {
        const ethervox_weather_forecast_t* fc = &response->forecasts[i];
        
        if (i > 0) {
            offset += snprintf(json + offset, buffer_size - offset, ",");
        }
        
        offset += snprintf(json + offset, buffer_size - offset,
            "{\"timestamp\":%ld,\"temperature\":%.1f,\"apparent_temperature\":%.1f,"
            "\"humidity\":%d,\"wind_speed\":%.1f,\"wind_direction\":%d,"
            "\"precipitation\":%.1f,\"weather_code\":%d,\"condition\":\"%s\"}",
            (long)fc->timestamp, fc->temperature, fc->apparent_temperature,
            fc->humidity, fc->wind_speed, fc->wind_direction,
            fc->precipitation, fc->weather_code, fc->condition);
    }
    
    offset += snprintf(json + offset, buffer_size - offset, "]}");
    
    return json;
}

/**
 * Tool wrapper function that gets called by the Governor when the LLM invokes the weather tool
 */
static int tool_weather_forecast_wrapper(const char* parameters_json, char** result_out, char** error_out) {
    ethervox_result_t result_code;
    char* error_msg = NULL;
    
    // Parse parameters
    char* location_str = parse_json_string(parameters_json, "location");
    if (!location_str) {
        *error_out = strdup("Missing required parameter: location");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* forecast_type_str = parse_json_string(parameters_json, "forecast_type");
    int days_ahead = parse_json_int(parameters_json, "days_ahead", 1);
    
    ethervox_weather_forecast_type_t forecast_type = parse_forecast_type(forecast_type_str);
    free(forecast_type_str);
    
    // Build request
    ethervox_weather_request_t request = {0};
    request.forecast_type = forecast_type;
    request.days_ahead = days_ahead;
    request.use_cache = 1; // Always use cache
    
    // Check if location is coordinates or city name
    if (is_coordinate_format(location_str)) {
        if (!parse_coordinates(location_str, &request.latitude, &request.longitude)) {
            *error_out = strdup("Invalid coordinate format. Use: latitude,longitude");
            free(location_str);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        request.location_name[0] = '\0'; // Empty string
    } else {
        // City name - need to geocode first
        strncpy(request.location_name, location_str, sizeof(request.location_name) - 1);
        request.location_name[sizeof(request.location_name) - 1] = '\0';
        
        result_code = ethervox_weather_geocode(location_str, &request.latitude, &request.longitude, &error_msg);
        if (result_code != ETHERVOX_SUCCESS) {
            *error_out = error_msg ? error_msg : strdup("Geocoding failed");
            free(location_str);
            return result_code;
        }
    }
    
    // Get forecast
    ethervox_weather_response_t* response = NULL;
    result_code = ethervox_weather_get_forecast(&request, &response, &error_msg);
    
    free(location_str);
    
    if (result_code != ETHERVOX_SUCCESS) {
        *error_out = error_msg ? error_msg : strdup("Weather forecast failed");
        return result_code;
    }
    
    // Format response as JSON
    char* json_response = format_weather_response(response);
    ethervox_weather_free_response(response);
    
    if (!json_response) {
        *error_out = strdup("Failed to format weather response");
        return ETHERVOX_ERROR_FAILED;
    }
    
    *result_out = json_response;
    return ETHERVOX_SUCCESS;
}

/**
 * Register weather forecast tool with the Governor
 */
ethervox_result_t ethervox_weather_tools_register(void* governor_registry) {
    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)governor_registry;
    
    if (!registry) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Cannot register weather tools: NULL registry");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Initialize weather subsystem on first registration
    if (!weather_initialized) {
        ethervox_weather_config_t config = {
            .enable_cache = true,
            .cache_ttl_sec = 3600,          // 1 hour cache
            .max_cache_entries = 100,       // Max 100 cached forecasts
            .request_timeout_sec = 10,      // 10 second HTTP timeout
            .max_retries = 3,               // Retry failed requests 3 times
            .user_agent = "EthervoxAI/1.0"
        };
        ethervox_result_t result = ethervox_weather_init(&config);
        if (result != ETHERVOX_SUCCESS) {
            ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                        "Failed to initialize weather subsystem");
            return result;
        }
        weather_initialized = 1;
    }
    
    // Define tool
    ethervox_tool_t tool = {
        .name = "get_weather_forecast",
        .description = 
            "Get weather forecast for a location with temperature, humidity, wind, and precipitation",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"location\":{\"type\":\"string\",\"description\":\"Location as city name (e.g., 'San Francisco, CA') or coordinates (e.g., '37.7749,-122.4194')\"},"
            "\"forecast_type\":{\"type\":\"string\",\"enum\":[\"current\",\"hourly\",\"daily\",\"7-day\"],\"description\":\"Type of forecast: 'current' for current conditions, 'hourly' for 24-hour forecast, 'daily' or '7-day' for multi-day forecast\",\"default\":\"current\"},"
            "\"days_ahead\":{\"type\":\"integer\",\"description\":\"For daily forecast: number of days ahead (1-7)\",\"minimum\":1,\"maximum\":7,\"default\":1}"
            "},\"required\":[\"location\"]}",
        .execute = tool_weather_forecast_wrapper,
        .is_deterministic = false,         // Weather changes over time
        .requires_confirmation = false,    // No user confirmation needed
        .is_stateful = false,              // No state modification
        .estimated_latency_ms = 500.0f     // Network request latency
    };
    
    // Register with registry
    ethervox_result_t result = ethervox_tool_registry_add(registry, &tool);
    
    if (ethervox_is_success(result)) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Weather forecast tool registered successfully");
    } else {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to register weather tool");
    }
    
    return result;
}
