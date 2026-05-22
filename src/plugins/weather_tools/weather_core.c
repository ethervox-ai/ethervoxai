/**
 * @file weather_core.c
 * @brief Weather forecast core implementation using Open-Meteo API
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * 
 * Weather data provided by Open-Meteo (MIT License)
 * API: https://open-meteo.com
 */

#include "ethervox/weather_tools.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

// Platform-specific HTTP implementation
#ifdef ETHERVOX_PLATFORM_ANDROID
// Android: Use JNI-based HTTP client (implemented in weather_http_android.c)
extern ethervox_result_t android_http_get_request(
    const char* url,
    char** response_out,
    char** error_message_out);
#define PLATFORM_HTTP_GET android_http_get_request
#else
// Desktop: Use libcurl
#if HAVE_LIBCURL
#include <curl/curl.h>
#else
#error "Weather tools require libcurl support on desktop platforms. Install libcurl development packages."
#endif
#endif

#include "cJSON.h"

// ============================================================================
// Constants
// ============================================================================

#define OPENMETEO_FORECAST_URL "https://api.open-meteo.com/v1/forecast"
#define OPENMETEO_GEOCODING_URL "https://geocoding-api.open-meteo.com/v1/search"
#define DEFAULT_USER_AGENT "EthervoxAI/1.0 (Weather Tool)"

// ============================================================================
// Module State
// ============================================================================

static ethervox_weather_config_t g_config = {
    .enable_cache = true,
    .cache_ttl_sec = ETHERVOX_WEATHER_CACHE_TTL_SEC,
    .max_cache_entries = 100,
    .request_timeout_sec = ETHERVOX_WEATHER_API_TIMEOUT_SEC,
    .max_retries = ETHERVOX_WEATHER_MAX_RETRIES,
    .user_agent = DEFAULT_USER_AGENT
};

static bool g_initialized = false;

// ============================================================================
// Forward Declarations
// ============================================================================

// Cache functions (implemented in weather_cache.c)
extern ethervox_weather_response_t* weather_cache_get(const ethervox_weather_request_t* request);
extern void weather_cache_put(const ethervox_weather_request_t* request, const ethervox_weather_response_t* response);
extern void weather_cache_clear(void);
extern void weather_cache_get_stats(uint64_t* hit_count, uint64_t* miss_count, size_t* size);

// ============================================================================
// WMO Weather Code Conversion
// ============================================================================

/**
 * @brief Convert WMO weather code to human-readable condition
 * 
 * WMO Code Table 4677
 * See: https://www.nodc.noaa.gov/archive/arc0021/0002199/1.1/data/0-data/HTML/WMO-CODE/WMO4677.HTM
 */
static const char* wmo_code_to_condition(int code) {
    switch (code) {
        case 0: return "Clear sky";
        case 1: return "Mainly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: case 48: return "Foggy";
        case 51: case 53: case 55: return "Drizzle";
        case 56: case 57: return "Freezing drizzle";
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

// ============================================================================
// HTTP Request Handling
// ============================================================================

#ifdef ETHERVOX_PLATFORM_ANDROID
/**
 * @brief Make HTTP GET request - Android implementation (delegates to JNI)
 */
static ethervox_result_t http_get_request(
    const char* url,
    char** response_out,
    char** error_message_out
) {
    // On Android, use JNI-based HTTP client
    return android_http_get_request(url, response_out, error_message_out);
}

/**
 * @brief URL-encode a string - Android simple implementation
 */
static char* url_encode(const char* str) {
    if (!str) return NULL;
    
    // Simple URL encoding for Android (handles spaces and special chars)
    size_t len = strlen(str);
    char* encoded = malloc(len * 3 + 1);  // Max 3x expansion
    if (!encoded) return NULL;
    
    const char* hex = "0123456789ABCDEF";
    size_t pos = 0;
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[pos++] = c;
        } else if (c == ' ') {
            encoded[pos++] = '+';
        } else {
            encoded[pos++] = '%';
            encoded[pos++] = hex[c >> 4];
            encoded[pos++] = hex[c & 0x0F];
        }
    }
    encoded[pos] = '\0';
    
    return encoded;
}

