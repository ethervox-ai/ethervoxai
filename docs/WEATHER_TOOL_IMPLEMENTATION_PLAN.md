# Weather Tool Implementation Plan (Open-Meteo)

This document provides a complete implementation plan for adding a weather forecast tool to EthervoxAI using Open-Meteo as the primary data source.

## Overview

**Data Source**: Open-Meteo (https://open-meteo.com)
- **License**: MIT (compatible with CC BY-NC-SA 4.0)
- **Cost**: Free, unlimited requests
- **API Key**: Not required
- **Coverage**: Global
- **Features**: Current weather, hourly, daily, 7-day forecasts

**Implementation Category**: External Tools (API calls)

**Estimated Implementation Time**: 2-3 days
- Day 1: Core implementation + HTTP client
- Day 2: Tool wrapper + registration + caching
- Day 3: Unit tests + integration tests + documentation

---

## Files to Create/Modify

### New Files (13 files)

```
src/plugins/weather_tools/
├── weather_core.c              # Core HTTP fetching + parsing
├── weather_cache.c             # Response caching layer
└── weather_registry.c          # Tool wrapper + registration

include/ethervox/
└── weather_tools.h             # Public API

tests/unit/
├── test_weather_parse.c        # JSON parsing tests
├── test_weather_cache.c        # Caching logic tests
└── test_weather_http.c         # HTTP client tests (mocked)

tests/integration/
└── test_weather_tool.c         # End-to-end integration test

docs/
├── WEATHER_TOOL_IMPLEMENTATION_PLAN.md  # This file
└── WEATHER_TOOL_USAGE.md       # User-facing documentation

examples/
└── weather_example.c           # Standalone usage example
```

### Files to Modify (5 files)

```
src/plugins/CMakeLists.txt              # Add weather_tools subdirectory
src/plugins/weather_tools/CMakeLists.txt # New build configuration
tests/CMakeLists.txt                    # Add weather test executables
src/dialogue/dialogue_core.c            # Register weather tool
THIRD_PARTY_LICENSES.md                 # Add Open-Meteo attribution
```

---

## Implementation Checklist

### Phase 1: Core Implementation (Day 1)

#### 1.1 HTTP Client Foundation
- [x] Verify `HAVE_LIBCURL` is available
- [ ] Create `weather_http_request()` wrapper
- [ ] Implement 10-second timeout
- [ ] Add retry logic (3 attempts with exponential backoff)
- [ ] Handle HTTP error codes (200, 429, 500, 503)
- [ ] Log all requests for debugging

#### 1.2 Data Structures
- [ ] Define `weather_request_t` (input parameters)
- [ ] Define `weather_forecast_t` (output data)
- [ ] Define `weather_config_t` (configuration)
- [ ] Define error codes (use existing -600 to -649 range)

#### 1.3 Core Functions
- [ ] `ethervox_weather_init()` - Initialize module
- [ ] `ethervox_weather_get_forecast()` - Main API
- [ ] `ethervox_weather_parse_json()` - Parse Open-Meteo response
- [ ] `ethervox_weather_format_condition()` - Convert WMO codes to text
- [ ] `ethervox_weather_cleanup()` - Free resources

### Phase 2: Tool Integration (Day 2)

#### 2.1 Tool Wrapper
- [ ] Create `tool_weather_forecast_wrapper()`
- [ ] Parse `location` parameter (support "City, State" or "lat,lon")
- [ ] Parse `forecast_type` parameter (current/hourly/daily/7-day)
- [ ] Parse `days_ahead` parameter (0-7)
- [ ] Format JSON result string
- [ ] Handle all error cases with descriptive messages

#### 2.2 Geocoding Support
- [ ] Implement `weather_geocode()` using Open-Meteo Geocoding API
- [ ] Cache geocoding results (city name → lat/lon)
- [ ] Support formats: "New York", "New York, NY", "40.7128,-74.0060"

#### 2.3 Caching Layer
- [ ] Design cache key structure: `hash(lat+lon+forecast_type+date)`
- [ ] Implement in-memory cache (max 100 entries)
- [ ] Set TTL: 1 hour for forecasts, 24 hours for geocoding
- [ ] Add cache statistics for monitoring
- [ ] Implement LRU eviction policy

#### 2.4 Tool Registration
- [ ] Define `ethervox_weather_tools_register()` function
- [ ] Create tool schema with JSON parameters
- [ ] Set tool metadata (is_deterministic=false, requires_confirmation=false)
- [ ] Set estimated_latency_ms=1500
- [ ] Register in dialogue_core.c for Android/CLI availability

### Phase 3: Testing (Day 3)

#### 3.1 Unit Tests
- [ ] `test_weather_parse.c` - JSON parsing
  - [ ] Valid Open-Meteo response
  - [ ] Malformed JSON handling
  - [ ] Missing fields handling
  - [ ] WMO code conversion (0-99)
- [ ] `test_weather_cache.c` - Caching logic
  - [ ] Cache hit/miss
  - [ ] TTL expiration
  - [ ] LRU eviction
  - [ ] Thread safety
- [ ] `test_weather_http.c` - HTTP layer (mocked)
  - [ ] Successful request
  - [ ] Network timeout
  - [ ] HTTP error codes
  - [ ] Retry logic

#### 3.2 Integration Tests
- [ ] Add `test_weather_tool()` to integration_tests.c
- [ ] Test with real Open-Meteo API call
- [ ] Verify tool wrapper parameter parsing
- [ ] Verify JSON result formatting
- [ ] Test error handling (invalid location, network failure)

#### 3.3 Platform Testing
- [ ] Test on macOS (development machine)
- [ ] Test on Linux (CI/CD)
- [ ] Test on Android (via JNI)
- [ ] Test on Windows (cross-compile)
- [ ] Mark ESP32 as unsupported (offline-only platform)

### Phase 4: Documentation

- [ ] Add usage examples to WEATHER_TOOL_USAGE.md
- [ ] Document API endpoints used
- [ ] Document caching behavior
- [ ] Update THIRD_PARTY_LICENSES.md with Open-Meteo attribution
- [ ] Add error handling guide
- [ ] Update this checklist with lessons learned

---

## Detailed Implementation

## Step 1: Header File (`include/ethervox/weather_tools.h`)

```c
/**
 * @file weather_tools.h
 * @brief Weather forecast tool using Open-Meteo API
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
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
```

---

## Step 2: Core Implementation (`src/plugins/weather_tools/weather_core.c`)

### 2.1 Open-Meteo API Endpoints

```c
// Open-Meteo API endpoints
#define OPENMETEO_FORECAST_URL "https://api.open-meteo.com/v1/forecast"
#define OPENMETEO_GEOCODING_URL "https://geocoding-api.open-meteo.com/v1/search"

// WMO Weather interpretation codes
// See: https://www.nodc.noaa.gov/archive/arc0021/0002199/1.1/data/0-data/HTML/WMO-CODE/WMO4677.HTM
static const char* wmo_code_to_condition(int code) {
    switch (code) {
        case 0: return "Clear sky";
        case 1: return "Mainly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: case 48: return "Foggy";
        case 51: case 53: case 55: return "Drizzle";
        case 61: case 63: case 65: return "Rain";
        case 66: case 67: return "Freezing rain";
        case 71: case 73: case 75: return "Snow";
        case 77: return "Snow grains";
        case 80: case 81: case 82: return "Rain showers";
        case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Thunderstorm with hail";
        default: return "Unknown";
    }
}
```

### 2.2 HTTP Request Function

```c
/**
 * @brief Make HTTP request to Open-Meteo API
 * 
 * @param url Full URL with query parameters
 * @param response_out JSON response string (caller must free)
 * @param error_message_out Error message (caller must free)
 * @return ETHERVOX_SUCCESS or error code
 */
static ethervox_result_t weather_http_request(
    const char* url,
    char** response_out,
    char** error_message_out
) {
#if HAVE_LIBCURL
    // Use existing platform_http.h wrapper
    // Implement timeout, retry, error handling
    // Log request URL (without sensitive data)
    // Return ETHERVOX_ERROR_NETWORK on failure
    // Return ETHERVOX_ERROR_API_RATE_LIMIT on HTTP 429
    // Return ETHERVOX_ERROR_API_RESPONSE on HTTP 500+
#else
    *error_message_out = strdup("Weather tools require libcurl support");
    return ETHERVOX_ERROR_NOT_SUPPORTED;
#endif
}
```

### 2.3 JSON Parsing Function

```c
/**
 * @brief Parse Open-Meteo JSON response
 * 
 * Expected JSON structure:
 * {
 *   "latitude": 40.7128,
 *   "longitude": -74.0060,
 *   "timezone": "America/New_York",
 *   "current_weather": {
 *     "temperature": 22.5,
 *     "windspeed": 12.3,
 *     "weathercode": 2,
 *     "time": "2026-05-14T15:00"
 *   },
 *   "hourly": {
 *     "time": ["2026-05-14T00:00", ...],
 *     "temperature_2m": [18.5, 19.2, ...],
 *     "weathercode": [2, 1, ...],
 *     ...
 *   },
 *   "daily": { ... }
 * }
 */
static ethervox_result_t weather_parse_response(
    const char* json,
    const ethervox_weather_request_t* request,
    ethervox_weather_response_t** response_out,
    char** error_message_out
) {
    // Use cJSON for parsing
    // Extract coordinates, timezone
    // Parse current_weather, hourly, or daily based on request type
    // Convert WMO codes to human-readable conditions
    // Allocate and populate weather_response_t
    // Handle missing fields gracefully
}
```

---

## Step 3: Tool Wrapper (`src/plugins/weather_tools/weather_registry.c`)

```c
/**
 * @brief Tool wrapper for weather forecast
 * 
 * Expected JSON parameters:
 * {
 *   "location": "New York, NY" or "40.7128,-74.0060",
 *   "forecast_type": "current" | "hourly" | "daily" | "7-day",
 *   "days_ahead": 0-7 (optional, default 0)
 * }
 * 
 * Returns JSON:
 * {
 *   "location": "New York, NY",
 *   "coordinates": {"lat": 40.7128, "lon": -74.0060},
 *   "current": {
 *     "temperature": 22.5,
 *     "condition": "Partly cloudy",
 *     "humidity": 65,
 *     "wind_speed": 12.3
 *   },
 *   "forecasts": [
 *     {
 *       "time": "2026-05-14T15:00",
 *       "temperature": 23.0,
 *       "condition": "Sunny",
 *       ...
 *     }
 *   ],
 *   "from_cache": false,
 *   "data_source": "Open-Meteo"
 * }
 */
static int tool_weather_forecast_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    // Parse location parameter
    // Parse forecast_type parameter (default to "current")
    // Parse days_ahead parameter (default to 0)
    
    // Geocode location if needed
    // Create weather_request_t
    // Call ethervox_weather_get_forecast()
    
    // Format response as JSON string
    // Free weather response
    
    // Handle all error cases with descriptive messages
}
```

### Tool Registration

```c
int ethervox_weather_tools_register(void* registry_ptr) {
    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)registry_ptr;
    
    ethervox_tool_t tool_weather = {
        .name = "get_weather",
        .description = 
            "Get current weather or forecast for any location worldwide. "
            "Returns temperature, conditions, humidity, wind, and precipitation. "
            "Data from Open-Meteo (MIT licensed). "
            "Examples: "
            "'Get weather for Tokyo', "
            "'Get 7-day forecast for London', "
            "'What's the current temperature in New York?'",
        .parameters_json_schema =
            "{\"type\":\"object\",\"properties\":{"
            "\"location\":{\"type\":\"string\",\"description\":\"City name (e.g., 'London', 'New York, NY') or coordinates (e.g., '40.7128,-74.0060')\"},"
            "\"forecast_type\":{\"type\":\"string\",\"enum\":[\"current\",\"hourly\",\"daily\",\"7-day\"],\"description\":\"Type of forecast\",\"default\":\"current\"},"
            "\"days_ahead\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":7,\"description\":\"Days ahead (0=today, 1=tomorrow, etc.)\",\"default\":0}"
            "},\"required\":[\"location\"]}",
        .execute = tool_weather_forecast_wrapper,
        .is_deterministic = false,       // Weather changes over time
        .requires_confirmation = false,   // Safe read-only API call
        .is_stateful = false,            // No state modification
        .estimated_latency_ms = 1500.0f  // API call + parsing
    };
    
    return ethervox_tool_registry_add(registry, &tool_weather);
}
```

---

## Step 4: Caching Implementation (`src/plugins/weather_tools/weather_cache.c`)

```c
/**
 * @brief Cache entry structure
 */
typedef struct weather_cache_entry {
    char key[64];                 // hash(lat+lon+type+date)
    time_t expires_at;            // TTL expiration timestamp
    ethervox_weather_response_t* response; // Cached response
    struct weather_cache_entry* next; // LRU linked list
} weather_cache_entry_t;

/**
 * @brief Generate cache key from request
 */
static void weather_cache_make_key(
    const ethervox_weather_request_t* request,
    char* key_out,
    size_t key_size
) {
    // Simple hash: "lat_lon_type_date"
    snprintf(key_out, key_size, "%.4f_%.4f_%d_%d",
             request->latitude,
             request->longitude,
             request->forecast_type,
             request->days_ahead);
}

/**
 * @brief Get cached response (if valid)
 */
static ethervox_weather_response_t* weather_cache_get(
    const ethervox_weather_request_t* request
) {
    // Generate key
    // Search cache
    // Check TTL expiration
    // Update LRU order
    // Return response or NULL
}

/**
 * @brief Store response in cache
 */
static void weather_cache_put(
    const ethervox_weather_request_t* request,
    const ethervox_weather_response_t* response
) {
    // Generate key
    // Check if already exists (update TTL)
    // If cache full, evict LRU entry
    // Deep copy response
    // Insert at head of LRU list
}
```

---

## Step 5: Unit Tests

### Test 1: JSON Parsing (`tests/unit/test_weather_parse.c`)

```c
void test_parse_current_weather(void) {
    const char* json = 
        "{"
        "\"latitude\":40.7128,\"longitude\":-74.0060,"
        "\"timezone\":\"America/New_York\","
        "\"current_weather\":{"
            "\"temperature\":22.5,"
            "\"windspeed\":12.3,"
            "\"weathercode\":2,"
            "\"time\":\"2026-05-14T15:00\""
        "}"
        "}";
    
    ethervox_weather_request_t request = {
        .latitude = 40.7128,
        .longitude = -74.0060,
        .forecast_type = ETHERVOX_WEATHER_CURRENT
    };
    
    ethervox_weather_response_t* response = NULL;
    char* error = NULL;
    
    ethervox_result_t ret = weather_parse_response(json, &request, &response, &error);
    
    assert(ret == ETHERVOX_SUCCESS);
    assert(response != NULL);
    assert(response->forecast_count == 1);
    assert(fabs(response->forecasts[0].temperature - 22.5) < 0.01);
    assert(strcmp(response->forecasts[0].condition, "Partly cloudy") == 0);
    
    ethervox_weather_free_response(response);
}
```

### Test 2: Cache Logic (`tests/unit/test_weather_cache.c`)

```c
void test_cache_hit_miss(void) {
    ethervox_weather_init(NULL);
    
    ethervox_weather_request_t request = {
        .latitude = 40.7128,
        .longitude = -74.0060,
        .forecast_type = ETHERVOX_WEATHER_CURRENT,
        .use_cache = true
    };
    
    // First call - cache miss
    ethervox_weather_response_t* response1 = NULL;
    ethervox_weather_get_forecast(&request, &response1, NULL);
    assert(!response1->from_cache);
    
    // Second call - cache hit
    ethervox_weather_response_t* response2 = NULL;
    ethervox_weather_get_forecast(&request, &response2, NULL);
    assert(response2->from_cache);
    
    ethervox_weather_free_response(response1);
    ethervox_weather_free_response(response2);
    ethervox_weather_cleanup();
}
```

---

## Step 6: Integration Test (`src/dialogue/integration_tests.c`)

```c
static void test_weather_tool(void) {
    TEST_HEADER("Test X: Weather Forecast Tool");
    
    // Test with coordinates
    const char* args1 = "{\"location\":\"40.7128,-74.0060\",\"forecast_type\":\"current\"}";
    char* result1 = NULL;
    char* error1 = NULL;
    
    int ret = tool_weather_forecast_wrapper(args1, &result1, &error1);
    
    if (ret != 0) {
        TEST_FAIL("Coordinates request failed: %s", error1 ? error1 : "unknown");
        free(error1);
        return;
    }
    
    // Verify JSON result contains temperature
    if (!strstr(result1, "\"temperature\"")) {
        TEST_FAIL("Result missing temperature field");
        free(result1);
        return;
    }
    
    free(result1);
    
    // Test with city name
    const char* args2 = "{\"location\":\"London\",\"forecast_type\":\"daily\"}";
    char* result2 = NULL;
    char* error2 = NULL;
    
    ret = tool_weather_forecast_wrapper(args2, &result2, &error2);
    
    if (ret != 0) {
        TEST_FAIL("City name request failed: %s", error2 ? error2 : "unknown");
        free(error2);
        return;
    }
    
    free(result2);
    
    TEST_PASS("Weather tool works correctly");
    g_tests_passed++;
}
```

---

## Step 7: CMakeLists Configuration

### `src/plugins/weather_tools/CMakeLists.txt`

```cmake
# Weather tools plugin
if(HAVE_LIBCURL)
    add_library(weather_tools STATIC
        weather_core.c
        weather_cache.c
        weather_registry.c
    )
    
    target_include_directories(weather_tools PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(weather_tools
        cJSON
        ${CURL_LIBRARIES}
    )
    
    message(STATUS "Weather tools enabled (libcurl available)")
else()
    message(STATUS "Weather tools disabled (libcurl not available)")
endif()
```

### `tests/CMakeLists.txt` additions

```cmake
if(HAVE_LIBCURL)
    # Weather parsing test
    add_executable(test_weather_parse unit/test_weather_parse.c)
    target_link_libraries(test_weather_parse ethervoxai weather_tools)
    add_test(NAME WeatherParse COMMAND test_weather_parse)
    
    # Weather cache test
    add_executable(test_weather_cache unit/test_weather_cache.c)
    target_link_libraries(test_weather_cache ethervoxai weather_tools)
    add_test(NAME WeatherCache COMMAND test_weather_cache)
    
    # Weather HTTP test (mocked)
    add_executable(test_weather_http unit/test_weather_http.c)
    target_link_libraries(test_weather_http ethervoxai weather_tools)
    add_test(NAME WeatherHTTP COMMAND test_weather_http)
    
    set_tests_properties(WeatherParse WeatherCache WeatherHTTP PROPERTIES
        TIMEOUT 30
        LABELS "unit;weather_tools"
    )
endif()
```

---

## Step 8: Dialogue System Registration

### Modify `src/dialogue/dialogue_core.c`

```c
// At the top, add include:
#include "ethervox/weather_tools.h"

// In tool registry initialization (around line 690):
    // Register unit conversion tool
    if (ethervox_unit_conversion_register(registry) == 0) {
        tool_count++;
        // ... existing logging ...
    }
    
#if HAVE_LIBCURL
    // Register weather tools (requires libcurl)
    if (ethervox_weather_tools_register(registry) == 0) {
        tool_count++;
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGI("Registered weather forecast tool with Governor");
#else
        printf("Registered weather forecast tool with Governor\n");
#endif
    }
#endif
    
    // Register memory tools if memory store is available...
```

---

## Error Handling Strategy

### Use Existing Error Codes

```c
// From include/ethervox/error.h

ETHERVOX_ERROR_NETWORK              // -600: Network connection failed
ETHERVOX_ERROR_API_CALL             // -601: API call failed
ETHERVOX_ERROR_API_RESPONSE         // -602: Invalid API response
ETHERVOX_ERROR_API_RATE_LIMIT       // -603: Rate limit exceeded
ETHERVOX_ERROR_DOWNLOAD_TIMEOUT     // -608: Request timeout
```

### Error Messages for LLM

Provide helpful error messages that the LLM can relay to users:

```c
if (ret == ETHERVOX_ERROR_NETWORK) {
    *error_message_out = strdup(
        "Cannot fetch weather: No internet connection. "
        "Weather data requires online access to Open-Meteo API."
    );
}

if (ret == ETHERVOX_ERROR_API_RESPONSE) {
    *error_message_out = strdup(
        "Weather service returned invalid data. "
        "This may be a temporary issue. Please try again."
    );
}

if (ret == ETHERVOX_ERROR_DOWNLOAD_TIMEOUT) {
    *error_message_out = strdup(
        "Weather request timed out after 10 seconds. "
        "The weather service may be experiencing issues."
    );
}
```

---

## Platform Support Matrix

| Platform | Support | Notes |
|----------|---------|-------|
| **macOS** | ✅ Full | libcurl available via Homebrew |
| **Linux** | ✅ Full | libcurl via apt/yum |
| **Windows** | ✅ Full | libcurl via vcpkg or MinGW |
| **Android** | ✅ Full | libcurl via NDK |
| **Raspberry Pi** | ✅ Full | libcurl via apt |
| **ESP32** | ❌ Not supported | Offline-only, no libcurl |

For ESP32, return `ETHERVOX_ERROR_NOT_SUPPORTED` with message:
```c
"Weather forecasts require internet connection. "
"This feature is not available on offline-only platforms."
```

---

## Attribution Requirements

### Update `THIRD_PARTY_LICENSES.md`

```markdown
### Open-Meteo Weather API
- **License**: MIT License
- **Purpose**: Weather forecast data for get_weather tool
- **URL**: https://open-meteo.com
- **API Documentation**: https://open-meteo.com/en/docs
- **Copyright**: Copyright (c) 2022 Patrick Zippenfenig

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

[Full MIT license text...]

**Attribution in EthervoxAI**: Weather data provided by Open-Meteo (open-meteo.com)
**Data Sources**: Open-Meteo aggregates data from national weather services including
DWD (Germany), NOAA (USA), Meteodeo France, and others.
```

### Display Attribution in App

When returning weather results to users, include:
```json
{
  "weather": { ... },
  "data_source": "Open-Meteo",
  "attribution": "Weather data from Open-Meteo (open-meteo.com)"
}
```

---

## Performance Considerations

### Expected Latencies

- **Geocoding**: 100-300ms (first call), 0ms (cached)
- **Current weather**: 200-500ms
- **Hourly forecast**: 300-700ms
- **7-day forecast**: 400-1000ms
- **Cache hit**: <1ms

### Optimization Strategies

1. **Aggressive Caching**
   - Cache geocoding results for 24 hours
   - Cache forecasts for 1 hour
   - LRU eviction for memory management

2. **Parallel Requests**
   - If multiple locations requested, fetch in parallel
   - Use libcurl multi-handle interface

3. **Response Compression**
   - Accept gzip encoding from API
   - Reduces bandwidth by ~70%

4. **Smart TTL**
   - Current weather: 1 hour TTL
   - Hourly forecast: 1 hour TTL
   - Daily forecast: 3 hour TTL
   - Longer forecasts less accurate = longer TTL acceptable

---

## Security Considerations

### Input Validation

```c
// Validate latitude/longitude ranges
if (latitude < -90.0 || latitude > 90.0) {
    return ETHERVOX_ERROR_INVALID_PARAMETER;
}
if (longitude < -180.0 || longitude > 180.0) {
    return ETHERVOX_ERROR_INVALID_PARAMETER;
}

// Validate location name length (prevent injection)
if (strlen(location_name) > 255) {
    return ETHERVOX_ERROR_INVALID_PARAMETER;
}

// URL encode location name before API call
char* encoded = url_encode(location_name);
```

### Privacy Considerations

1. **Logging**: Do NOT log exact coordinates in production
   ```c
   // Bad: ethervox_log("Weather request for 40.7128,-74.0060");
   // Good: ethervox_log("Weather request for New York, NY");
   ```

2. **Cache**: Clear cache on user logout/session end
   ```c
   // In cleanup function
   ethervox_weather_clear_cache();
   ```

3. **API Requests**: No user identification sent to Open-Meteo
   - No API key required
   - No user tracking
   - Requests are anonymous

---

## Testing Strategy

### Unit Tests (Fast, Isolated)

Run daily during development:
```bash
./build/tests/test_weather_parse
./build/tests/test_weather_cache
./build/tests/test_weather_http  # Mocked, no real API calls
```

### Integration Tests (Slow, Real API)

Run before each commit:
```bash
echo "/test" | ./build/ethervoxai
```

### Manual Testing (Interactive)

```bash
# Start EthervoxAI CLI
./build/ethervoxai

# Test various queries
User: What's the weather in New York?
User: Get 7-day forecast for Tokyo
User: What's the temperature in London?
User: Will it rain tomorrow in Seattle?
```

---

## Implementation Timeline

### Day 1: Core Implementation (6-8 hours)

- [ ] 9:00-10:00: Create header file with data structures
- [ ] 10:00-12:00: Implement HTTP client wrapper (weather_http_request)
- [ ] 12:00-13:00: Lunch break
- [ ] 13:00-15:00: Implement JSON parsing (weather_parse_response)
- [ ] 15:00-17:00: Implement geocoding function
- [ ] 17:00-17:30: Initial testing with curl commands

### Day 2: Tool Integration (6-8 hours)

- [ ] 9:00-10:30: Implement caching layer
- [ ] 10:30-12:00: Create tool wrapper function
- [ ] 12:00-13:00: Lunch break
- [ ] 13:00-14:30: Implement tool registration
- [ ] 14:30-16:00: Register in dialogue_core.c
- [ ] 16:00-17:00: Manual testing in CLI
- [ ] 17:00-17:30: Fix bugs found during testing

### Day 3: Testing & Documentation (6-8 hours)

- [ ] 9:00-11:00: Write unit tests (parse, cache, http)
- [ ] 11:00-12:00: Write integration test
- [ ] 12:00-13:00: Lunch break
- [ ] 13:00-14:30: Test on all platforms (macOS, Linux, Android)
- [ ] 14:30-16:00: Write user documentation
- [ ] 16:00-17:00: Update THIRD_PARTY_LICENSES.md
- [ ] 17:00-17:30: Final code review and cleanup

---

## Success Criteria

### Functional Requirements

- [ ] Get current weather for any location
- [ ] Get hourly forecast (next 24 hours)
- [ ] Get daily forecast (today)
- [ ] Get 7-day forecast
- [ ] Support city names and coordinates
- [ ] Cache responses to reduce API calls
- [ ] Handle network errors gracefully
- [ ] Return human-readable conditions

### Non-Functional Requirements

- [ ] Average latency < 1.5 seconds
- [ ] Cache hit rate > 60% in typical usage
- [ ] No memory leaks (verified with valgrind)
- [ ] Thread-safe (can be called from multiple threads)
- [ ] Works on macOS, Linux, Windows, Android, Raspberry Pi
- [ ] Degrades gracefully when offline (returns cached data)

### Code Quality

- [ ] All functions have ethervox_result_t return type
- [ ] Comprehensive error handling
- [ ] Unit test coverage > 80%
- [ ] Integration tests pass
- [ ] No compiler warnings
- [ ] Follows existing code style
- [ ] Documentation complete

---

## Future Enhancements (Not in Initial Implementation)

### Phase 2 Features

- [ ] Air quality index (Open-Meteo supports this)
- [ ] UV index
- [ ] Sunrise/sunset times
- [ ] Weather alerts (severe weather warnings)
- [ ] Historical weather data
- [ ] Climate averages for location

### Phase 3 Features

- [ ] Multi-source comparison (Open-Meteo + NOAA)
- [ ] Consensus forecasting with confidence scores
- [ ] Persistent cache (SQLite storage)
- [ ] Weather-triggered automations
- [ ] Voice notifications for weather changes

---

## Notes for Implementer

### Quick Start Checklist

Before starting implementation:
1. [ ] Verify libcurl is available: `pkg-config --libs libcurl`
2. [ ] Test Open-Meteo API manually:
   ```bash
   curl "https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current_weather=true"
   ```
3. [ ] Review existing tool implementations in `src/plugins/`
4. [ ] Read error handling guide: `.github/error-handling-reference.md`
5. [ ] Set up test location coordinates for development

### Common Pitfalls to Avoid

1. **Don't parse JSON manually** - Use cJSON library (already in project)
2. **Don't forget to URL-encode** - City names may contain spaces
3. **Don't hardcode URLs** - Use #define constants
4. **Don't skip caching** - API calls are slow
5. **Don't log user locations** - Privacy concern
6. **Don't forget platform checks** - ESP32 doesn't support this
7. **Don't forget attribution** - Open-Meteo license requires it

### Debugging Tips

```c
// Enable verbose HTTP logging
#define WEATHER_DEBUG 1
#if WEATHER_DEBUG
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Weather API request: %s", url);
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Response: %s", response_json);
#endif
```

### Useful Test Commands

```bash
# Test geocoding
curl "https://geocoding-api.open-meteo.com/v1/search?name=London&count=1"

# Test current weather
curl "https://api.open-meteo.com/v1/forecast?latitude=51.51&longitude=-0.13&current_weather=true"

# Test hourly forecast
curl "https://api.open-meteo.com/v1/forecast?latitude=51.51&longitude=-0.13&hourly=temperature_2m,weathercode&forecast_days=1"

# Test daily forecast
curl "https://api.open-meteo.com/v1/forecast?latitude=51.51&longitude=-0.13&daily=weathercode,temperature_2m_max,temperature_2m_min&forecast_days=7"
```

---

## Questions & Decisions

### Q: Should we support temperature units (Celsius/Fahrenheit)?

**Decision**: Always use Celsius internally, convert in presentation layer if needed.
- Open-Meteo returns Celsius by default
- Simpler internal representation
- Governor can convert in response if user asks for Fahrenheit

### Q: Should we support multiple data sources (NOAA, OpenWeatherMap)?

**Decision**: Not in Phase 1. Start with Open-Meteo only.
- Open-Meteo already aggregates multiple sources
- Keeps implementation simple
- Can add multi-source in Phase 2 if needed

### Q: How should we handle cache invalidation?

**Decision**: Time-based TTL only (1 hour for forecasts).
- No manual invalidation needed
- Simple implementation
- Good enough for weather data (changes slowly)

### Q: Should we cache negative results (city not found)?

**Decision**: Yes, cache for 5 minutes.
- Prevents repeated failed API calls
- User might retry immediately
- Short TTL in case of typo correction

---

## Contact & Support

- **Implementation Questions**: Refer to `docs/ADDING_NEW_LLM_TOOL.md`
- **Error Handling**: See `.github/error-handling-reference.md`
- **API Documentation**: https://open-meteo.com/en/docs
- **License Questions**: Check `THIRD_PARTY_LICENSES.md`

---

**Status**: Ready for implementation
**Estimated Effort**: 2-3 days (16-24 hours)
**Priority**: Medium (enhancement, not critical feature)
**Assignee**: TBD
