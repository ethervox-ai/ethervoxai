// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
#ifndef ETHERVOX_LOGGING_H
#define ETHERVOX_LOGGING_H

#include "ethervox/error.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ETHERVOX_LOG_LEVEL_TRACE = 0,
    ETHERVOX_LOG_LEVEL_DEBUG = 1,
    ETHERVOX_LOG_LEVEL_INFO = 2,
    ETHERVOX_LOG_LEVEL_WARN = 3,
    ETHERVOX_LOG_LEVEL_ERROR = 4,
    ETHERVOX_LOG_LEVEL_FATAL = 5,
    ETHERVOX_LOG_LEVEL_OFF = 6
} ethervox_log_level_t;

/**
 * Subsystem identifiers for structured logging (C4.3)
 */
typedef enum {
    ETHERVOX_SUBSYSTEM_CORE = 0,        /**< Core initialization/lifecycle */
    ETHERVOX_SUBSYSTEM_MODEL_POOL = 1,  /**< Model pool management */
    ETHERVOX_SUBSYSTEM_LLM = 2,         /**< LLM generation */
    ETHERVOX_SUBSYSTEM_GRAMMAR = 3,     /**< Grammar/structured generation */
    ETHERVOX_SUBSYSTEM_SESSION = 4,     /**< Session forking/KV management */
    ETHERVOX_SUBSYSTEM_MEDIA = 5,       /**< Multimodal (vision/audio) */
    ETHERVOX_SUBSYSTEM_MEMORY = 6,      /**< Memory/eviction */
    ETHERVOX_SUBSYSTEM_PLATFORM = 7,    /**< Platform abstraction */
    ETHERVOX_SUBSYSTEM_UNKNOWN = 255
} ethervox_log_subsystem_t;

/**
 * Structured log field for key-value pairs (C4.3)
 */
typedef struct {
    const char* key;    /**< Field name (never NULL) */
    const char* value;  /**< Field value (never NULL, may be empty string) */
} ethervox_log_field_t;

/**
 * Structured log entry (C4.3)
 * 
 * Passed to callback for host processing. All pointers valid only for
 * the duration of the callback.
 */
typedef struct {
    ethervox_log_level_t level;         /**< Log level */
    ethervox_log_subsystem_t subsystem; /**< Originating subsystem */
    const char* file;                   /**< Source file (basename only) */
    int line;                           /**< Line number */
    const char* func;                   /**< Function name */
    const char* message;                /**< Log message */
    const ethervox_log_field_t* fields; /**< Optional key-value fields (NULL if count=0) */
    uint32_t field_count;               /**< Number of fields */
    uint64_t timestamp_ms;              /**< Unix timestamp in milliseconds */
} ethervox_log_entry_t;

/**
 * Structured log callback (C4.3)
 * 
 * Invoked for every log message if callback is set. Host can format/route
 * messages based on level/subsystem/fields. Must be thread-safe.
 * 
 * @param entry Log entry (valid only for callback duration)
 * @param user_data User data passed to ethervox_log_set_callback
 */
typedef void (*ethervox_log_callback_t)(const ethervox_log_entry_t* entry, void* user_data);

/**
 * @brief Set global log level
 * @param level Minimum log level to output
 */
void ethervox_log_set_level(ethervox_log_level_t level);

/**
 * @brief Get current log level
 * @return Current log level
 */
ethervox_log_level_t ethervox_log_get_level(void);

/**
 * @brief Set structured log callback (C4.3)
 * 
 * Registers a callback to receive all log messages in structured form.
 * If set, callback receives every log entry regardless of log level
 * (level filtering is callback's responsibility).
 * 
 * Thread-safety: Callback must be thread-safe. Multiple threads may
 * call it concurrently.
 * 
 * @param callback Log callback (NULL to disable structured logging)
 * @param user_data User data passed to callback
 */
void ethervox_log_set_callback(ethervox_log_callback_t callback, void* user_data);

/**
 * @brief Log formatted message
 * @param level Log level
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param fmt Format string (printf-style)
 */
void ethervox_log(ethervox_log_level_t level, const char* file, int line, 
                  const char* func, const char* fmt, ...);