#else  // Desktop/libcurl implementation

/**
 * @brief Callback for curl to write response data
 */
struct curl_response {
    char* data;
    size_t size;
};

static size_t weather_curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct curl_response* response = (struct curl_response*)userp;
    
    char* ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Failed to allocate memory for HTTP response");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;
    
    return realsize;
}

/**
 * @brief Make HTTP GET request with retry logic
 */
static ethervox_result_t http_get_request(
    const char* url,
    char** response_out,
    char** error_message_out
) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error_message_out) {
            *error_message_out = strdup("Failed to initialize HTTP client");
        }
        return ETHERVOX_ERROR_NETWORK;
    }
    
    struct curl_response response = {0};
    ethervox_result_t result = ETHERVOX_SUCCESS;
    CURLcode res;
    long http_code = 0;
    int retry_count = 0;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "HTTP GET: %s", url);
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, weather_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, g_config.user_agent);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)g_config.request_timeout_sec);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    
    // Retry loop
    while (retry_count <= g_config.max_retries) {
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            
            if (http_code == 200) {
                // Success
                *response_out = response.data;
                result = ETHERVOX_SUCCESS;
                break;
            } else if (http_code == 429) {
                // Rate limit - don't retry
                result = ETHERVOX_ERROR_API_RATE_LIMIT;
                if (error_message_out) {
                    *error_message_out = strdup("API rate limit exceeded");
                }
                break;
            } else if (http_code >= 500) {
                // Server error - retry
                ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__,
                            "HTTP %ld - retrying (%d/%d)", http_code, retry_count + 1, g_config.max_retries);
                retry_count++;
                if (retry_count <= g_config.max_retries) {
                    // Exponential backoff: 1s, 2s, 4s
                    struct timespec ts = {.tv_sec = 1 << (retry_count - 1), .tv_nsec = 0};
                    nanosleep(&ts, NULL);
                    continue;
                }
                result = ETHERVOX_ERROR_API_RESPONSE;
                if (error_message_out) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "API server error (HTTP %ld)", http_code);
                    *error_message_out = strdup(msg);
                }
                break;
            } else {
                // Other HTTP error
                result = ETHERVOX_ERROR_API_RESPONSE;
                if (error_message_out) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "API request failed (HTTP %ld)", http_code);
                    *error_message_out = strdup(msg);
                }
                break;
            }
        } else {
            // Network error
            if (res == CURLE_OPERATION_TIMEDOUT) {
                result = ETHERVOX_ERROR_DOWNLOAD_TIMEOUT;
                if (error_message_out) {
                    *error_message_out = strdup("Request timeout - weather service not responding");
                }
            } else {
                result = ETHERVOX_ERROR_NETWORK;
                if (error_message_out) {
                    char msg[512];
                    snprintf(msg, sizeof(msg), "Network error: %s", curl_easy_strerror(res));
                    *error_message_out = strdup(msg);
                }
            }
            break;
        }
    }
    
    if (result != ETHERVOX_SUCCESS && response.data) {
        free(response.data);
    }
    
    curl_easy_cleanup(curl);
    return result;
}

/**
 * @brief URL-encode a string
 */
static char* url_encode(const char* str) {
    CURL* curl = curl_easy_init();
    if (!curl) return NULL;
    
    char* encoded = curl_easy_escape(curl, str, 0);
    char* result = encoded ? strdup(encoded) : NULL;
    
    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);
    
    return result;
}

#endif  // ETHERVOX_PLATFORM_ANDROID

// ============================================================================
// Geocoding
// ============================================================================

/**
 * @brief Expand US state abbreviations to full names
 * Example: "Portland, OR" -> "Portland, Oregon"
 */
