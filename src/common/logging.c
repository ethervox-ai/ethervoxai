// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
#include "ethervox/logging.h"
#include "ethervox/platform_thread.h"
#include "ethervox/platform_time.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#define ethervox_isatty _isatty
#define ethervox_fileno _fileno
#else
#include <unistd.h>
#define ethervox_isatty isatty
#define ethervox_fileno fileno
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

// Global debug flag (referenced by config.h macros) - default enabled
int g_ethervox_debug_enabled = 1;

static ethervox_log_level_t g_log_level = ETHERVOX_LOG_LEVEL_INFO;

// Structured logging callback (C4.3)
static ethervox_log_callback_t g_log_callback = NULL;
static void* g_log_callback_user_data = NULL;
static ethervox_mutex_t g_callback_mutex;
static bool g_callback_mutex_init = false;

// Metrics (C4.3)
static ethervox_metrics_t g_metrics = {0};
static ethervox_mutex_t g_metrics_mutex;
static bool g_metrics_mutex_init = false;

// Forward declarations
static void invoke_callback(const ethervox_log_entry_t* entry);

void ethervox_log_set_level(ethervox_log_level_t level) {
    g_log_level = level;
}

ethervox_log_level_t ethervox_log_get_level(void) {
    return g_log_level;
}

// ANSI color codes
#define COLOR_RESET    "\033[0m"
#define COLOR_TRACE    "\033[36m"   // Cyan
#define COLOR_DEBUG    "\033[34m"   // Blue
#define COLOR_INFO     "\033[32m"   // Green
#define COLOR_WARN     "\033[33m"   // Yellow
#define COLOR_ERROR    "\033[31m"   // Red
#define COLOR_FATAL    "\033[35m"   // Magenta