/**
 * @brief Log formatted message with subsystem (C4.3)
 * @param level Log level
 * @param subsystem Originating subsystem
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param fmt Format string (printf-style)
 */
void ethervox_log_ex(ethervox_log_level_t level, ethervox_log_subsystem_t subsystem,
                     const char* file, int line, const char* func, const char* fmt, ...);

/**
 * @brief Log with structured fields (C4.3)
 * @param level Log level
 * @param subsystem Originating subsystem
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param fields Array of key-value fields (NULL if field_count=0)
 * @param field_count Number of fields
 * @param fmt Format string (printf-style)
 */
void ethervox_log_fields(ethervox_log_level_t level, ethervox_log_subsystem_t subsystem,
                         const char* file, int line, const char* func,
                         const ethervox_log_field_t* fields, uint32_t field_count,
                         const char* fmt, ...);

/**
 * @brief Log error with context
 * @param ctx Error context to log
 */
void ethervox_log_error_context(const ethervox_error_context_t* ctx);

// Convenience macros
#define ETHERVOX_LOG_TRACE(...) \
    ethervox_log(ETHERVOX_LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
    
#define ETHERVOX_LOG_DEBUG(...) \
    ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
    
#define ETHERVOX_LOG_INFO(...) \
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
    
#define ETHERVOX_LOG_WARN(...) \
    ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
    
#define ETHERVOX_LOG_ERROR(...) \
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define ETHERVOX_LOG_FATAL(...) \
    ethervox_log(ETHERVOX_LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Macro to log and return error
 */
#define ETHERVOX_LOG_RETURN_ERROR(code, ...) \
    do { \
        ETHERVOX_LOG_ERROR(__VA_ARGS__); \
        ETHERVOX_RETURN_ERROR(code, NULL); \
    } while(0)

// ============================================================================
// Metrics API (C4.3)
// ============================================================================

/**
 * Performance and usage metrics snapshot (C4.3)
 * 
 * Provides point-in-time metrics for monitoring, profiling, and debugging.
 * All counters are cumulative since library initialization.
 */
typedef struct {
    // Model pool metrics
    uint32_t models_loaded_count;       /**< Currently loaded models */
    uint64_t total_model_memory_bytes;  /**< Total model memory in use */
    uint64_t memory_budget_bytes;       /**< Configured memory budget */
    uint32_t models_loaded_total;       /**< Total models loaded (lifetime) */
    uint32_t models_evicted_total;      /**< Total models evicted (lifetime) */
    uint64_t bytes_evicted_total;       /**< Total bytes freed by eviction */
    
    // Generation metrics
    uint64_t generations_total;         /**< Total generation requests */
    uint64_t generations_succeeded;     /**< Successful generations */
    uint64_t generations_failed;        /**< Failed generations */
    uint64_t tokens_generated_total;    /**< Total tokens generated */
    uint64_t generation_time_ms_total;  /**< Cumulative generation time */
    
    // Session metrics (C3.2)
    uint32_t sessions_active;           /**< Active sessions */
    uint32_t sessions_forked_total;     /**< Total forks performed */
    uint64_t kv_cache_hits;             /**< KV cache hits */
    uint64_t kv_cache_misses;           /**< KV cache misses */
    
    // Grammar/structured metrics (C3.3)
    uint64_t structured_gens_total;     /**< Structured generations */
    float avg_confidence;               /**< Average confidence score */
    
    // Error metrics
    uint32_t errors_total;              /**< Total errors logged */
    uint32_t errors_oom;                /**< Out-of-memory errors */
    uint32_t errors_timeout;            /**< Timeout errors */
} ethervox_metrics_t;

/**
 * @brief Get current metrics snapshot (C4.3)
 * 
 * Returns a point-in-time snapshot of all metrics. Safe to call from any thread.
 * 
 * @param metrics Receives metrics snapshot
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_metrics_snapshot(ethervox_metrics_t* metrics);

/**
 * @brief Reset metrics counters (C4.3)
 * 
 * Resets all cumulative counters to zero. Current state (loaded models,
 * active sessions) is preserved.
 * 
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_metrics_reset(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_LOGGING_H