static char* expand_us_state(const char* location_name) {
    // Map of US state abbreviations to full names
    static const struct {
        const char* abbr;
        const char* full;
    } states[] = {
        {"AL", "Alabama"}, {"AK", "Alaska"}, {"AZ", "Arizona"}, {"AR", "Arkansas"},
        {"CA", "California"}, {"CO", "Colorado"}, {"CT", "Connecticut"}, {"DE", "Delaware"},
        {"FL", "Florida"}, {"GA", "Georgia"}, {"HI", "Hawaii"}, {"ID", "Idaho"},
        {"IL", "Illinois"}, {"IN", "Indiana"}, {"IA", "Iowa"}, {"KS", "Kansas"},
        {"KY", "Kentucky"}, {"LA", "Louisiana"}, {"ME", "Maine"}, {"MD", "Maryland"},
        {"MA", "Massachusetts"}, {"MI", "Michigan"}, {"MN", "Minnesota"}, {"MS", "Mississippi"},
        {"MO", "Missouri"}, {"MT", "Montana"}, {"NE", "Nebraska"}, {"NV", "Nevada"},
        {"NH", "New Hampshire"}, {"NJ", "New Jersey"}, {"NM", "New Mexico"}, {"NY", "New York"},
        {"NC", "North Carolina"}, {"ND", "North Dakota"}, {"OH", "Ohio"}, {"OK", "Oklahoma"},
        {"OR", "Oregon"}, {"PA", "Pennsylvania"}, {"RI", "Rhode Island"}, {"SC", "South Carolina"},
        {"SD", "South Dakota"}, {"TN", "Tennessee"}, {"TX", "Texas"}, {"UT", "Utah"},
        {"VT", "Vermont"}, {"VA", "Virginia"}, {"WA", "Washington"}, {"WV", "West Virginia"},
        {"WI", "Wisconsin"}, {"WY", "Wyoming"}, {"DC", "Washington DC"}
    };
    
    // Look for comma followed by 2-letter state code
    const char* comma = strrchr(location_name, ',');
    if (!comma) return strdup(location_name);
    
    // Extract potential state abbreviation
    const char* state_start = comma + 1;
    while (*state_start == ' ') state_start++;
    
    if (strlen(state_start) != 2 && strlen(state_start) != 3) {
        return strdup(location_name);  // Not a 2-letter code
    }
    
    // Convert to uppercase for comparison
    char abbr[3] = {0};
    abbr[0] = toupper(state_start[0]);
    abbr[1] = toupper(state_start[1]);
    
    // Find matching state
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
        if (strcmp(abbr, states[i].abbr) == 0) {
            // Build expanded location name
            size_t city_len = comma - location_name;
            size_t full_len = city_len + 2 + strlen(states[i].full) + 1;  // city + ", " + state + null
            char* expanded = malloc(full_len);
            if (!expanded) return strdup(location_name);
            
            strncpy(expanded, location_name, city_len);
            expanded[city_len] = '\0';
            strcat(expanded, ", ");
            strcat(expanded, states[i].full);
            
            ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                        "Expanded '%s' to '%s'", location_name, expanded);
            return expanded;
        }
    }
    
    return strdup(location_name);  // No match, return original
}

/**
 * @brief Try geocoding with a specific location string
 * Returns ETHERVOX_SUCCESS if found, ETHERVOX_ERROR_NOT_FOUND if not found, other errors otherwise
 */
