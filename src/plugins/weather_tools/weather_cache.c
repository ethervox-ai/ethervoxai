/**
 * @file weather_cache.c
 * @brief Weather response caching implementation
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#include "ethervox/weather_tools.h"
#include "ethervox/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// Cache Entry Structure
// ============================================================================

typedef struct weather_cache_entry {
    char key[128];                           // Cache key
    time_t expires_at;                       // Expiration timestamp
    ethervox_weather_response_t* response;   // Cached response
    struct weather_cache_entry* next;        // LRU linked list
    struct weather_cache_entry* prev;
} weather_cache_entry_t;

// ============================================================================
// Cache State
// ============================================================================

static weather_cache_entry_t* g_cache_head = NULL;
static weather_cache_entry_t* g_cache_tail = NULL;
static size_t g_cache_size = 0;
static size_t g_max_cache_entries = 100;
static int g_cache_ttl_sec = ETHERVOX_WEATHER_CACHE_TTL_SEC;

// Statistics
static uint64_t g_cache_hits = 0;
static uint64_t g_cache_misses = 0;

// ============================================================================
// Cache Key Generation
// ============================================================================

/**
 * @brief Generate cache key from request
 */
static void weather_cache_make_key(
    const ethervox_weather_request_t* request,
    char* key_out,
    size_t key_size
) {
    // Format: "lat_lon_type_days"
    snprintf(key_out, key_size, "%.4f_%.4f_%d_%d",
             request->latitude,
             request->longitude,
             request->forecast_type,
             request->days_ahead);
}

// ============================================================================
// LRU List Management
// ============================================================================

/**
 * @brief Move entry to head of LRU list (most recently used)
 */
static void move_to_head(weather_cache_entry_t* entry) {
    if (entry == g_cache_head) {
        return; // Already at head
    }
    
    // Remove from current position
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == g_cache_tail) {
        g_cache_tail = entry->prev;
    }
    
    // Insert at head
    entry->prev = NULL;
    entry->next = g_cache_head;
    if (g_cache_head) {
        g_cache_head->prev = entry;
    }
    g_cache_head = entry;
    
    if (!g_cache_tail) {
        g_cache_tail = entry;
    }
}

/**
 * @brief Remove entry from LRU list
 */
static void remove_from_list(weather_cache_entry_t* entry) {
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        g_cache_head = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        g_cache_tail = entry->prev;
    }
    
    g_cache_size--;
}

/**
 * @brief Free cache entry
 */
static void free_cache_entry(weather_cache_entry_t* entry) {
    if (!entry) return;
    
    if (entry->response) {
        if (entry->response->forecasts) {
            free(entry->response->forecasts);
        }
        free(entry->response);
    }
    
    free(entry);
}

/**
 * @brief Evict least recently used entry
 */
static void evict_lru(void) {
    if (!g_cache_tail) {
        return;
    }
    
    weather_cache_entry_t* lru = g_cache_tail;
    remove_from_list(lru);
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Evicted LRU cache entry: %s", lru->key);
    
    free_cache_entry(lru);
}

// ============================================================================
// Cache Operations
// ============================================================================

/**
 * @brief Deep copy weather response
 */
static ethervox_weather_response_t* clone_response(const ethervox_weather_response_t* source) {
    if (!source) return NULL;
    
    ethervox_weather_response_t* copy = malloc(sizeof(ethervox_weather_response_t));
    if (!copy) return NULL;
    
    memcpy(copy, source, sizeof(ethervox_weather_response_t));
    
    if (source->forecast_count > 0 && source->forecasts) {
        size_t forecasts_size = source->forecast_count * sizeof(ethervox_weather_forecast_t);
        copy->forecasts = malloc(forecasts_size);
        if (!copy->forecasts) {
            free(copy);
            return NULL;
        }
        memcpy(copy->forecasts, source->forecasts, forecasts_size);
    } else {
        copy->forecasts = NULL;
    }
    
    // Mark as from cache
    copy->from_cache = true;
    
    return copy;
}

