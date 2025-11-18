// SPDX-License-Identifier: CC-BY-NC-SA-4.0
#ifndef ETHERVOX_LOGGING_H
#define ETHERVOX_LOGGING_H

#include "ethervox/error.h"
#include <stdio.h>
#include <stdarg.h>

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

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_LOGGING_H