/**
 * @file weather_tools.h
 * @brief Weather forecast tool using Open-Meteo API
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * 
 * Weather data provided by Open-Meteo (MIT License)
 * API: https://open-meteo.com
 */

#ifndef ETHERVOX_WEATHER_TOOLS_H
#define ETHERVOX_WEATHER_TOOLS_H

#include "ethervox/error.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define ETHERVOX_WEATHER_MAX_FORECASTS 168  // 7 days * 24 hours
#define ETHERVOX_WEATHER_CACHE_TTL_SEC 3600 // 1 hour
#define ETHERVOX_WEATHER_API_TIMEOUT_SEC 10
#define ETHERVOX_WEATHER_MAX_RETRIES 3

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Weather forecast type
 */
typedef enum {
    ETHERVOX_WEATHER_CURRENT,   // Current conditions
    ETHERVOX_WEATHER_HOURLY,    // Next 24 hours
    ETHERVOX_WEATHER_DAILY,     // Today's forecast
    ETHERVOX_WEATHER_7DAY       // 7-day forecast
} ethervox_weather_forecast_type_t;

/**
 * @brief Weather request parameters
 */
typedef struct {
    double latitude;              // -90 to 90
    double longitude;             // -180 to 180
    char location_name[256];      // Human-readable name (optional)
    ethervox_weather_forecast_type_t forecast_type;
    int days_ahead;               // 0-7 (0 = today)
    bool use_cache;               // Enable caching
} ethervox_weather_request_t;

/**
 * @brief Single forecast data point
 */
typedef struct {
    time_t timestamp;             // Unix timestamp
    float temperature;            // Celsius
    float apparent_temperature;   // "Feels like" temp
    int humidity;                 // Percentage (0-100)
    float wind_speed;             // km/h
    int wind_direction;           // Degrees (0-360)
    float precipitation;          // mm
    int precipitation_probability; // Percentage (0-100)
    int weather_code;             // WMO weather code (0-99)
    char condition[128];          // Human-readable: "Sunny", "Rainy", etc.
    float cloud_cover;            // Percentage (0-100)
    float visibility;             // meters
} ethervox_weather_forecast_t;

/**
 * @brief Weather response with metadata
 */
typedef struct {
    double latitude;              // Actual coordinates used
    double longitude;
    char location_name[256];      // Location name
    char timezone[64];            // e.g., "America/New_York"
    time_t generated_at;          // Response generation time
    bool from_cache;              // True if served from cache
    size_t forecast_count;        // Number of forecasts returned
    ethervox_weather_forecast_t* forecasts; // Array of forecasts
} ethervox_weather_response_t;

/**
 * @brief Weather tools configuration
 */
typedef struct {
    bool enable_cache;            // Enable response caching
    int cache_ttl_sec;            // Cache TTL in seconds
    int max_cache_entries;        // Max cached responses
    int request_timeout_sec;      // HTTP timeout
    int max_retries;              // Max retry attempts
    char user_agent[256];         // Custom user agent string
} ethervox_weather_config_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize weather tools module
 * 
 * @param config Configuration (NULL for defaults)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_weather_init(const ethervox_weather_config_t* config);

/**
 * @brief Get weather forecast for a location
 * 
 * @param request Request parameters
 * @param response_out Pointer to receive response (caller must free)
 * @param error_message_out Error message on failure (caller must free)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_weather_get_forecast(
    const ethervox_weather_request_t* request,
    ethervox_weather_response_t** response_out,
    char** error_message_out
);

/**
 * @brief Geocode a location name to coordinates
 * 
 * @param location_name City name (e.g., "New York", "London, UK")
 * @param latitude_out Pointer to receive latitude
 * @param longitude_out Pointer to receive longitude
 * @param error_message_out Error message on failure (caller must free)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_weather_geocode(
    const char* location_name,
    double* latitude_out,
    double* longitude_out,
    char** error_message_out
);

/**
 * @brief Free weather response
 * 
 * @param response Response to free
 */
void ethervox_weather_free_response(ethervox_weather_response_t* response);

/**
 * @brief Clear weather cache
 * 
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_weather_clear_cache(void);

/**
 * @brief Get cache statistics
 * 
 * @param hit_count_out Cache hit count
 * @param miss_count_out Cache miss count
 * @param size_out Current cache size
 */
void ethervox_weather_get_cache_stats(
    uint64_t* hit_count_out,
    uint64_t* miss_count_out,
    size_t* size_out
);

/**
 * @brief Cleanup weather tools module
 */
void ethervox_weather_cleanup(void);

/**
 * @brief Register weather tools with tool registry
 * 
 * @param registry_ptr Tool registry pointer
 * @return 0 on success, negative on error
 */
int ethervox_weather_tools_register(void* registry_ptr);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_WEATHER_TOOLS_H