/**
 * @brief Get cached response if valid
 */
ethervox_weather_response_t* weather_cache_get(const ethervox_weather_request_t* request) {
    char key[128];
    weather_cache_make_key(request, key, sizeof(key));
    
    time_t now = time(NULL);
    
    // Search cache
    weather_cache_entry_t* entry = g_cache_head;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // Found entry - check expiration
            if (now < entry->expires_at) {
                // Valid entry - move to head and return copy
                move_to_head(entry);
                g_cache_hits++;
                
                ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                            "Cache hit: %s (expires in %lds)",
                            key, (long)(entry->expires_at - now));
                
                return clone_response(entry->response);
            } else {
                // Expired entry - remove it
                ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                            "Cache expired: %s", key);
                
                remove_from_list(entry);
                free_cache_entry(entry);
                break;
            }
        }
        entry = entry->next;
    }
    
    g_cache_misses++;
    return NULL;
}

/**
 * @brief Store response in cache
 */
void weather_cache_put(
    const ethervox_weather_request_t* request,
    const ethervox_weather_response_t* response
) {
    if (!request || !response) return;
    
    char key[128];
    weather_cache_make_key(request, key, sizeof(key));
    
    // Check if entry already exists
    weather_cache_entry_t* existing = g_cache_head;
    while (existing) {
        if (strcmp(existing->key, key) == 0) {
            // Update existing entry
            if (existing->response) {
                if (existing->response->forecasts) {
                    free(existing->response->forecasts);
                }
                free(existing->response);
            }
            
            existing->response = clone_response(response);
            existing->expires_at = time(NULL) + g_cache_ttl_sec;
            move_to_head(existing);
            
            ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                        "Updated cache entry: %s", key);
            return;
        }
        existing = existing->next;
    }
    
    // Create new entry
    weather_cache_entry_t* entry = calloc(1, sizeof(weather_cache_entry_t));
    if (!entry) return;
    
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    entry->response = clone_response(response);
    entry->expires_at = time(NULL) + g_cache_ttl_sec;
    
    if (!entry->response) {
        free(entry);
        return;
    }
    
    // Evict LRU if cache is full
    if (g_cache_size >= g_max_cache_entries) {
        evict_lru();
    }
    
    // Insert at head
    entry->next = g_cache_head;
    entry->prev = NULL;
    
    if (g_cache_head) {
        g_cache_head->prev = entry;
    } else {
        g_cache_tail = entry;
    }
    
    g_cache_head = entry;
    g_cache_size++;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__,
                "Cached response: %s (size: %zu/%zu)",
                key, g_cache_size, g_max_cache_entries);
}

/**
 * @brief Clear all cache entries
 */
void weather_cache_clear(void) {
    weather_cache_entry_t* entry = g_cache_head;
    
    while (entry) {
        weather_cache_entry_t* next = entry->next;
        free_cache_entry(entry);
        entry = next;
    }
    
    g_cache_head = NULL;
    g_cache_tail = NULL;
    g_cache_size = 0;
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Cache cleared");
}

/**
 * @brief Get cache statistics
 */
void weather_cache_get_stats(
    uint64_t* hit_count_out,
    uint64_t* miss_count_out,
    size_t* size_out
) {
    if (hit_count_out) *hit_count_out = g_cache_hits;
    if (miss_count_out) *miss_count_out = g_cache_misses;
    if (size_out) *size_out = g_cache_size;
}

/**
 * @brief Set cache configuration
 */
void weather_cache_set_config(int ttl_sec, size_t max_entries) {
    g_cache_ttl_sec = ttl_sec;
    g_max_cache_entries = max_entries;
    
    // Evict entries if new max is smaller
    while (g_cache_size > g_max_cache_entries) {
        evict_lru();
    }
}