static ethervox_result_t try_geocode_variant(
    const char* location_variant,
    double* latitude_out,
    double* longitude_out
) {
    // URL-encode location name
    char* encoded_name = url_encode(location_variant);
    if (!encoded_name) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Build geocoding URL
    char url[1024];
    snprintf(url, sizeof(url), "%s?name=%s&count=1&language=en&format=json",
             OPENMETEO_GEOCODING_URL, encoded_name);
    free(encoded_name);
    
    // Make request
    char* response_json = NULL;
    char* error_msg = NULL;
    ethervox_result_t result = http_get_request(url, &response_json, &error_msg);
    
    if (result != ETHERVOX_SUCCESS) {
        free(error_msg);
        return result;
    }
    
    // Parse JSON response
    cJSON* root = cJSON_Parse(response_json);
    free(response_json);
    
    if (!root) {
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    cJSON* results = cJSON_GetObjectItem(root, "results");
    if (!results || !cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        cJSON_Delete(root);
        return ETHERVOX_ERROR_NOT_FOUND;
    }
    
    // Extract coordinates
    cJSON* first_result = cJSON_GetArrayItem(results, 0);
    cJSON* lat = cJSON_GetObjectItem(first_result, "latitude");
    cJSON* lon = cJSON_GetObjectItem(first_result, "longitude");
    
    if (!lat || !lon || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
        cJSON_Delete(root);
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    *latitude_out = lat->valuedouble;
    *longitude_out = lon->valuedouble;
    
    cJSON_Delete(root);
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_weather_geocode(
    const char* location_name,
    double* latitude_out,
    double* longitude_out,
    char** error_message_out
) {
    if (!g_initialized) {
        if (error_message_out) {
            *error_message_out = strdup("Weather tools not initialized");
        }
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    if (!location_name || !latitude_out || !longitude_out) {
        if (error_message_out) {
            *error_message_out = strdup("Invalid parameters");
        }
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ethervox_result_t result;
    
    // Strategy 1: Try with expanded state name (e.g., "Portland, OR" -> "Portland, Oregon")
    char* expanded_location = expand_us_state(location_name);
    result = try_geocode_variant(expanded_location, latitude_out, longitude_out);
    
    if (result == ETHERVOX_SUCCESS) {
        ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                    "Geocoded '%s' (expanded from '%s') -> lat=%.4f, lon=%.4f",
                    expanded_location, location_name, *latitude_out, *longitude_out);
        free(expanded_location);
        return ETHERVOX_SUCCESS;
    }
    
    // Strategy 2: If that failed and we have a comma, try just the city name
    const char* comma = strchr(location_name, ',');
    if (comma && result == ETHERVOX_ERROR_NOT_FOUND) {
        // Extract city name only
        size_t city_len = comma - location_name;
        char city_only[256];
        if (city_len < sizeof(city_only)) {
            strncpy(city_only, location_name, city_len);
            city_only[city_len] = '\0';
            
            // Trim trailing spaces
            char* end = city_only + strlen(city_only) - 1;
            while (end > city_only && *end == ' ') {
                *end = '\0';
                end--;
            }
            
            ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                        "Trying city name only: '%s'", city_only);
            
            result = try_geocode_variant(city_only, latitude_out, longitude_out);
            
            if (result == ETHERVOX_SUCCESS) {
                ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                            "Geocoded '%s' (city only from '%s') -> lat=%.4f, lon=%.4f",
                            city_only, location_name, *latitude_out, *longitude_out);
                free(expanded_location);
                return ETHERVOX_SUCCESS;
            }
        }
    }
    
    // Strategy 3: Try with ", USA" appended
    if (comma && result == ETHERVOX_ERROR_NOT_FOUND) {
        char usa_variant[512];
        size_t city_len = comma - location_name;
        if (city_len < sizeof(usa_variant) - 10) {
            strncpy(usa_variant, location_name, city_len);
            usa_variant[city_len] = '\0';
            strcat(usa_variant, ", USA");
            
            ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                        "Trying with USA suffix: '%s'", usa_variant);
            
            result = try_geocode_variant(usa_variant, latitude_out, longitude_out);
            
            if (result == ETHERVOX_SUCCESS) {
                ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                            "Geocoded '%s' (with USA from '%s') -> lat=%.4f, lon=%.4f",
                            usa_variant, location_name, *latitude_out, *longitude_out);
                free(expanded_location);
                return ETHERVOX_SUCCESS;
            }
        }
    }
    
    free(expanded_location);
    
    // All strategies failed
    if (result == ETHERVOX_ERROR_NOT_FOUND) {
        if (error_message_out) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Location '%s' not found. Try using just the city name or add country (e.g., 'Portland, USA')", location_name);
            *error_message_out = strdup(msg);
        }
    } else if (error_message_out) {
        *error_message_out = strdup("Geocoding request failed");
    }
    
    return result;
}

// ============================================================================
// Weather Forecast Parsing
// ============================================================================