static const char* log_level_string(ethervox_log_level_t level) {
    switch (level) {
        case ETHERVOX_LOG_LEVEL_TRACE: return "TRACE";
        case ETHERVOX_LOG_LEVEL_DEBUG: return "DEBUG";
        case ETHERVOX_LOG_LEVEL_INFO:  return "INFO ";
        case ETHERVOX_LOG_LEVEL_WARN:  return "WARN ";
        case ETHERVOX_LOG_LEVEL_ERROR: return "ERROR";
        case ETHERVOX_LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

static const char* log_level_color(ethervox_log_level_t level) {
    switch (level) {
        case ETHERVOX_LOG_LEVEL_TRACE: return COLOR_TRACE;
        case ETHERVOX_LOG_LEVEL_DEBUG: return COLOR_DEBUG;
        case ETHERVOX_LOG_LEVEL_INFO:  return COLOR_INFO;
        case ETHERVOX_LOG_LEVEL_WARN:  return COLOR_WARN;
        case ETHERVOX_LOG_LEVEL_ERROR: return COLOR_ERROR;
        case ETHERVOX_LOG_LEVEL_FATAL: return COLOR_FATAL;
        default: return COLOR_RESET;
    }
}

static const char* extract_filename(const char* path) {
    const char* filename = strrchr(path, '/');
    if (filename) {
        return filename + 1;
    }
    filename = strrchr(path, '\\');
    if (filename) {
        return filename + 1;
    }
    return path;
}

void ethervox_log(ethervox_log_level_t level, const char* file, int line, 
                  const char* func, const char* fmt, ...) {
    if (level < g_log_level) {
        return;
    }
    
    // Format message for both console and callback
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    // Get timestamp
    time_t now = time(NULL);
    struct tm tm_info;
    char timestamp[20];
#ifdef _WIN32
    localtime_s(&tm_info, &now);  // Windows: different parameter order
#else
    localtime_r(&now, &tm_info);  // POSIX
#endif
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);
    
    // Get color for this level -- only when stderr is a real terminal, never when piped or
    // redirected into a log file / another process's captured output (callers like the
    // ethervoxai-workspace model pool pipe this through structured logging and never want raw
    // ANSI escapes embedded in it)
    static int stderr_is_tty = -1;
    if (stderr_is_tty == -1) {
        stderr_is_tty = ethervox_isatty(ethervox_fileno(stderr)) ? 1 : 0;
    }
    const char* color = stderr_is_tty ? log_level_color(level) : "";
    const char* color_reset = stderr_is_tty ? COLOR_RESET : "";
    
#ifdef __ANDROID__
    // On Android, use Android logging system
    android_LogPriority priority;
    switch (level) {
        case ETHERVOX_LOG_LEVEL_TRACE:
        case ETHERVOX_LOG_LEVEL_DEBUG:
            priority = ANDROID_LOG_DEBUG;
            break;
        case ETHERVOX_LOG_LEVEL_INFO:
            priority = ANDROID_LOG_INFO;
            break;
        case ETHERVOX_LOG_LEVEL_WARN:
            priority = ANDROID_LOG_WARN;
            break;
        case ETHERVOX_LOG_LEVEL_ERROR:
            priority = ANDROID_LOG_ERROR;
            break;
        case ETHERVOX_LOG_LEVEL_FATAL:
            priority = ANDROID_LOG_FATAL;
            break;
        default:
            priority = ANDROID_LOG_INFO;
    }
    
    // Log with Android logger, including file and function info
    __android_log_print(priority, "EthervoxCore", "[%s:%d %s] %s",
                       extract_filename(file), line, func, message);
#else
    // On desktop platforms, use colored stderr output
    // Print log prefix with color
    fprintf(stderr, "%s[%s] [%s] [%s:%d %s] ", 
            color,
            timestamp,
            log_level_string(level),
            extract_filename(file),
            line,
            func);
    
    // Print message in color
    fprintf(stderr, "%s%s\n", message, color_reset);
    fflush(stderr);
#endif
    
    // Invoke structured callback if set (C4.3)
    ethervox_log_entry_t entry = {
        .level = level,
        .subsystem = ETHERVOX_SUBSYSTEM_CORE,  // Default subsystem
        .file = extract_filename(file),
        .line = line,
        .func = func,
        .message = message,
        .fields = NULL,
        .field_count = 0,
        .timestamp_ms = ethervox_time_now_ms()
    };
    invoke_callback(&entry);
}

void ethervox_log_error_context(const ethervox_error_context_t* ctx) {
    if (!ctx) {
        return;
    }
    
#ifdef __ANDROID__
    // On Android, log error context via Android logger
    __android_log_print(ANDROID_LOG_ERROR, "EthervoxCore", "[ERROR CONTEXT]");
    __android_log_print(ANDROID_LOG_ERROR, "EthervoxCore", "  Code: %d (%s)", 
                       ctx->code, ethervox_error_string(ctx->code));
    if (ctx->message) {
        __android_log_print(ANDROID_LOG_ERROR, "EthervoxCore", "  Message: %s", ctx->message);
    }
    __android_log_print(ANDROID_LOG_ERROR, "EthervoxCore", "  Location: %s:%d in %s()",
                       extract_filename(ctx->file), ctx->line, ctx->function);
    __android_log_print(ANDROID_LOG_ERROR, "EthervoxCore", "  Timestamp: %llu ms",
                       (unsigned long long)ctx->timestamp_ms);
#else
    // On desktop, use colored stderr output
    fprintf(stderr, "%s[ERROR CONTEXT]\n", COLOR_ERROR);
    fprintf(stderr, "  Code: %d (%s)\n", ctx->code, ethervox_error_string(ctx->code));
    if (ctx->message) {
        fprintf(stderr, "  Message: %s\n", ctx->message);
    }
    fprintf(stderr, "  Location: %s:%d in %s()\n", 
            extract_filename(ctx->file), ctx->line, ctx->function);
    fprintf(stderr, "  Timestamp: %llu ms%s\n", (unsigned long long)ctx->timestamp_ms, COLOR_RESET);
    fflush(stderr);
#endif
}

// ============================================================================
// Structured logging implementation (C4.3)
// ============================================================================

void ethervox_log_set_callback(ethervox_log_callback_t callback, void* user_data) {
    if (!g_callback_mutex_init) {
        ethervox_mutex_init(&g_callback_mutex);
        g_callback_mutex_init = true;
    }
    
    ethervox_mutex_lock(&g_callback_mutex);
    g_log_callback = callback;
    g_log_callback_user_data = user_data;
    ethervox_mutex_unlock(&g_callback_mutex);
}

/**
 * Internal: invoke callback if set
 */
static void invoke_callback(const ethervox_log_entry_t* entry) {
    if (!g_callback_mutex_init) {
        return;
    }
    
    ethervox_mutex_lock(&g_callback_mutex);
    if (g_log_callback) {
        g_log_callback(entry, g_log_callback_user_data);
    }
    ethervox_mutex_unlock(&g_callback_mutex);
}

void ethervox_log_ex(ethervox_log_level_t level, ethervox_log_subsystem_t subsystem,
                     const char* file, int line, const char* func, const char* fmt, ...) {
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    // Call existing log function for console output
    ethervox_log(level, file, line, func, "%s", message);
    
    // Invoke structured callback if set
    ethervox_log_entry_t entry = {
        .level = level,
        .subsystem = subsystem,
        .file = extract_filename(file),
        .line = line,
        .func = func,
        .message = message,
        .fields = NULL,
        .field_count = 0,
        .timestamp_ms = ethervox_time_now_ms()
    };
    invoke_callback(&entry);
}

void ethervox_log_fields(ethervox_log_level_t level, ethervox_log_subsystem_t subsystem,
                         const char* file, int line, const char* func,
                         const ethervox_log_field_t* fields, uint32_t field_count,
                         const char* fmt, ...) {
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    // Call existing log function for console output
    ethervox_log(level, file, line, func, "%s", message);
    
    // Invoke structured callback with fields
    ethervox_log_entry_t entry = {
        .level = level,
        .subsystem = subsystem,
        .file = extract_filename(file),
        .line = line,
        .func = func,
        .message = message,
        .fields = fields,
        .field_count = field_count,
        .timestamp_ms = ethervox_time_now_ms()
    };
    invoke_callback(&entry);
}

// ============================================================================
// Metrics implementation (C4.3)
// ============================================================================

ethervox_result_t ethervox_metrics_snapshot(ethervox_metrics_t* metrics) {
    if (!metrics) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (!g_metrics_mutex_init) {
        ethervox_mutex_init(&g_metrics_mutex);
        g_metrics_mutex_init = true;
    }
    
    ethervox_mutex_lock(&g_metrics_mutex);
    memcpy(metrics, &g_metrics, sizeof(ethervox_metrics_t));
    ethervox_mutex_unlock(&g_metrics_mutex);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_metrics_reset(void) {
    if (!g_metrics_mutex_init) {
        ethervox_mutex_init(&g_metrics_mutex);
        g_metrics_mutex_init = true;
    }
    
    ethervox_mutex_lock(&g_metrics_mutex);
    // Reset cumulative counters, preserve current state
    g_metrics.models_loaded_total = 0;
    g_metrics.models_evicted_total = 0;
    g_metrics.bytes_evicted_total = 0;
    g_metrics.generations_total = 0;
    g_metrics.generations_succeeded = 0;
    g_metrics.generations_failed = 0;
    g_metrics.tokens_generated_total = 0;
    g_metrics.generation_time_ms_total = 0;
    g_metrics.sessions_forked_total = 0;
    g_metrics.kv_cache_hits = 0;
    g_metrics.kv_cache_misses = 0;
    g_metrics.structured_gens_total = 0;
    g_metrics.avg_confidence = 0.0f;
    g_metrics.errors_total = 0;
    g_metrics.errors_oom = 0;
    g_metrics.errors_timeout = 0;
    ethervox_mutex_unlock(&g_metrics_mutex);
    
    return ETHERVOX_SUCCESS;
}

// Internal: update metrics (called by various subsystems)
void ethervox_metrics_record_generation(bool success, uint32_t tokens, uint64_t time_ms) {
    if (!g_metrics_mutex_init) {
        ethervox_mutex_init(&g_metrics_mutex);
        g_metrics_mutex_init = true;
    }
    
    ethervox_mutex_lock(&g_metrics_mutex);
    g_metrics.generations_total++;
    if (success) {
        g_metrics.generations_succeeded++;
        g_metrics.tokens_generated_total += tokens;
        g_metrics.generation_time_ms_total += time_ms;
    } else {
        g_metrics.generations_failed++;
    }
    ethervox_mutex_unlock(&g_metrics_mutex);
}

void ethervox_metrics_record_structured_gen(float confidence) {
    if (!g_metrics_mutex_init) {
        ethervox_mutex_init(&g_metrics_mutex);
        g_metrics_mutex_init = true;
    }
    
    ethervox_mutex_lock(&g_metrics_mutex);
    g_metrics.structured_gens_total++;
    // Running average
    float n = (float)g_metrics.structured_gens_total;
    g_metrics.avg_confidence = (g_metrics.avg_confidence * (n - 1.0f) + confidence) / n;
    ethervox_mutex_unlock(&g_metrics_mutex);
}

void ethervox_metrics_record_error(ethervox_result_t code) {
    if (!g_metrics_mutex_init) {
        ethervox_mutex_init(&g_metrics_mutex);
        g_metrics_mutex_init = true;
    }
    
    ethervox_mutex_lock(&g_metrics_mutex);
    g_metrics.errors_total++;
    if (code == ETHERVOX_ERROR_OUT_OF_MEMORY) {
        g_metrics.errors_oom++;
    } else if (code == ETHERVOX_ERROR_TIMEOUT) {
        g_metrics.errors_timeout++;
    }
    ethervox_mutex_unlock(&g_metrics_mutex);
}