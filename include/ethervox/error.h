// SPDX-License-Identifier: CC-BY-NC-SA-4.0
#ifndef ETHERVOX_ERROR_H
#define ETHERVOX_ERROR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard error codes for EthervoxAI
 * 
 * All functions should return ethervox_result_t for consistency.
 * Use ETHERVOX_SUCCESS (0) for success, negative values for errors.
 */
typedef enum {
    ETHERVOX_SUCCESS = 0,
    
    // General errors (-1 to -99)
    ETHERVOX_ERROR_GENERIC = -1,
    ETHERVOX_ERROR_NULL_POINTER = -2,
    ETHERVOX_ERROR_INVALID_ARGUMENT = -3,
    ETHERVOX_ERROR_OUT_OF_MEMORY = -4,
    ETHERVOX_ERROR_NOT_INITIALIZED = -5,
    ETHERVOX_ERROR_ALREADY_INITIALIZED = -6,
    ETHERVOX_ERROR_TIMEOUT = -7,
    ETHERVOX_ERROR_NOT_SUPPORTED = -8,
    ETHERVOX_ERROR_BUFFER_TOO_SMALL = -9,
    
    // Platform/HAL errors (-100 to -199)
    ETHERVOX_ERROR_PLATFORM_INIT = -100,
    ETHERVOX_ERROR_HAL_NOT_FOUND = -101,
    ETHERVOX_ERROR_GPIO_FAILURE = -102,
    ETHERVOX_ERROR_HARDWARE_NOT_AVAILABLE = -103,
    
    // Audio errors (-200 to -299)
    ETHERVOX_ERROR_AUDIO_INIT = -200,
    ETHERVOX_ERROR_AUDIO_DEVICE_NOT_FOUND = -201,
    ETHERVOX_ERROR_AUDIO_FORMAT_UNSUPPORTED = -202,
    ETHERVOX_ERROR_AUDIO_BUFFER_OVERFLOW = -203,
    ETHERVOX_ERROR_AUDIO_BUFFER_UNDERFLOW = -204,
    ETHERVOX_ERROR_AUDIO_DEVICE_BUSY = -205,
    
    // STT errors (-300 to -399)
    ETHERVOX_ERROR_STT_INIT = -300,
    ETHERVOX_ERROR_STT_MODEL_NOT_FOUND = -301,
    ETHERVOX_ERROR_STT_PROCESSING = -302,
    
    // Wake word errors (-400 to -499)
    ETHERVOX_ERROR_WAKEWORD_INIT = -400,
    ETHERVOX_ERROR_WAKEWORD_MODEL_NOT_FOUND = -401,
    
    // Plugin errors (-500 to -599)
    ETHERVOX_ERROR_PLUGIN_NOT_FOUND = -500,
    ETHERVOX_ERROR_PLUGIN_INIT = -501,
    ETHERVOX_ERROR_PLUGIN_EXECUTION = -502,
    ETHERVOX_ERROR_PLUGIN_MAX_REACHED = -503,
    
    // Network/API errors (-600 to -699)
    ETHERVOX_ERROR_NETWORK = -600,
    ETHERVOX_ERROR_API_CALL = -601,
    ETHERVOX_ERROR_API_RESPONSE = -602,
    ETHERVOX_ERROR_API_RATE_LIMIT = -603,
    
    // File I/O errors (-700 to -799)
    ETHERVOX_ERROR_FILE_NOT_FOUND = -700,
    ETHERVOX_ERROR_FILE_READ = -701,
    ETHERVOX_ERROR_FILE_WRITE = -702,
    ETHERVOX_ERROR_FILE_PERMISSION = -703,
    
} ethervox_result_t;

/**
 * @brief Error context for detailed diagnostics
 */
typedef struct {
    ethervox_result_t code;
    const char* message;
    const char* file;
    int line;
    const char* function;
    uint64_t timestamp_ms;
} ethervox_error_context_t;

/**
 * @brief Convert error code to human-readable string
 * @param result Error code
 * @return String description (never NULL)
 */
const char* ethervox_error_string(ethervox_result_t result);

/**
 * @brief Check if result is success
 * @param result Result code to check
 * @return true if successful, false otherwise
 */
static inline bool ethervox_is_success(ethervox_result_t result) {
    return result == ETHERVOX_SUCCESS;
}

/**
 * @brief Check if result is error
 * @param result Result code to check
 * @return true if error, false otherwise
 */
static inline bool ethervox_is_error(ethervox_result_t result) {
    return result != ETHERVOX_SUCCESS;
}

/**
 * @brief Set last error context (thread-local if available)
 * @param code Error code
 * @param message Error message (can be NULL)
 * @param file Source file (__FILE__)
 * @param line Line number (__LINE__)
 * @param function Function name (__func__)
 */
void ethervox_error_set_context(
    ethervox_result_t code,
    const char* message,
    const char* file,
    int line,
    const char* function
);

/**
 * @brief Get last error context
 * @return Pointer to error context (may be NULL if no error set)
 */
const ethervox_error_context_t* ethervox_error_get_context(void);

/**
 * @brief Clear error context
 */
void ethervox_error_clear(void);

/**
 * @brief Macro to set error with context
 */
#define ETHERVOX_ERROR_SET(code, msg) \
    ethervox_error_set_context(code, msg, __FILE__, __LINE__, __func__)

/**
 * @brief Macro to return error with context
 */
#define ETHERVOX_RETURN_ERROR(code, msg) \
    do { \
        ETHERVOX_ERROR_SET(code, msg); \
        return code; \
    } while(0)

/**
 * @brief Macro to check result and propagate error
 */
#define ETHERVOX_CHECK(expr) \
    do { \
        ethervox_result_t _result = (expr); \
        if (ethervox_is_error(_result)) { \
            return _result; \
        } \
    } while(0)

/**
 * @brief Macro to check pointer and return error if NULL
 */
#define ETHERVOX_CHECK_PTR(ptr) \
    do { \
        if ((ptr) == NULL) { \
            ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NULL_POINTER, #ptr " is NULL"); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_ERROR_H