/**
 * @brief Parse ISO 8601 timestamp to Unix time
 */
static time_t parse_iso8601(const char* timestamp) {
    struct tm tm = {0};
    sscanf(timestamp, "%d-%d-%dT%d:%d",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return mktime(&tm);
}

/**
 * @brief Parse current weather from JSON
 */
static ethervox_result_t parse_current_weather(
    cJSON* root,
    ethervox_weather_response_t* response
) {
    cJSON* current = cJSON_GetObjectItem(root, "current_weather");
    if (!current) {
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    response->forecast_count = 1;
    response->forecasts = calloc(1, sizeof(ethervox_weather_forecast_t));
    if (!response->forecasts) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    ethervox_weather_forecast_t* fc = &response->forecasts[0];
    
    cJSON* temp = cJSON_GetObjectItem(current, "temperature");
    cJSON* windspeed = cJSON_GetObjectItem(current, "windspeed");
    cJSON* winddirection = cJSON_GetObjectItem(current, "winddirection");
    cJSON* weathercode = cJSON_GetObjectItem(current, "weathercode");
    cJSON* time_item = cJSON_GetObjectItem(current, "time");
    
    if (temp && cJSON_IsNumber(temp)) {
        fc->temperature = (float)temp->valuedouble;
        fc->apparent_temperature = fc->temperature; // No feels-like in current weather
    }
    
    if (windspeed && cJSON_IsNumber(windspeed)) {
        fc->wind_speed = (float)windspeed->valuedouble;
    }
    
    if (winddirection && cJSON_IsNumber(winddirection)) {
        fc->wind_direction = winddirection->valueint;
    }
    
    if (weathercode && cJSON_IsNumber(weathercode)) {
        fc->weather_code = weathercode->valueint;
        strncpy(fc->condition, wmo_code_to_condition(fc->weather_code), sizeof(fc->condition) - 1);
    }
    
    if (time_item && cJSON_IsString(time_item)) {
        fc->timestamp = parse_iso8601(time_item->valuestring);
    } else {
        fc->timestamp = time(NULL);
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * @brief Parse hourly forecast from JSON
 */
static ethervox_result_t parse_hourly_forecast(
    cJSON* root,
    ethervox_weather_response_t* response,
    int max_hours
) {
    cJSON* hourly = cJSON_GetObjectItem(root, "hourly");
    if (!hourly) {
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    cJSON* time_array = cJSON_GetObjectItem(hourly, "time");
    cJSON* temp_array = cJSON_GetObjectItem(hourly, "temperature_2m");
    cJSON* apparent_temp_array = cJSON_GetObjectItem(hourly, "apparent_temperature");
    cJSON* humidity_array = cJSON_GetObjectItem(hourly, "relative_humidity_2m");
    cJSON* precipitation_array = cJSON_GetObjectItem(hourly, "precipitation");
    cJSON* precipitation_prob_array = cJSON_GetObjectItem(hourly, "precipitation_probability");
    cJSON* weathercode_array = cJSON_GetObjectItem(hourly, "weathercode");
    cJSON* windspeed_array = cJSON_GetObjectItem(hourly, "wind_speed_10m");
    cJSON* winddirection_array = cJSON_GetObjectItem(hourly, "wind_direction_10m");
    
    if (!time_array || !cJSON_IsArray(time_array)) {
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    int count = cJSON_GetArraySize(time_array);
    if (count > max_hours) count = max_hours;
    
    response->forecast_count = count;
    response->forecasts = calloc(count, sizeof(ethervox_weather_forecast_t));
    if (!response->forecasts) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    for (int i = 0; i < count; i++) {
        ethervox_weather_forecast_t* fc = &response->forecasts[i];
        
        cJSON* time_item = cJSON_GetArrayItem(time_array, i);
        if (time_item && cJSON_IsString(time_item)) {
            fc->timestamp = parse_iso8601(time_item->valuestring);
        }
        
        cJSON* temp = cJSON_GetArrayItem(temp_array, i);
        if (temp && cJSON_IsNumber(temp)) {
            fc->temperature = (float)temp->valuedouble;
        }
        
        cJSON* apparent = cJSON_GetArrayItem(apparent_temp_array, i);
        if (apparent && cJSON_IsNumber(apparent)) {
            fc->apparent_temperature = (float)apparent->valuedouble;
        }
        
        cJSON* humidity = cJSON_GetArrayItem(humidity_array, i);
        if (humidity && cJSON_IsNumber(humidity)) {
            fc->humidity = humidity->valueint;
        }
        
        cJSON* precip = cJSON_GetArrayItem(precipitation_array, i);
        if (precip && cJSON_IsNumber(precip)) {
            fc->precipitation = (float)precip->valuedouble;
        }
        
        cJSON* precip_prob = cJSON_GetArrayItem(precipitation_prob_array, i);
        if (precip_prob && cJSON_IsNumber(precip_prob)) {
            fc->precipitation_probability = precip_prob->valueint;
        }
        
        cJSON* weathercode = cJSON_GetArrayItem(weathercode_array, i);
        if (weathercode && cJSON_IsNumber(weathercode)) {
            fc->weather_code = weathercode->valueint;
            strncpy(fc->condition, wmo_code_to_condition(fc->weather_code), sizeof(fc->condition) - 1);
        }
        
        cJSON* windspeed = cJSON_GetArrayItem(windspeed_array, i);
        if (windspeed && cJSON_IsNumber(windspeed)) {
            fc->wind_speed = (float)windspeed->valuedouble;
        }
        
        cJSON* winddirection = cJSON_GetArrayItem(winddirection_array, i);
        if (winddirection && cJSON_IsNumber(winddirection)) {
            fc->wind_direction = winddirection->valueint;
        }
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * @brief Parse daily forecast from JSON
 */
static ethervox_result_t parse_daily_forecast(
    cJSON* root,
    ethervox_weather_response_t* response,
    int max_days
) {
    cJSON* daily = cJSON_GetObjectItem(root, "daily");
    if (!daily) {
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    cJSON* time_array = cJSON_GetObjectItem(daily, "time");
    cJSON* temp_max_array = cJSON_GetObjectItem(daily, "temperature_2m_max");
    cJSON* temp_min_array = cJSON_GetObjectItem(daily, "temperature_2m_min");
    cJSON* precipitation_array = cJSON_GetObjectItem(daily, "precipitation_sum");
    cJSON* precipitation_prob_array = cJSON_GetObjectItem(daily, "precipitation_probability_max");
    cJSON* weathercode_array = cJSON_GetObjectItem(daily, "weathercode");
    cJSON* windspeed_array = cJSON_GetObjectItem(daily, "wind_speed_10m_max");
    
    if (!time_array || !cJSON_IsArray(time_array)) {
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    int count = cJSON_GetArraySize(time_array);
    if (count > max_days) count = max_days;
    
    response->forecast_count = count;
    response->forecasts = calloc(count, sizeof(ethervox_weather_forecast_t));
    if (!response->forecasts) {
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    for (int i = 0; i < count; i++) {
        ethervox_weather_forecast_t* fc = &response->forecasts[i];
        
        cJSON* time_item = cJSON_GetArrayItem(time_array, i);
        if (time_item && cJSON_IsString(time_item)) {
            fc->timestamp = parse_iso8601(time_item->valuestring);
        }
        
        cJSON* temp_max = cJSON_GetArrayItem(temp_max_array, i);
        cJSON* temp_min = cJSON_GetArrayItem(temp_min_array, i);
        if (temp_max && temp_min && cJSON_IsNumber(temp_max) && cJSON_IsNumber(temp_min)) {
            // Average of max and min for simplicity
            fc->temperature = (float)((temp_max->valuedouble + temp_min->valuedouble) / 2.0);
            fc->apparent_temperature = fc->temperature;
        }
        
        cJSON* precip = cJSON_GetArrayItem(precipitation_array, i);
        if (precip && cJSON_IsNumber(precip)) {
            fc->precipitation = (float)precip->valuedouble;
        }
        
        cJSON* precip_prob = cJSON_GetArrayItem(precipitation_prob_array, i);
        if (precip_prob && cJSON_IsNumber(precip_prob)) {
            fc->precipitation_probability = precip_prob->valueint;
        }
        
        cJSON* weathercode = cJSON_GetArrayItem(weathercode_array, i);
        if (weathercode && cJSON_IsNumber(weathercode)) {
            fc->weather_code = weathercode->valueint;
            strncpy(fc->condition, wmo_code_to_condition(fc->weather_code), sizeof(fc->condition) - 1);
        }
        
        cJSON* windspeed = cJSON_GetArrayItem(windspeed_array, i);
        if (windspeed && cJSON_IsNumber(windspeed)) {
            fc->wind_speed = (float)windspeed->valuedouble;
        }
    }
    
    return ETHERVOX_SUCCESS;
}

// ============================================================================
// Main Weather Forecast API
// ============================================================================

ethervox_result_t ethervox_weather_get_forecast(
    const ethervox_weather_request_t* request,
    ethervox_weather_response_t** response_out,
    char** error_message_out
) {
    if (!g_initialized) {
        if (error_message_out) {
            *error_message_out = strdup("Weather tools not initialized");
        }
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    if (!request || !response_out) {
        if (error_message_out) {
            *error_message_out = strdup("Invalid parameters");
        }
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Validate coordinates
    if (request->latitude < -90.0 || request->latitude > 90.0 ||
        request->longitude < -180.0 || request->longitude > 180.0) {
        if (error_message_out) {
            *error_message_out = strdup("Invalid coordinates (latitude must be -90 to 90, longitude -180 to 180)");
        }
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check cache
    if (request->use_cache && g_config.enable_cache) {
        ethervox_weather_response_t* cached = weather_cache_get(request);
        if (cached) {
            *response_out = cached;
            ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                        "Cache hit for (%.4f, %.4f)", request->latitude, request->longitude);
            return ETHERVOX_SUCCESS;
        }
    }
    
    // Build forecast URL based on type
    char url[2048];
    const char* base = OPENMETEO_FORECAST_URL;
    
    switch (request->forecast_type) {
        case ETHERVOX_WEATHER_CURRENT:
            snprintf(url, sizeof(url),
                     "%s?latitude=%.4f&longitude=%.4f&current_weather=true",
                     base, request->latitude, request->longitude);
            break;
            
        case ETHERVOX_WEATHER_HOURLY:
            snprintf(url, sizeof(url),
                     "%s?latitude=%.4f&longitude=%.4f"
                     "&hourly=temperature_2m,apparent_temperature,relative_humidity_2m,"
                     "precipitation,precipitation_probability,weathercode,"
                     "wind_speed_10m,wind_direction_10m"
                     "&forecast_days=1",
                     base, request->latitude, request->longitude);
            break;
            
        case ETHERVOX_WEATHER_DAILY:
            snprintf(url, sizeof(url),
                     "%s?latitude=%.4f&longitude=%.4f"
                     "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,"
                     "precipitation_probability_max,weathercode,wind_speed_10m_max"
                     "&forecast_days=1",
                     base, request->latitude, request->longitude);
            break;
            
        case ETHERVOX_WEATHER_7DAY:
            snprintf(url, sizeof(url),
                     "%s?latitude=%.4f&longitude=%.4f"
                     "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,"
                     "precipitation_probability_max,weathercode,wind_speed_10m_max"
                     "&forecast_days=7",
                     base, request->latitude, request->longitude);
            break;
            
        default:
            if (error_message_out) {
                *error_message_out = strdup("Invalid forecast type");
            }
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Make HTTP request
    char* response_json = NULL;
    char* error_msg = NULL;
    ethervox_result_t result = http_get_request(url, &response_json, &error_msg);
    
    if (result != ETHERVOX_SUCCESS) {
        if (error_message_out) {
            *error_message_out = error_msg;
        } else {
            free(error_msg);
        }
        return result;
    }
    
    // Parse JSON
    cJSON* root = cJSON_Parse(response_json);
    free(response_json);
    
    if (!root) {
        if (error_message_out) {
            *error_message_out = strdup("Failed to parse weather response");
        }
        return ETHERVOX_ERROR_API_RESPONSE;
    }
    
    // Create response structure
    ethervox_weather_response_t* response = calloc(1, sizeof(ethervox_weather_response_t));
    if (!response) {
        cJSON_Delete(root);
        if (error_message_out) {
            *error_message_out = strdup("Memory allocation failed");
        }
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Extract metadata
    cJSON* lat = cJSON_GetObjectItem(root, "latitude");
    cJSON* lon = cJSON_GetObjectItem(root, "longitude");
    cJSON* tz = cJSON_GetObjectItem(root, "timezone");
    
    if (lat && cJSON_IsNumber(lat)) response->latitude = lat->valuedouble;
    if (lon && cJSON_IsNumber(lon)) response->longitude = lon->valuedouble;
    if (tz && cJSON_IsString(tz)) {
        strncpy(response->timezone, tz->valuestring, sizeof(response->timezone) - 1);
    }
    
    strncpy(response->location_name, request->location_name, sizeof(response->location_name) - 1);
    response->generated_at = time(NULL);
    response->from_cache = false;
    
    // Parse forecast data based on type
    switch (request->forecast_type) {
        case ETHERVOX_WEATHER_CURRENT:
            result = parse_current_weather(root, response);
            break;
        case ETHERVOX_WEATHER_HOURLY:
            result = parse_hourly_forecast(root, response, 24);
            break;
        case ETHERVOX_WEATHER_DAILY:
            result = parse_daily_forecast(root, response, 1);
            break;
        case ETHERVOX_WEATHER_7DAY:
            result = parse_daily_forecast(root, response, 7);
            break;
    }
    
    cJSON_Delete(root);
    
    if (result != ETHERVOX_SUCCESS) {
        ethervox_weather_free_response(response);
        if (error_message_out) {
            *error_message_out = strdup("Failed to parse weather data");
        }
        return result;
    }
    
    // Cache the response
    if (request->use_cache && g_config.enable_cache) {
        weather_cache_put(request, response);
    }
    
    *response_out = response;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Fetched weather for (%.4f, %.4f): %d forecasts",
                request->latitude, request->longitude, (int)response->forecast_count);
    
    return ETHERVOX_SUCCESS;
}

// ============================================================================
// Module Lifecycle
// ============================================================================

ethervox_result_t ethervox_weather_init(const ethervox_weather_config_t* config) {
    if (g_initialized) {
        return ETHERVOX_SUCCESS;
    }
    
    // Use provided config or defaults
    if (config) {
        g_config = *config;
    }
    
#ifndef ETHERVOX_PLATFORM_ANDROID
    // Initialize curl globally (not needed on Android - using JNI HTTP)
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    
    g_initialized = true;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Weather tools initialized (cache: %s, TTL: %ds)",
                g_config.enable_cache ? "enabled" : "disabled",
                g_config.cache_ttl_sec);
    
    return ETHERVOX_SUCCESS;
}

void ethervox_weather_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    
    weather_cache_clear();
    
#ifndef ETHERVOX_PLATFORM_ANDROID
    curl_global_cleanup();
#endif
    
    g_initialized = false;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Weather tools cleaned up");
}

void ethervox_weather_free_response(ethervox_weather_response_t* response) {
    if (!response) return;
    
    if (response->forecasts) {
        free(response->forecasts);
    }
    free(response);
}

ethervox_result_t ethervox_weather_clear_cache(void) {
    if (!g_initialized) {
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    weather_cache_clear();
    return ETHERVOX_SUCCESS;
}

void ethervox_weather_get_cache_stats(
    uint64_t* hit_count_out,
    uint64_t* miss_count_out,
    size_t* size_out
) {
    if (g_initialized) {
        weather_cache_get_stats(hit_count_out, miss_count_out, size_out);
    } else {
        if (hit_count_out) *hit_count_out = 0;
        if (miss_count_out) *miss_count_out = 0;
        if (size_out) *size_out = 0;
    }
}
