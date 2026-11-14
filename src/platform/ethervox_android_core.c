/**
 * @file ethervox_android_core.c
 * @brief Android JNI wrapper for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI. Proprietary and confidential. See LICENSE.
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 *
 * Granite Speech integration: Mode 1 (voice conversation), Mode 2
 * (transcription), and Mode 4 (voice-to-text) each go through their own
 * dedicated JNI entry points, all backed by Granite Speech ASR
 * (see ethervox/stt.h) instead of Whisper/SpeechRecognizer:
 *   - Mode 1/4: voiceConversationStart/Stop/Interrupt and
 *     voiceQueryStart/Stop (see the "Voice Conversation" section below) both
 *     wrap ethervox_conversation_init/start (src/dialogue/voice_conversation.c),
 *     the single, platform-agnostic Mode 1/4 orchestrator - Android supplies
 *     platform-native TTS via the on_speak_request callback and resolves the
 *     Granite Speech Plus model path against the app's files dir (ASR-only
 *     prompt, same weights Mode 2 uses in SAA mode); the underlying state
 *     machine, VAD, and barge-in logic are unchanged from desktop.
 *   - Mode 2: startVoiceTranscription/stopVoiceTranscription (see "Voice
 *     Tools / Transcription" below) wrap ethervox_voice_tools_init/
 *     start_listen/stop_listen, which initialize Granite Speech Plus (SAA)
 *     internally.
 * "Which ASR backend is active" is therefore decided in exactly one place
 * per mode, not split between a Kotlin-only path and a JNI-only path.
 */

#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "ethervox/audio.h"
#include "ethervox/llm.h"
#include "ethervox/stt.h"
#include "ethervox/wake_word.h"
#include "ethervox/paths.h"
#include "ethervox/logging.h"
#include "ethervox/cancel_token.h"  // N6.4: Cancellation token for generation
#include "ethervox/kv_cache_persistence.h"  // N6.5: C1.6 cache API
#include "ethervox/host_tools.h"  // N5.2: Host-registered tools (C2.2)
#include "ethervox/model_pool.h"  // N6.2a: Model pool for memory management
// NOTE: dialogue.h removed - using direct governor/registry architecture
#include "ethervox/compute_tools.h"
#include "ethervox/config.h"
#include "ethervox/conversation.h"
#include "ethervox/conversation_tools.h"
#include "ethervox/device_profile.h"
#include "ethervox/get_tool_info.h"
#include "ethervox/governor.h"
#include "ethervox/integration_tests.h"
#include "ethervox/llm_tool_tests.h"
#include "ethervox/memory_tools.h"
#include "ethervox/platform.h"
#include "ethervox/startup_prompt_tools.h"
#include "ethervox/system_info_tools.h"
#include "ethervox/timer_tools.h"
#include "ethervox/tool_manifest.h"
#include "ethervox/kv_cache_persistence.h"  // KV cache save/load for fast startup
#include "ethervox/conversation_summary.h"  // Conversation summarization for KV cache
#include "ethervox/tool_prompt_optimizer.h"
#include "ethervox/unit_conversion.h"
#include "ethervox/voice_tools.h"
#include "ethervox/weather_tools.h"

#if ETHERVOX_WITH_LLAMA && LLAMA_CPP_AVAILABLE
#include "llama.h"
#endif

#define LOGI(...) ETHERVOX_LOGI(__VA_ARGS__)
#define LOGE(...) ETHERVOX_LOGE(__VA_ARGS__)
#define LOGW(...) ETHERVOX_LOGE(__VA_ARGS__)

// Global debug mode flag (defined in logging.c, referenced here)
extern int g_ethervox_debug_enabled;

// Global log callback for sending C logs to Java/Kotlin debug window
ethervox_log_callback_t g_ethervox_log_callback = NULL;

// Logging helper that sends to both logcat and callback
void ethervox_log_with_callback(int level, const char* tag, const char* fmt, ...) {
  if (!g_ethervox_debug_enabled)
    return;

  // Guard against NULL format string
  if (!fmt) {
    fmt = "(null format string)";
  }
  if (!tag) {
    tag = "EthervoxCore";
  }

  char buffer[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  // Send to Android logcat
  __android_log_print(level, tag, "%s", buffer);

  // Note: C4.3 structured logging - callback invoked by core logging system,
  // not called directly here. The java_log_callback_wrapper will be invoked
  // automatically for all log entries if registered.
}

// JNI callback storage for log forwarding
JavaVM* g_jvm = NULL;  // Exported for use by plugins (e.g., weather_http_android.c)
static jobject g_log_callback_obj = NULL;
static jmethodID g_log_callback_method = NULL;

// Global runtime instances (managed from Java layer)
static ethervox_audio_runtime_t* g_audio_runtime = NULL;
static ethervox_wake_runtime_t* g_wake_runtime = NULL;
static ethervox_stt_runtime_t* g_stt_runtime = NULL;
static ethervox_platform_t* g_platform = NULL;

// Direct Governor and Registry (mirrors desktop main.c architecture)
// REFACTORED 2025-12-09: Removed dialogue_engine wrapper to align with desktop
// Android now uses same initialization path: governor + tool_registry directly
static ethervox_governor_t* g_governor = NULL;
static ethervox_tool_registry_t* g_registry = NULL;
static tool_manifest_registry_t* g_manifest_registry = NULL;
static ethervox_memory_store_t* g_memory_store = NULL;

// Model pool for memory-managed model loading (N6.2a)
static ethervox_model_pool_t* g_model_pool = NULL;
static ethervox_model_handle_t* g_governor_handle = NULL;  // Handle for main governor model
static ethervox_model_handle_t* g_speech_handle = NULL;     // Handle for speech model (when loaded)

// Cancellation token for generation (N6.4)
static ethervox_cancel_token_t* g_cancel_token = NULL;

// Android-specific files directory (deprecated - migrate to g_android_paths)
static char g_android_files_dir[512] = {0};

// Android paths configuration (N6.1)
static char g_paths_buffer[2048] = {0};  // Buffer for path strings
static ethervox_paths_t g_android_paths = {0};  // Global paths struct
static bool g_paths_initialized = false;

// Governor runtime configuration (tunable from Java settings)
typedef struct {
  int batch_size;            // Main batch size for conversation (default: adaptive)
  int startup_batch_size;    // Startup batch size for faster TTFT (default: 128)
  int context_size;          // Context window size (default: 8192)
  float temperature;         // Sampling temperature (default: 0.3)
  int threads;               // CPU threads (default: adaptive)
  int max_tokens_per_response;  // Max tokens to generate per response (default: 2048)
} governor_runtime_config_t;

static governor_runtime_config_t g_governor_config = {
  .batch_size = ETHERVOX_GOVERNOR_BATCH_SIZE,
  .startup_batch_size = ETHERVOX_GOVERNOR_STARTUP_BATCH_SIZE,
  .context_size = ETHERVOX_GOVERNOR_CONTEXT_SIZE,
  .temperature = ETHERVOX_GOVERNOR_TEMPERATURE,
  .threads = ETHERVOX_GOVERNOR_THREADS,
  .max_tokens_per_response = 192  // Conservative limit for voice assistant responses (was 2048)
};

// Getter function for Android files directory (used by manifest system)
const char* ethervox_android_get_files_dir(void) {
  return g_android_files_dir;
}

// Getter function for startup batch size (used by governor.c)
int ethervox_get_startup_batch_size(void) {
  return g_governor_config.startup_batch_size;
}

// ===========================================================================
// Utility Functions
// ===========================================================================

// Default startup prompt (defined here for JNI access, same as main.c)
static const char* DEFAULT_STARTUP_PROMPT = "Greet the user with a short creative greeting.";

// Validate and truncate UTF-8 string at incomplete multi-byte sequences
// This is needed because llama.cpp token streaming can split emoji bytes
static bool validate_and_fix_utf8(const char* str, size_t* out_valid_len) {
  if (!str) {
    *out_valid_len = 0;
    return true;
  }

  size_t len = strlen(str);
  *out_valid_len = 0;
  bool is_valid = true;

  for (size_t i = 0; i < len;) {
    unsigned char c = (unsigned char)str[i];
    int expected_bytes = 0;

    if (c <= 0x7F) {
      // ASCII - 1 byte
      expected_bytes = 1;
    } else if ((c & 0xE0) == 0xC0) {
      // 2-byte sequence: 110xxxxx
      expected_bytes = 2;
    } else if ((c & 0xF0) == 0xE0) {
      // 3-byte sequence: 1110xxxx
      expected_bytes = 3;
    } else if ((c & 0xF8) == 0xF0) {
      // 4-byte sequence: 11110xxx (emojis!)
      expected_bytes = 4;
    } else {
      // Invalid start byte
      is_valid = false;
      break;
    }

    // Check if we have all the bytes for this character
    if (i + expected_bytes > len) {
      // Incomplete sequence at end - truncate here
      __android_log_print(ANDROID_LOG_DEBUG, "EthervoxJNI",
                          "Incomplete UTF-8 sequence at end: need %d bytes, have %zu",
                          expected_bytes, len - i);
      is_valid = false;
      break;
    }

    // Verify continuation bytes (should be 10xxxxxx)
    for (int j = 1; j < expected_bytes; j++) {
      if ((str[i + j] & 0xC0) != 0x80) {
        __android_log_print(ANDROID_LOG_WARN, "EthervoxJNI",
                            "Invalid continuation byte at position %zu", i + j);
        is_valid = false;
        break;
      }
    }

    if (!is_valid)
      break;

    i += expected_bytes;
    *out_valid_len = i;
  }

  return is_valid;
}

static jstring create_jstring(JNIEnv* env, const char* str) {
  if (!str) {
    return (*env)->NewStringUTF(env, "");
  }

  size_t valid_len = 0;
  bool is_valid = validate_and_fix_utf8(str, &valid_len);

  if (is_valid) {
    // String is completely valid UTF-8
    return (*env)->NewStringUTF(env, str);
  } else {
    // String has invalid or incomplete UTF-8
    // Create truncated string with only valid portion
    if (valid_len == 0) {
      __android_log_print(ANDROID_LOG_WARN, "EthervoxJNI",
                          "Completely invalid UTF-8 string, returning empty");
      return (*env)->NewStringUTF(env, "");
    }

    char* valid_str = malloc(valid_len + 1);
    if (!valid_str) {
      return (*env)->NewStringUTF(env, "");
    }

    memcpy(valid_str, str, valid_len);
    valid_str[valid_len] = '\0';

    __android_log_print(ANDROID_LOG_DEBUG, "EthervoxJNI",
                        "Truncated UTF-8 string from %zu to %zu bytes (incomplete sequence at end)",
                        strlen(str), valid_len);

    jstring result = (*env)->NewStringUTF(env, valid_str);
    free(valid_str);
    return result;
  }
}

// ===========================================================================
// Platform Initialization
// ===========================================================================

// Track last load error code for corruption detection
static int g_last_governor_load_error = 0;

// Context for model loading progress callback
typedef struct {
  JNIEnv* env;
  jobject callback_obj;
  jmethodID on_governor_progress_method;
} jni_load_context_t;

// Native callback wrapper for model loading progress
static bool native_load_progress_callback(const char* stage, float progress,
                                         const char* message, void* user_data) {
  jni_load_context_t* ctx = (jni_load_context_t*)user_data;
  if (!ctx || !ctx->env || !ctx->callback_obj || !ctx->on_governor_progress_method) {
    LOGW("native_load_progress_callback: No callback configured (ctx=%p, env=%p, obj=%p, method=%p)", 
         ctx, ctx ? ctx->env : NULL, ctx ? ctx->callback_obj : NULL, 
         ctx ? (void*)ctx->on_governor_progress_method : NULL);
    return true;  // No callback configured, continue loading
  }
  
  LOGI("native_load_progress_callback: stage='%s', progress=%.1f%%, message='%s'", 
       stage ? stage : "NULL", progress * 100.0f, message ? message : "NULL");
  
  // Call onGovernorProgress callback with stage as event type and message
  jstring j_stage = create_jstring(ctx->env, stage ? stage : "loading");
  jstring j_message = create_jstring(ctx->env, message ? message : "");
  (*ctx->env)->CallVoidMethod(ctx->env, ctx->callback_obj, ctx->on_governor_progress_method,
                              j_stage, j_message);
  (*ctx->env)->DeleteLocalRef(ctx->env, j_stage);
  (*ctx->env)->DeleteLocalRef(ctx->env, j_message);
  
  LOGI("native_load_progress_callback: Java callback invoked successfully");
  
  return true;  // Continue loading
}

JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_loadGovernorModel(
    JNIEnv* env, jobject thiz, jstring modelPath, jobject callback) {
  (void)thiz;

  if (!g_governor) {
    LOGE("Cannot load Governor model - governor not initialized");
    g_last_governor_load_error = -1;
    return JNI_FALSE;
  }

  const char* path = (*env)->GetStringUTFChars(env, modelPath, NULL);

  LOGI("[JNI] Loading Governor model from: %s", path);

  // === CRITICAL: Initialize Tool Manifest System BEFORE loading model ===
  // This ensures optimized prompts are available during system prompt generation
  if (!g_manifest_registry) {
    LOGI("[JNI] Initializing manifest registry BEFORE model load (for optimized prompts)");

    // Use the centralized manifest setup helper
    tool_manifest_registry_t* manifest = NULL;
    ethervox_result_t manifest_result =
        ethervox_governor_setup_manifest(g_governor, path, &manifest);

    if (ethervox_is_success(manifest_result) && manifest) {
      g_manifest_registry = manifest;
      
      // Attach manifest to governor so it can use optimized prompts
      ethervox_governor_set_manifest(g_governor, manifest);
      
      // Also set manifest on get_tool_info meta-tool
      ethervox_get_tool_info_set_manifest(manifest);
      
      LOGI("[JNI] Manifest initialized and attached to governor");
      LOGI("  Fallback level: %u", manifest->fallback_level);
      LOGI("  Tools available: %s", manifest->tools_available ? "YES" : "NO");
      LOGI("  Optimization loaded: %s", manifest->optimization_loaded ? "YES" : "NO");
    } else {
      LOGW("Manifest initialization failed - will use legacy system prompt");
      // Continue anyway - graceful fallback to old system
    }
  } else {
    LOGI("[JNI] Manifest already initialized, using existing registry");
    // Make sure it's attached to governor
    ethervox_governor_set_manifest(g_governor, g_manifest_registry);
    ethervox_get_tool_info_set_manifest(g_manifest_registry);
  }

  // Setup progress callback if provided
  jni_load_context_t load_ctx = {0};
  ethervox_load_progress_callback progress_callback = NULL;
  void* progress_user_data = NULL;
  
  if (callback) {
    jclass callback_class = (*env)->GetObjectClass(env, callback);
    jmethodID on_governor_progress = (*env)->GetMethodID(env, callback_class, "onGovernorProgress",
                                                        "(Ljava/lang/String;Ljava/lang/String;)V");
    if (on_governor_progress) {
      load_ctx.env = env;
      load_ctx.callback_obj = callback;
      load_ctx.on_governor_progress_method = on_governor_progress;
      progress_callback = native_load_progress_callback;
      progress_user_data = &load_ctx;
    }
  }

  // Load model - Governor will handle KV cache detection, loading, and saving internally
  // N6.1: Pass cache_dir string (not the whole paths struct - this version uses legacy signature)
  const char* cache_dir = g_paths_initialized ? g_android_paths.cache_dir : g_android_files_dir;
  LOGI("[JNI] Loading model (Governor will check for KV cache at: %s)...", cache_dir ? cache_dir : "(null)");
  ethervox_result_t result = ethervox_governor_load_model(
      g_governor, path, cache_dir, NULL, progress_callback, progress_user_data
  );

  (*env)->ReleaseStringUTFChars(env, modelPath, path);

  // Store error code for corruption detection
  g_last_governor_load_error = result;

  if (ethervox_is_success(result)) {
    LOGI("[JNI] Governor model loaded successfully");
    return JNI_TRUE;
  } else {
    if (result == -2) {
      LOGE("Failed to load Governor model - likely corrupted");
    } else {
      LOGE("Failed to load Governor model");
    }
    return JNI_FALSE;
  }
}

// N6.3: Deleted loadGovernorModelWithAudio() and isGovernorAudioCapable() - dead code
// from failed unified architecture. Android now uses two separate models:
//   - Governor (text-only) via loadGovernorModel()
//   - Granite Speech Plus (audio) via ethervox_voice_tools_init() -> STT backend
// See docs/MODELS.md §1 for why the unified approach failed.

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_wasLastLoadCorrupted(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;

  // -2 indicates probable corruption
  return (g_last_governor_load_error == -2) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_unloadGovernorModel(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_governor) {
    LOGE("Cannot unload Governor model - Governor not initialized");
    return JNI_FALSE;
  }

  LOGI("[JNI] Unloading Governor model to free memory");

  ethervox_result_t result = ethervox_governor_unload_model(g_governor);

  if (ethervox_is_success(result)) {
    LOGI("[JNI] Governor model unloaded successfully");
    return JNI_TRUE;
  } else {
    LOGE("Failed to unload Governor model");
    return JNI_FALSE;
  }
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_reloadGovernorModel(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_governor) {
    LOGE("Cannot reload Governor model - Governor not initialized");
    return JNI_FALSE;
  }

  LOGI("[JNI] Reloading Governor model");

  ethervox_result_t result = ethervox_governor_reload_model(g_governor);

  if (ethervox_is_success(result)) {
    LOGI("[JNI] Governor model reloaded successfully");
    return JNI_TRUE;
  } else {
    LOGE("Failed to reload Governor model");
    return JNI_FALSE;
  }
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_clearKvCache(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_governor) {
    LOGE("Cannot clear KV cache - Governor not initialized");
    return JNI_FALSE;
  }

  LOGI("[JNI] Clearing KV cache (force clear with summarization)");
  
  // Actually clear the KV cache using the summarize_and_clear_cache function
  // This will:
  // 1. Generate a summary of the conversation
  // 2. Store it in memory with context_summary tag
  // 3. Clear conversation tokens from KV cache (keeping system prompt)
  ethervox_result_t result = ethervox_governor_summarize_and_clear_cache(g_governor, true);
  
  if (ethervox_is_success(result)) {
    LOGI("[JNI] KV cache cleared successfully - context will be restored on next message");
    return JNI_TRUE;
  } else {
    LOGE("[JNI] Failed to clear KV cache");
    return JNI_FALSE;
  }
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_deleteSystemPromptCache(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;
  
  if (g_android_files_dir[0] == '\0') {
    LOGE("Cannot delete cache - files directory not set");
    return JNI_FALSE;
  }
  
  // Build cache directory path
  char cache_dir[512];
  snprintf(cache_dir, sizeof(cache_dir), "%s/.ethervox/cache", g_android_files_dir);
  
  LOGI("[JNI] Deleting all KV cache files in: %s", cache_dir);
  
  // Open directory and delete .kvcache files using native C operations
  DIR* dir = opendir(cache_dir);
  if (!dir) {
    // Try without .ethervox subdirectory
    snprintf(cache_dir, sizeof(cache_dir), "%s/cache", g_android_files_dir);
    LOGI("[JNI] Trying alternate path: %s", cache_dir);
    dir = opendir(cache_dir);
    
    if (!dir) {
      LOGW("[JNI] Cache directory does not exist - nothing to delete");
      return JNI_TRUE;  // Not an error if cache doesn't exist
    }
  }
  
  int deleted_count = 0;
  int error_count = 0;
  int total_files = 0;
  struct dirent* entry;
  
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    
    total_files++;
    LOGI("[JNI]   Found file: %s", entry->d_name);
    
    // Check if file ends with .kvcache
    size_t name_len = strlen(entry->d_name);
    if (name_len > 8 && strcmp(entry->d_name + name_len - 8, ".kvcache") == 0) {
      // Build full path
      char file_path[768];
      snprintf(file_path, sizeof(file_path), "%s/%s", cache_dir, entry->d_name);
      
      LOGI("[JNI]   Attempting to delete: %s", file_path);
      
      // Delete file using unlink
      if (unlink(file_path) == 0) {
        LOGI("[JNI]   ✓ Deleted: %s", entry->d_name);
        deleted_count++;
      } else {
        LOGE("[JNI]   ✗ Failed to delete: %s (errno=%d: %s)", 
             entry->d_name, errno, strerror(errno));
        error_count++;
      }
    }
  }
  
  closedir(dir);
  
  LOGI("[JNI] Directory scan complete: %d total files, %d deleted, %d errors", 
       total_files, deleted_count, error_count);
  
  if (deleted_count > 0) {
    LOGI("[JNI] ✓ Deleted %d KV cache file(s) - next load will regenerate", deleted_count);
  } else {
    LOGI("[JNI] No .kvcache files found to delete");
  }
  
  if (error_count > 0) {
    LOGW("[JNI] ⚠ Failed to delete %d file(s)", error_count);
  }
  
  // Return true if we deleted at least one file or if there were no files to delete
  return (deleted_count > 0 || error_count == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_isGovernorLoaded(JNIEnv* env,
                                                                                    jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_governor) {
    return JNI_FALSE;
  }

  return ethervox_governor_is_loaded(g_governor) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL Java_com_droid_ethervox_1core_NativeLib_getKvCacheStatus(JNIEnv* env,
                                                                                     jobject thiz) {
  (void)thiz;

  // Return runtime KV cache usage for UI visualization
  if (!g_governor || !ethervox_governor_is_loaded(g_governor)) {
    return NULL;
  }

  int32_t current_pos = 0;
  int32_t context_size = 0;
  float usage = ethervox_governor_get_kv_cache_usage(g_governor, &current_pos, &context_size);

  if (usage < 0.0f) {
    return NULL;
  }

  // Find KvCacheStatus class
  jclass kvCacheClass = (*env)->FindClass(env, "com/droid/ethervox_core/KvCacheStatus");
  if (!kvCacheClass) {
    ETHERVOX_LOGE("Failed to find KvCacheStatus class");
    return NULL;
  }

  // Get constructor: KvCacheStatus(currentPosition: Int, contextSize: Int, usagePercent: Float)
  jmethodID constructor = (*env)->GetMethodID(env, kvCacheClass, "<init>", "(IIF)V");
  if (!constructor) {
    ETHERVOX_LOGE("Failed to find KvCacheStatus constructor");
    return NULL;
  }

  // Create and return KvCacheStatus object with runtime stats
  jobject result = (*env)->NewObject(env, kvCacheClass, constructor, 
                                      (jint)current_pos, 
                                      (jint)context_size, 
                                      (jfloat)usage);
  
  return result;
}

JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_getKvCacheContents(JNIEnv* env,
                                                                                       jobject thiz) {
  (void)thiz;

  if (!g_governor || !ethervox_governor_is_loaded(g_governor)) {
    return NULL;
  }

  char* contents = ethervox_governor_get_kv_cache_contents(g_governor);
  if (!contents) {
    return NULL;
  }

  // Create Java string from C string
  jstring result = (*env)->NewStringUTF(env, contents);
  
  // Free the C string
  free(contents);
  
  return result;
}

JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_getSystemPromptContent(JNIEnv* env,
                                                                                          jobject thiz) {
  (void)thiz;

  if (!g_governor || !ethervox_governor_is_loaded(g_governor)) {
    return NULL;
  }

  char* contents = ethervox_governor_get_system_prompt_content(g_governor);
  if (!contents) {
    return NULL;
  }

  // Create Java string from C string
  jstring result = (*env)->NewStringUTF(env, contents);
  
  // Free the C string
  free(contents);
  
  return result;
}

JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_triggerConversationSummary(JNIEnv* env,
                                                                                              jobject thiz) {
  (void)thiz;

  if (!g_governor || !ethervox_governor_is_loaded(g_governor)) {
    return (*env)->NewStringUTF(env, "Error: Governor not loaded");
  }

  if (!g_memory_store || !g_memory_store->is_initialized) {
    return (*env)->NewStringUTF(env, "Error: Memory store not initialized");
  }

  // Allocate buffer for summary output
  char summary_buffer[2048];
  
  // Call the conversation summary generation function
  ethervox_result_t result = ethervox_generate_conversation_summary(g_governor, g_memory_store, summary_buffer, sizeof(summary_buffer));
  
  if (result == ETHERVOX_SUCCESS) {
    if (summary_buffer[0] == '\0') {
      return (*env)->NewStringUTF(env, "No conversation to summarize");
    }
    return (*env)->NewStringUTF(env, "Conversation summarized and moved into the KV cache");
  } else {
    // Get error message based on result code
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Error: Failed to generate summary (code %d)", result);
    return (*env)->NewStringUTF(env, error_msg);
  }
}

JNIEXPORT jobjectArray JNICALL
Java_com_droid_ethervox_1core_NativeLib_getConversationHistory(JNIEnv* env, jobject thiz) {
  (void)thiz;

  if (!g_memory_store || !g_memory_store->is_initialized) {
    // Return empty array if memory not initialized
    jclass memoryClass = (*env)->FindClass(env, "com/droid/ethervox_core/ConversationMemory");
    return (*env)->NewObjectArray(env, 0, memoryClass, NULL);
  }

  // Count non-summary entries (filter out context_summary tags)
  uint32_t conversationCount = 0;
  for (uint32_t i = 0; i < g_memory_store->entry_count; i++) {
    ethervox_memory_entry_t* entry = &g_memory_store->entries[i];
    
    // Skip entries with "context_summary" tag (these are model-only context)
    bool isSummary = false;
    for (uint32_t j = 0; j < entry->tag_count; j++) {
      if (strcmp(entry->tags[j], "context_summary") == 0) {
        isSummary = true;
        break;
      }
    }
    
    if (!isSummary) {
      conversationCount++;
    }
  }

  // Find ConversationMemory class
  jclass memoryClass = (*env)->FindClass(env, "com/droid/ethervox_core/ConversationMemory");
  if (!memoryClass) {
    ETHERVOX_LOGE("Failed to find ConversationMemory class");
    return (*env)->NewObjectArray(env, 0, memoryClass, NULL);
  }

  // Get constructor: ConversationMemory(memoryId: Long, turnId: Long, timestamp: Long, 
  //                                      text: String, isUser: Boolean, importance: Float, toolsUsed: Array<String>, isImported: Boolean)
  jmethodID constructor = (*env)->GetMethodID(env, memoryClass, "<init>", "(JJJLjava/lang/String;ZF[Ljava/lang/String;Z)V");
  if (!constructor) {
    ETHERVOX_LOGE("Failed to find ConversationMemory constructor");
    return (*env)->NewObjectArray(env, 0, memoryClass, NULL);
  }

  // Create array
  jobjectArray result = (*env)->NewObjectArray(env, (jsize)conversationCount, memoryClass, NULL);
  if (!result) {
    ETHERVOX_LOGE("Failed to create ConversationMemory array");
    return (*env)->NewObjectArray(env, 0, memoryClass, NULL);
  }

  // Populate array with non-summary entries
  uint32_t arrayIndex = 0;
  for (uint32_t i = 0; i < g_memory_store->entry_count; i++) {
    ethervox_memory_entry_t* entry = &g_memory_store->entries[i];
    
    // Skip entries with "context_summary" tag
    bool isSummary = false;
    for (uint32_t j = 0; j < entry->tag_count; j++) {
      if (strcmp(entry->tags[j], "context_summary") == 0) {
        isSummary = true;
        break;
      }
    }
    
    if (isSummary) {
      continue;
    }
    
    // Check if entry has "imported" tag
    bool isImported = false;
    for (uint32_t j = 0; j < entry->tag_count; j++) {
      if (strcmp(entry->tags[j], "imported") == 0) {
        isImported = true;
        break;
      }
    }
    
    // Debug log for imported status
    ETHERVOX_LOGI("Entry memory_id=%llu tags=%u isImported=%d", 
                 (unsigned long long)entry->memory_id, 
                 entry->tag_count,
                 isImported ? 1 : 0);

    // Debug log for tools count
    if (entry->tools_called_count > 0) {
      ETHERVOX_LOGI("Entry memory_id=%llu has %u tools", 
                   (unsigned long long)entry->memory_id, 
                   entry->tools_called_count);
    }

    // Create tools array
    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    jobjectArray toolsArray = (*env)->NewObjectArray(env, (jsize)entry->tools_called_count, stringClass, NULL);
    for (uint32_t t = 0; t < entry->tools_called_count; t++) {
      jstring toolName = (*env)->NewStringUTF(env, entry->tools_called[t]);
      (*env)->SetObjectArrayElement(env, toolsArray, (jsize)t, toolName);
      (*env)->DeleteLocalRef(env, toolName);
    }
    
    // Create ConversationMemory object
    jstring text = (*env)->NewStringUTF(env, entry->text);
    jobject memoryObj = (*env)->NewObject(
      env, memoryClass, constructor,
      (jlong)entry->memory_id,
      (jlong)entry->turn_id,
      (jlong)entry->timestamp,
      text,
      (jboolean)entry->is_user_message,
      (jfloat)entry->importance,
      toolsArray,
      (jboolean)isImported
    );
    
    (*env)->SetObjectArrayElement(env, result, (jsize)arrayIndex, memoryObj);
    (*env)->DeleteLocalRef(env, text);
    (*env)->DeleteLocalRef(env, toolsArray);
    (*env)->DeleteLocalRef(env, memoryObj);
    
    arrayIndex++;
  }

  ETHERVOX_LOGI("Returned %u conversation messages from memory", conversationCount);
  return result;
}

JNIEXPORT jobjectArray JNICALL
Java_com_droid_ethervox_1core_NativeLib_getRegisteredPlugins(JNIEnv* env, jobject thiz) {
  if (!g_registry) {
    // Return empty array if not initialized
    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    return (*env)->NewObjectArray(env, 0, stringClass, NULL);
  }

  int tool_count = g_registry->tool_count;

  // Create Java string array
  jclass stringClass = (*env)->FindClass(env, "java/lang/String");
  jobjectArray result = (*env)->NewObjectArray(env, tool_count, stringClass, NULL);

  // Populate array with tool names
  for (int i = 0; i < tool_count; i++) {
    jstring toolName = (*env)->NewStringUTF(env, g_registry->tools[i].name);
    (*env)->SetObjectArrayElement(env, result, i, toolName);
    (*env)->DeleteLocalRef(env, toolName);
  }

  return result;
}

// Thread data for optimization (to avoid blocking UI)
typedef struct {
  ethervox_governor_t* governor;
  tool_manifest_registry_t* manifest_registry;
  char model_path[512];
  bool optimize_new_only;
  int result;
  bool completed;
  pthread_mutex_t mutex;
} optimization_thread_data_t;

// Optimization thread function
static void* optimization_thread_func(void* arg) {
  optimization_thread_data_t* data = (optimization_thread_data_t*)arg;

  LOGI("Optimization thread started for model: %s", data->model_path);

  // Run optimizer (this can take 30-60 seconds)
  ethervox_result_t result = ethervox_optimize_tool_prompts(
      data->governor, data->model_path, data->manifest_registry, data->optimize_new_only);

  // Update result atomically
  pthread_mutex_lock(&data->mutex);
  data->result = result;
  data->completed = true;
  pthread_mutex_unlock(&data->mutex);

  if (ethervox_is_success(result)) {
    LOGI("Optimization thread completed successfully");
  } else {
    LOGE("Optimization thread failed with code: %d", result);
  }

  return NULL;
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_optimizeToolPrompts(
    JNIEnv* env, jobject thiz, jstring modelPath) {
  (void)thiz;

  if (!g_governor) {
    LOGE("Governor not loaded - cannot optimize tool prompts");
    return -1;
  }

  if (!g_manifest_registry) {
    LOGE("Manifest registry not initialized - ensure model is loaded first");
    return -2;
  }

  const char* model_path = (*env)->GetStringUTFChars(env, modelPath, NULL);
  if (!model_path) {
    LOGE("Failed to get model path string");
    return -3;
  }

  LOGI("Starting tool prompt optimization (V2) for model: %s", model_path);
  LOGI("This may take 30-60 seconds and is memory-intensive");

  // Allocate thread data
  optimization_thread_data_t* thread_data = malloc(sizeof(optimization_thread_data_t));
  if (!thread_data) {
    LOGE("Failed to allocate optimization thread data");
    (*env)->ReleaseStringUTFChars(env, modelPath, model_path);
    return -4;
  }

  // Initialize thread data
  thread_data->governor = g_governor;
  thread_data->manifest_registry = g_manifest_registry;
  snprintf(thread_data->model_path, sizeof(thread_data->model_path), "%s", model_path);
  thread_data->optimize_new_only = false;  // Always optimize all tools
  thread_data->result = -999;              // Sentinel for "not completed"
  thread_data->completed = false;
  pthread_mutex_init(&thread_data->mutex, NULL);

  LOGI("Thread data initialized: optimize_new_only=%s",
       thread_data->optimize_new_only ? "true" : "false");

  (*env)->ReleaseStringUTFChars(env, modelPath, model_path);

  // Create optimization thread (to avoid blocking UI)
  pthread_t thread;
  if (pthread_create(&thread, NULL, optimization_thread_func, thread_data) != 0) {
    LOGE("Failed to create optimization thread");
    pthread_mutex_destroy(&thread_data->mutex);
    free(thread_data);
    return -5;
  }

  // Detach thread and wait for completion
  pthread_detach(thread);

  // Wait for completion with periodic logging
  while (!thread_data->completed) {
    sleep(5);  // Check every 5 seconds

    pthread_mutex_lock(&thread_data->mutex);
    bool is_completed = thread_data->completed;
    pthread_mutex_unlock(&thread_data->mutex);

    if (!is_completed) {
      static int wait_seconds = 0;
      wait_seconds += 5;
      LOGI("Optimization in progress... (%d seconds elapsed)", wait_seconds);
    }
  }

  // Get final result
  pthread_mutex_lock(&thread_data->mutex);
  int result = thread_data->result;
  pthread_mutex_unlock(&thread_data->mutex);

  // Cleanup
  pthread_mutex_destroy(&thread_data->mutex);
  free(thread_data);

  if (ethervox_is_success(result)) {
    LOGI("Tool prompt optimization completed successfully");
  } else {
    LOGE("Tool prompt optimization failed with code: %d", result);
  }

  return result;
}

/**
 * Get manifest registry information
 * Returns JSON string with current manifest state
 */
JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_getManifestInfo(JNIEnv* env,
                                                                                  jobject thiz) {
  (void)thiz;

  if (!g_manifest_registry) {
    // No manifest available - return minimal JSON
    return (*env)->NewStringUTF(
        env,
        "{\"available\":false,\"tool_count\":0,\"fallback_level\":3,\"optimized_loaded\":false,"
        "\"tools_detected\":false,\"tools_loaded_count\":0}");
  }

  ETHERVOX_LOGI("DEBUG getManifestInfo: reading tools_loaded_count=%u",
                g_manifest_registry->tools_loaded_count);

  // Build JSON response with guard status
  char json[512];
  snprintf(json, sizeof(json),
           "{\"available\":%s,\"tool_count\":%u,\"fallback_level\":%u,\"optimized_loaded\":%s,"
           "\"tools_detected\":%s,\"tools_loaded_count\":%u}",
           g_manifest_registry->tools_available ? "true" : "false",
           g_manifest_registry->header.tool_count, g_manifest_registry->fallback_level,
           g_manifest_registry->optimization_loaded ? "true" : "false",
           g_manifest_registry->tools_detected ? "true" : "false",
           g_manifest_registry->tools_loaded_count);

  return (*env)->NewStringUTF(env, json);
}

/**
 * Get the optimized tool prompts directory path
 * Returns the absolute path where optimized JSON files are stored/expected
 */
JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_getOptimizedDir(JNIEnv* env,
                                                                                  jobject thiz) {
  (void)thiz;

  char optimized_dir[512];

  if (g_android_files_dir[0] != '\0') {
    // Android: Use app files directory
    snprintf(optimized_dir, sizeof(optimized_dir), "%s/tools/optimized", g_android_files_dir);
  } else {
    // Fallback (should not happen on Android)
    const char* home = getenv("HOME");
    snprintf(optimized_dir, sizeof(optimized_dir), "%s/.ethervox/tools/optimized",
             home ? home : ".");
  }

  return (*env)->NewStringUTF(env, optimized_dir);
}

/**
 * Check if optimization file exists for a given model
 * Returns true if the optimized JSON file exists on disk
 */
JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_optimizationFileExists(
    JNIEnv* env, jobject thiz, jstring modelPath) {
  (void)thiz;

  if (!modelPath) {
    return JNI_FALSE;
  }

  const char* model_path_str = (*env)->GetStringUTFChars(env, modelPath, NULL);
  if (!model_path_str) {
    return JNI_FALSE;
  }

  // Extract model name from path (e.g., /path/to/granite-4.0-h-1b-Q4_K_M.gguf -> granite-4.0-h-1b)
  const char* filename = strrchr(model_path_str, '/');
  if (!filename) {
    filename = model_path_str;
  } else {
    filename++;  // Skip the '/'
  }

  // Copy filename and remove .gguf extension if present
  char model_name[256];
  size_t len = strlen(filename);
  
  // Check if filename ends with .gguf and calculate length without extension
  size_t copy_len = len;
  if (len > 5 && strcmp(filename + len - 5, ".gguf") == 0) {
    copy_len = len - 5;
  }
  
  // Copy the filename without .gguf extension
  if (copy_len >= sizeof(model_name)) {
    copy_len = sizeof(model_name) - 1;
  }
  memcpy(model_name, filename, copy_len);
  model_name[copy_len] = '\0';
  
  // Remove quantization suffix (e.g., -Q4_K_M, -Q6_K, etc.)
  // Look for pattern like -Q followed by digits and optional _K or _M suffix
  char* q_pos = strstr(model_name, "-Q");
  if (q_pos) {
    // Verify it looks like a quantization suffix (Q followed by digit)
    if (q_pos[2] >= '0' && q_pos[2] <= '9') {
      *q_pos = '\0';  // Truncate at the Q
      copy_len = q_pos - model_name;
    }
  }

  // Remove trailing dash if present
  if (copy_len > 0 && model_name[copy_len - 1] == '-') {
    model_name[copy_len - 1] = '\0';
  }

  (*env)->ReleaseStringUTFChars(env, modelPath, model_path_str);

  // Build path to optimization file
  char optimized_path[512];
  if (g_android_files_dir[0] != '\0') {
    snprintf(optimized_path, sizeof(optimized_path), "%s/tools/optimized/%s.json",
             g_android_files_dir, model_name);
  } else {
    const char* home = getenv("HOME");
    snprintf(optimized_path, sizeof(optimized_path), "%s/.ethervox/tools/optimized/%s.json",
             home ? home : ".", model_name);
  }

  // Check if file exists
  struct stat st;
  jboolean exists = (stat(optimized_path, &st) == 0) ? JNI_TRUE : JNI_FALSE;

  ETHERVOX_LOGI("Checking optimization file: %s -> %s", optimized_path,
                exists ? "EXISTS" : "NOT FOUND");

  return exists;
}

/**
 * Run comprehensive integration tests
 */
JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_runIntegrationTests(JNIEnv* env,
                                                                                   jobject thiz) {
  (void)env;
  (void)thiz;

  LOGI("Running integration tests...");
  run_integration_tests(g_governor);

  // Log completion with test reports path
  if (g_android_files_dir[0] != '\0') {
    LOGI("Integration tests complete. Reports saved to: %s/tests/", g_android_files_dir);
  } else {
    LOGI("Integration tests complete");
  }
}

/**
 * Run LLM tool usage tests
 */
JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_runLlmToolTests(JNIEnv* env,
                                                                               jobject thiz,
                                                                               jstring modelPath,
                                                                               jboolean verbose) {
  (void)thiz;

  if (!g_governor) {
    LOGE("Governor not loaded - cannot run LLM tool tests");
    return;
  }

  if (!g_memory_store) {
    LOGE("Memory store not initialized - cannot run LLM tool tests");
    return;
  }

  const char* model_path_str = (*env)->GetStringUTFChars(env, modelPath, NULL);
  if (!model_path_str) {
    LOGE("Failed to get model path string");
    return;
  }

  LOGI("Running LLM tool tests (verbose=%d)...", verbose);
  run_llm_tool_tests(g_governor, g_memory_store, model_path_str, (bool)verbose, NULL);

  // Log completion with test reports path
  if (g_android_files_dir[0] != '\0') {
    LOGI("LLM tool tests complete. Reports saved to: %s/tests/", g_android_files_dir);
  } else {
    LOGI("LLM tool tests complete");
  }

  (*env)->ReleaseStringUTFChars(env, modelPath, model_path_str);
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_platformInit(
    JNIEnv* env,
    jobject thiz,
    jlong memory_budget_bytes) {
  (void)thiz;

  if (g_platform) {
    LOGI("Platform already initialized");
    return 0;
  }

  // N6.2b: Validate memory budget (fail init if budget is absurd)
  const uint64_t MIN_BUDGET_BYTES = 500ULL * 1024 * 1024;   // 500 MB (governor alone is ~900MB)
  const uint64_t MAX_BUDGET_BYTES = 20ULL * 1024 * 1024 * 1024;  // 20 GB (sanity check)
  
  if (memory_budget_bytes <= 0) {
    LOGE("Memory budget is zero or negative: %lld bytes - init failed", (long long)memory_budget_bytes);
    LOGE("Budget derivation formula may be incorrect (totalRAM too small or headroom too large)");
    return -1;
  }
  
  if ((uint64_t)memory_budget_bytes < MIN_BUDGET_BYTES) {
    LOGE("Memory budget too small: %lld bytes (%.1f MB) - minimum is %llu MB",
         (long long)memory_budget_bytes,
         memory_budget_bytes / (1024.0 * 1024.0),
         MIN_BUDGET_BYTES / (1024ULL * 1024));
    LOGE("Device likely has insufficient RAM for model loading");
    return -1;
  }
  
  if ((uint64_t)memory_budget_bytes > MAX_BUDGET_BYTES) {
    LOGE("Memory budget absurdly large: %lld bytes (%.1f GB) - maximum is %llu GB",
         (long long)memory_budget_bytes,
         memory_budget_bytes / (1024.0 * 1024.0 * 1024.0),
         MAX_BUDGET_BYTES / (1024ULL * 1024 * 1024));
    LOGE("Budget derivation may be incorrect");
    return -1;
  }
  
  LOGI("Memory budget validated: %lld bytes (%.2f GB)",
       (long long)memory_budget_bytes,
       memory_budget_bytes / (1024.0 * 1024.0 * 1024.0));

  g_platform = (ethervox_platform_t*)calloc(1, sizeof(ethervox_platform_t));
  if (!g_platform) {
    LOGE("Failed to allocate platform");
    return -1;
  }

  ethervox_result_t result = ethervox_platform_init(g_platform);
  if (ethervox_is_error(result)) {
    LOGE("Failed to initialize platform");
    free(g_platform);
    g_platform = NULL;
    return -1;
  }

  // === DIRECT GOVERNOR INITIALIZATION (matches desktop main.c) ===
  // REFACTORED 2025-12-09: Removed dialogue_engine wrapper
  // Android now mirrors desktop architecture for consistency

  // Initialize memory store BEFORE tools so they can register with it
  g_memory_store = (ethervox_memory_store_t*)calloc(1, sizeof(ethervox_memory_store_t));
  if (g_memory_store) {
    // Initialize with NULL session_id (auto-generated) and NULL storage_dir (set later via JNI)
    ethervox_result_t mem_result = ethervox_memory_init(g_memory_store, NULL, NULL);
    if (ethervox_is_error(mem_result)) {
      LOGE("Failed to initialize memory store - error code: %d", mem_result);
      free(g_memory_store);
      g_memory_store = NULL;
    } else {
      LOGI("Memory store initialized successfully (in-memory mode until storage dir set)");
    }
  } else {
    LOGE("Failed to allocate memory store");
  }

  // Create and initialize tool registry
  g_registry = (ethervox_tool_registry_t*)malloc(sizeof(ethervox_tool_registry_t));
  if (!g_registry) {
    LOGE("Failed to allocate tool registry");
    if (g_memory_store) {
      ethervox_memory_cleanup(g_memory_store);
      free(g_memory_store);
      g_memory_store = NULL;
    }
    ethervox_platform_cleanup(g_platform);
    free(g_platform);
    g_platform = NULL;
    return -1;
  }

  ethervox_result_t registry_result = ethervox_tool_registry_init(g_registry, 16);
  if (ethervox_is_error(registry_result)) {
    LOGE("Failed to initialize tool registry - error code: %d", registry_result);
    free(g_registry);
    g_registry = NULL;
    if (g_memory_store) {
      ethervox_memory_cleanup(g_memory_store);
      free(g_memory_store);
      g_memory_store = NULL;
    }
    ethervox_platform_cleanup(g_platform);
    free(g_platform);
    g_platform = NULL;
    return -1;
  }

  LOGI("Tool registry initialized successfully");

  // Register compute tools (math, logic, etc.)
  int tool_count = ethervox_compute_tools_register_all(g_registry);
  LOGI("Registered %d compute tools", tool_count);

  // Register timer/alarm tools
  ethervox_tool_registry_add(g_registry, ethervox_tool_timer_create());
  ethervox_tool_registry_add(g_registry, ethervox_tool_timer_cancel());
  ethervox_tool_registry_add(g_registry, ethervox_tool_timer_list());
  ethervox_tool_registry_add(g_registry, ethervox_tool_alarm_create());
  tool_count += 4;

  // Register memory tools if memory store is available
  if (g_memory_store) {
    ethervox_result_t mem_tools_result = ethervox_memory_tools_register(g_registry, g_memory_store);
    if (ethervox_is_success(mem_tools_result)) {
      tool_count += 8;  // 8 memory tools (corrected from 6)
      LOGI("Registered memory tools successfully");
    } else {
      LOGE("Failed to register memory tools - error code: %d", mem_tools_result);
    }
  } else {
    LOGI("Memory store not available, skipping memory tools registration");
  }

  // Register conversation tools (speak, listen)
  ethervox_result_t conv_result = ethervox_conversation_tools_register(g_registry);
  if (ethervox_is_success(conv_result)) {
    tool_count += 2;  // speak, listen
    LOGI("Registered conversation tools successfully");
  } else {
    LOGE("Failed to register conversation tools - error code: %d", conv_result);
  }

  // Register unit conversion tool
  ethervox_result_t unit_result = ethervox_unit_conversion_register(g_registry);
  if (ethervox_is_success(unit_result)) {
    tool_count++;
    LOGI("Registered unit conversion tool");
  }

#if HAVE_LIBCURL && ETHERVOX_FEATURE_WEATHER
  // Register weather tools (requires libcurl - only available on desktop)
  ethervox_result_t weather_result = ethervox_weather_tools_register(g_registry);
  if (ethervox_is_success(weather_result)) {
    tool_count++;
    LOGI("Registered weather forecast tool");
  } else {
    LOGW("Weather tools unavailable (libcurl failed: %d)", weather_result);
  }
#elif defined(ETHERVOX_PLATFORM_ANDROID) && ETHERVOX_FEATURE_WEATHER
  // Android: Register weather tools (uses JNI HTTP client)
  ethervox_result_t weather_result = ethervox_weather_tools_register(g_registry);
  if (ethervox_is_success(weather_result)) {
    tool_count++;
    LOGI("Registered weather forecast tool (Android)");
  } else {
    LOGE("Failed to register weather tools: %d", weather_result);
  }
#else
  LOGI("Weather tools not available (no HTTP client)");
#endif

  // Register get_tool_info meta-tool for dynamic tool discovery
  ethervox_result_t tool_info_result = ethervox_get_tool_info_register(g_registry);
  if (ethervox_is_success(tool_info_result)) {
    tool_count++;
    LOGI("Registered get_tool_info meta-tool");
  }

  // Register system info tools
  ethervox_result_t sys_info_result = ethervox_system_info_tools_register(g_registry);
  if (ethervox_is_success(sys_info_result)) {
    tool_count += 2;  // get_system_info, get_device_info
    LOGI("Registered system info tools");
  }

  // Register startup prompt tools
  ethervox_result_t startup_result = ethervox_startup_prompt_tools_register(g_registry);
  if (ethervox_is_success(startup_result)) {
    tool_count++;
    LOGI("Registered startup prompt tools");
  }

  // Note: File tools not registered on Android for security reasons
  // Android apps have limited file system access

  LOGI("Total tools registered: %d", tool_count);

  // Create model pool for memory-managed model loading (N6.2a/N6.2b)
  // N6.2b: Budget derived from device RAM via Kotlin formula (totalRAM - 1.5GB headroom)
  if (g_paths_initialized) {
    ethervox_result_t pool_result = ethervox_model_pool_create(
        &g_android_paths,
        (uint64_t)memory_budget_bytes,  // N6.2b: device-derived budget
        &g_model_pool
    );
    if (ethervox_is_error(pool_result)) {
      LOGE("Failed to create model pool - error code: %d", pool_result);
      // Continue anyway - fall back to direct loading if pool unavailable
      g_model_pool = NULL;
    } else {
      LOGI("Model pool created with budget: %.2f GB",
           memory_budget_bytes / (1024.0 * 1024.0 * 1024.0));
    }
  } else {
    LOGW("Paths not initialized yet - model pool creation deferred");
  }

  // NOTE: Governor initialization moved to platformInitGovernor()
  // This allows user to choose minimal vs full mode before governor init

  LOGI(
      "Platform, memory store, and tool registry initialized successfully (Governor pending user "
      "choice)");

  return 0;
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_platformInitGovernor(
    JNIEnv* env, jobject thiz, jboolean minimal_mode, jboolean is_finetuned) {
  (void)env;
  (void)thiz;

  if (g_governor) {
    LOGI("Governor already initialized - cleaning up to reinitialize with new mode");
    // Unload model first if loaded
    if (ethervox_governor_is_loaded(g_governor)) {
      LOGI("Unloading existing Governor model");
      ethervox_governor_unload_model(g_governor);
    }
    // Clean up and free the Governor instance (cleanup already calls free)
    ethervox_governor_cleanup(g_governor);
    g_governor = NULL;
    LOGI("Governor cleaned up, ready to reinitialize");
  }

  if (!g_platform || !g_registry) {
    LOGE("Platform not initialized - call platformInit() first");
    return -1;
  }

  // Initialize Governor with chosen config (minimal, finetuned, or full)
  ethervox_governor_config_t gov_config = ethervox_governor_default_config();

  // Apply runtime configuration from g_governor_config
  gov_config.context_size = g_governor_config.context_size;
  gov_config.temperature = g_governor_config.temperature;
  gov_config.n_threads = g_governor_config.threads;
  gov_config.max_tokens_per_response = g_governor_config.max_tokens_per_response;

  if (minimal_mode) {
    gov_config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_MINIMAL;
    LOGI("Initializing Governor in MINIMAL MODE (fast loading, tools disabled)");
  } else if (is_finetuned) {
    gov_config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_FINETUNED;
    LOGI("Initializing Governor in FINETUNED MODE (basic instructions, no tool list)");
  } else {
    gov_config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_FULL;
    LOGI("Initializing Governor in FULL MODE (all tools available)");
  }

  LOGI("Governor config: ctx=%d, temp=%.2f, threads=%d, max_tokens=%d",
       gov_config.context_size, gov_config.temperature, gov_config.n_threads,
       gov_config.max_tokens_per_response);

  ethervox_result_t gov_result = ethervox_governor_init(&g_governor, &gov_config, g_registry);
  if (ethervox_is_error(gov_result)) {
    LOGE("Failed to initialize Governor - error code: %d", gov_result);
    return -1;
  }

  // N6.2a-core: Route governor model loading through the memory pool
  if (g_model_pool) {
    ethervox_result_t pool_result = ethervox_governor_set_model_pool(g_governor, g_model_pool);
    if (ethervox_is_error(pool_result)) {
      LOGE("Failed to set model pool on governor - error code: %d", pool_result);
      ethervox_governor_cleanup(g_governor);
      g_governor = NULL;
      return -1;
    }
    LOGI("[Governor] Configured to load via model pool");
  }

  const char* mode_name = minimal_mode ? "MINIMAL" : (is_finetuned ? "FINETUNED" : "FULL");
  LOGI("Governor initialized successfully (mode: %s)", mode_name);

  return 0;
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_platformCleanup(JNIEnv* env,
                                                                               jobject thiz) {
  (void)env;
  (void)thiz;

  // Cleanup in reverse order of initialization

  // Cleanup model pool (N6.2a) - unloads all models
  if (g_model_pool) {
    ethervox_model_pool_destroy(g_model_pool);
    g_model_pool = NULL;
    g_governor_handle = NULL;  // Handles invalidated by pool destruction
    g_speech_handle = NULL;
    LOGI("Model pool cleaned up");
  }

  if (g_manifest_registry) {
    // Manifest registry cleanup (if needed)
    free(g_manifest_registry);
    g_manifest_registry = NULL;
    LOGI("Manifest registry cleaned up");
  }

  if (g_governor) {
    ethervox_governor_cleanup(g_governor);  // cleanup already frees governor
    g_governor = NULL;
    LOGI("Governor cleaned up");
  }

  if (g_registry) {
    ethervox_tool_registry_cleanup(g_registry);
    free(g_registry);
    g_registry = NULL;
    LOGI("Tool registry cleaned up");
  }

  if (g_memory_store) {
    ethervox_memory_cleanup(g_memory_store);
    free(g_memory_store);
    g_memory_store = NULL;
    LOGI("Memory store cleaned up");
  }

  if (g_platform) {
    ethervox_platform_cleanup(g_platform);
    free(g_platform);
    g_platform = NULL;
    LOGI("Platform cleaned up");
  }
}

// DEPRECATED: This function is no longer used.
// Initialization now happens in platformInit() using direct governor/registry globals.
// Kept for binary compatibility with old Java code, but returns success immediately.
JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_initializeWithModel(
    JNIEnv* env, jobject thiz, jstring model_path, jfloat temperature, jint max_tokens,
    jfloat top_p, jint context_length) {
  (void)env;
  (void)thiz;
  (void)model_path;
  (void)temperature;
  (void)max_tokens;
  (void)top_p;
  (void)context_length;

  LOGI("initializeWithModel() called but deprecated - use platformInit() instead");

  // Return success if already initialized via platformInit
  if (g_governor) {
    return JNI_TRUE;
  }

  LOGE("Governor not initialized - call platformInit() first");
  return JNI_FALSE;
}

// DEPRECATED: Old dialogue engine API - runtime param updates not supported by governor
// Governor uses config set at initialization time only
JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_updateLLMParams(
    JNIEnv* env, jobject thiz, jfloat temperature, jint max_tokens, jfloat top_p) {
  (void)env;
  (void)thiz;
  (void)temperature;
  (void)max_tokens;
  (void)top_p;

  LOGW("updateLLMParams() deprecated - governor does not support runtime param updates");
  LOGW("LLM parameters are set at initialization time only");
  return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_isLlmLoaded(JNIEnv* env,
                                                                               jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_governor) {
    return JNI_FALSE;
  }

  // Use governor API to check if model is loaded
  return ethervox_governor_is_loaded(g_governor) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_getPlatformName(JNIEnv* env,
                                                                                  jobject thiz) {
  (void)thiz;

  const char* name = ethervox_platform_get_name();
  return create_jstring(env, name);
}

// ===========================================================================
// Audio Initialization
// ===========================================================================

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_audioInit(JNIEnv* env, jobject thiz,
                                                                         jint sample_rate,
                                                                         jint channels,
                                                                         jint buffer_size) {
  (void)thiz;

  if (g_audio_runtime) {
    LOGI("Audio already initialized");
    return 0;
  }

  g_audio_runtime = (ethervox_audio_runtime_t*)calloc(1, sizeof(ethervox_audio_runtime_t));
  if (!g_audio_runtime) {
    LOGE("Failed to allocate audio runtime");
    return -1;
  }

  ethervox_audio_config_t config = ethervox_audio_get_default_config();
  config.sample_rate = (uint32_t)sample_rate;
  config.channels = (uint16_t)channels;
  config.buffer_size = (uint32_t)buffer_size;

  ethervox_result_t result = ethervox_audio_init(g_audio_runtime, &config);
  if (ethervox_is_error(result)) {
    LOGE("Failed to initialize audio");
    free(g_audio_runtime);
    g_audio_runtime = NULL;
    return -1;
  }

  LOGI("Audio initialized: %d Hz, %d channels, buffer=%d", sample_rate, channels, buffer_size);
  return 0;
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_audioStartCapture(JNIEnv* env,
                                                                                 jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_audio_runtime) {
    LOGE("Audio not initialized");
    return -1;
  }

  ethervox_result_t result = ethervox_audio_start_capture(g_audio_runtime);
  if (ethervox_is_error(result)) {
    LOGE("Failed to start audio capture");
    return -1;
  }

  LOGI("Audio capture started");
  return 0;
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_audioStopCapture(JNIEnv* env,
                                                                                jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_audio_runtime) {
    return 0;
  }

  ethervox_result_t result = ethervox_audio_stop_capture(g_audio_runtime);
  LOGI("Audio capture stopped");
  return ethervox_is_success(result) ? 0 : -1;
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_audioCleanup(JNIEnv* env,
                                                                            jobject thiz) {
  (void)env;
  (void)thiz;

  if (g_audio_runtime) {
    ethervox_audio_cleanup(g_audio_runtime);
    free(g_audio_runtime);
    g_audio_runtime = NULL;
    LOGI("Audio cleaned up");
  }
}

// ===========================================================================
// Wake Word Detection
// ===========================================================================

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_wakeWordInit(JNIEnv* env,
                                                                            jobject thiz,
                                                                            jstring wake_word,
                                                                            jfloat sensitivity) {
  (void)thiz;

  if (g_wake_runtime) {
    LOGI("Wake word already initialized");
    return 0;
  }

  g_wake_runtime = (ethervox_wake_runtime_t*)calloc(1, sizeof(ethervox_wake_runtime_t));
  if (!g_wake_runtime) {
    LOGE("Failed to allocate wake word runtime");
    return -1;
  }

  ethervox_wake_config_t config = ethervox_wake_get_default_config();

  const char* wake_word_str = (*env)->GetStringUTFChars(env, wake_word, NULL);
  config.wake_word = wake_word_str;
  config.sensitivity = (float)sensitivity;

  ethervox_result_t result = ethervox_wake_init(g_wake_runtime, &config);

  (*env)->ReleaseStringUTFChars(env, wake_word, wake_word_str);

  if (ethervox_is_error(result)) {
    LOGE("Failed to initialize wake word");
    free(g_wake_runtime);
    g_wake_runtime = NULL;
    return -1;
  }

  LOGI("Wake word initialized with sensitivity %.2f", sensitivity);
  return 0;
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_wakeWordCleanup(JNIEnv* env,
                                                                               jobject thiz) {
  (void)env;
  (void)thiz;

  if (g_wake_runtime) {
    // Note: would call ethervox_wake_cleanup if it exists
    free(g_wake_runtime);
    g_wake_runtime = NULL;
    LOGI("Wake word cleaned up");
  }
}

// ===========================================================================
// Speech-to-Text (generic, currently unused)
//
// NOTE (Granite Speech integration): sttInit/sttStart/sttCleanup below are a
// generic, backend-selectable STT session that predates the Granite Speech
// work and is not currently called by any Android flow:
//   - Mode 2 (Transcription) uses ethervox_voice_tools_init/start_listen
//     directly (see "Voice Tools / Transcription" section below), which
//     already initializes Granite Speech Plus (SAA) internally - it does not
//     go through sttInit at all.
//   - Mode 1 (voice conversation) and Mode 4 (voice-to-text) now go through
//     ethervox_conversation_init/start (see "Voice Conversation" section
//     below), which resolves and initializes the Granite Speech Plus model
//     (ASR-only prompt) internally via voice_conversation.c - they also do
//     not go through sttInit.
// This generic path is left in place only in case a future feature needs a
// standalone STT session outside of either orchestrator; it is not part of
// the Mode 1/2/4 JNI surface NativeLib.kt should call.
// ===========================================================================

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_sttInit(JNIEnv* env, jobject thiz,
                                                                       jstring model_path,
                                                                       jstring language) {
  (void)thiz;

  if (g_stt_runtime) {
    LOGI("STT already initialized");
    return 0;
  }

  g_stt_runtime = (ethervox_stt_runtime_t*)calloc(1, sizeof(ethervox_stt_runtime_t));
  if (!g_stt_runtime) {
    LOGE("Failed to allocate STT runtime");
    return -1;
  }

  ethervox_stt_config_t config = ethervox_stt_get_default_config();

  const char* model_path_str = (*env)->GetStringUTFChars(env, model_path, NULL);
  const char* language_str = (*env)->GetStringUTFChars(env, language, NULL);

  config.model_path = model_path_str;
  config.language = language_str;

  ethervox_result_t result = ethervox_stt_init(g_stt_runtime, &config);

  (*env)->ReleaseStringUTFChars(env, model_path, model_path_str);
  (*env)->ReleaseStringUTFChars(env, language, language_str);

  if (ethervox_is_error(result)) {
    LOGE("Failed to initialize STT");
    free(g_stt_runtime);
    g_stt_runtime = NULL;
    return -1;
  }

  LOGI("STT initialized");
  return 0;
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_sttStart(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_stt_runtime) {
    LOGE("STT not initialized");
    return -1;
  }

  ethervox_result_t result = ethervox_stt_start(g_stt_runtime);
  if (ethervox_is_error(result)) {
    LOGE("Failed to start STT");
    return -1;
  }

  LOGI("STT started");
  return 0;
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_sttCleanup(JNIEnv* env,
                                                                          jobject thiz) {
  (void)env;
  (void)thiz;

  if (g_stt_runtime) {
    // Note: would call ethervox_stt_cleanup if it exists
    free(g_stt_runtime);
    g_stt_runtime = NULL;
    LOGI("STT cleaned up");
  }
}

// ===========================================================================
// LLM / Dialogue Management
// ===========================================================================

// DEPRECATED: Legacy dialogue processing - use processGovernorQuery instead
// This function is kept for backwards compatibility but now delegates to governor
JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_processDialogue(
    JNIEnv* env, jobject thiz, jstring user_text, jstring language) {
  (void)thiz;
  (void)language;  // Language detection now handled by governor

  if (!g_governor) {
    LOGE("Governor not initialized");
    return create_jstring(env, "[ERROR] Governor not initialized");
  }

  const char* text = (*env)->GetStringUTFChars(env, user_text, NULL);

  LOGI("Processing dialogue (legacy): '%s'", text);

  // Store user message to memory (before processing)
  if (g_memory_store) {
    const char* tags[] = {"conversation"};
    uint64_t memory_id;
    ethervox_result_t mem_result =
        ethervox_memory_store_add(g_memory_store, text, tags, 1, 0.8f, true, &memory_id);
    if (ethervox_is_error(mem_result)) {
      LOGW("Failed to store user message to memory: Error %d", mem_result);
    } else {
      LOGI("Stored user message to memory (ID: %llu)", (unsigned long long)memory_id);
    }
  }

  // N6.4: Create cancel token for synchronous execution
  if (g_cancel_token) {
    ethervox_cancel_token_free(g_cancel_token);
    g_cancel_token = NULL;
  }
  ethervox_result_t token_result = ethervox_cancel_token_create(&g_cancel_token);
  if (ethervox_is_error(token_result)) {
    LOGE("Failed to create cancel token");
    (*env)->ReleaseStringUTFChars(env, user_text, text);
    return (*env)->NewStringUTF(env, "[ERROR] Failed to create cancel token");
  }

  // Process with governor execute API
  char* response = NULL;
  char* error = NULL;
  ethervox_governor_status_t status =
      ethervox_governor_execute(g_governor, text, g_cancel_token, &response, &error, NULL, NULL, NULL, NULL, NULL);

  (*env)->ReleaseStringUTFChars(env, user_text, text);

  if (status != ETHERVOX_GOVERNOR_SUCCESS || !response) {
    LOGE("Failed to process query: %s", error ? error : "Unknown error");
    jstring result = create_jstring(env, "[ERROR] Failed to generate response");
    if (response)
      free(response);
    if (error)
      free(error);
    return result;
  }

  // Store assistant response to memory (after successful processing)
  if (g_memory_store && response) {
    const char* tags[] = {"conversation"};
    uint64_t memory_id;
    float importance = (status == ETHERVOX_GOVERNOR_SUCCESS) ? 0.8f : 0.7f;
    ethervox_result_t mem_result =
        ethervox_memory_store_add(g_memory_store, response, tags, 1, importance, false, &memory_id);
    if (ethervox_is_error(mem_result)) {
      LOGW("Failed to store assistant response to memory: Error %d", mem_result);
    } else {
      LOGI("Stored assistant response to memory (ID: %llu)", (unsigned long long)memory_id);
    }
  }

  // Format response with confidence placeholder for backwards compatibility
  char formatted_response[4096];
  snprintf(formatted_response, sizeof(formatted_response), "%s|100|false", response);

  jstring result = create_jstring(env, formatted_response);
  free(response);
  if (error)
    free(error);

  return result;
}

// Global callback context for streaming
typedef struct {
  JNIEnv* env;
  jobject callback_obj;
  jmethodID on_token_method;
  jmethodID on_complete_method;
  jmethodID on_error_method;
  jmethodID on_governor_progress_method;  // New: Governor progress callback
  bool conversation_ended;
  char utf8_buffer[4];  // Buffer for incomplete UTF-8 sequences (max 4 bytes)
  int utf8_buffer_len;  // Number of bytes currently in buffer
  
  // Tool tracking for memory storage
  char tools_called[16][64];  // Track up to 16 tools
  uint32_t tools_count;       // Number of tools used
  
  // Memory storage flag
  bool skip_memory_storage;   // If true, don't store this conversation turn to memory
} jni_stream_context_t;

// Native callback function that will be called from C
static void native_token_callback(const char* token, void* user_data) {
  jni_stream_context_t* ctx = (jni_stream_context_t*)user_data;
  if (!ctx || !ctx->env || !ctx->callback_obj) {
    return;
  }

  // Guard against NULL token
  if (!token) {
    token = "(null token)";
  }

  // Combine buffered incomplete UTF-8 bytes with new token
  size_t token_len = strlen(token);
  size_t combined_len = ctx->utf8_buffer_len + token_len;
  char* combined = malloc(combined_len + 1);
  if (!combined) {
    return;
  }

  // Copy buffered bytes + new token
  if (ctx->utf8_buffer_len > 0) {
    memcpy(combined, ctx->utf8_buffer, ctx->utf8_buffer_len);
  }
  memcpy(combined + ctx->utf8_buffer_len, token, token_len);
  combined[combined_len] = '\0';

  // Validate and find how much is valid UTF-8
  size_t valid_len = 0;
  bool is_valid = validate_and_fix_utf8(combined, &valid_len);

  if (!is_valid && valid_len < combined_len) {
    // There are incomplete bytes at the end - buffer them for next token
    ctx->utf8_buffer_len = combined_len - valid_len;
    memcpy(ctx->utf8_buffer, combined + valid_len, ctx->utf8_buffer_len);
  } else {
    // Everything is valid or we used all bytes
    ctx->utf8_buffer_len = 0;
  }

  // Send only the valid portion to Java
  if (valid_len > 0) {
    char* valid_str = malloc(valid_len + 1);
    if (valid_str) {
      memcpy(valid_str, combined, valid_len);
      valid_str[valid_len] = '\0';

      jstring j_token = (*ctx->env)->NewStringUTF(ctx->env, valid_str);
      (*ctx->env)->CallVoidMethod(ctx->env, ctx->callback_obj, ctx->on_token_method, j_token);
      (*ctx->env)->DeleteLocalRef(ctx->env, j_token);

      free(valid_str);
    }
  }

  free(combined);
}

// Governor progress callback function
static void native_governor_progress_callback(ethervox_governor_event_type_t event_type,
                                              const char* message, void* user_data) {
  jni_stream_context_t* ctx = (jni_stream_context_t*)user_data;
  if (!ctx || !ctx->env || !ctx->callback_obj || !ctx->on_governor_progress_method) {
    return;  // Callback not set or not available
  }

  // Guard against NULL message
  if (!message) {
    message = "(null message)";
  }

  // Map event type to string
  const char* event_str;
  switch (event_type) {
    case ETHERVOX_GOVERNOR_EVENT_ITERATION_START:
      event_str = "ITERATION_START";
      break;
    case ETHERVOX_GOVERNOR_EVENT_THINKING:
      event_str = "THINKING";
      break;
    case ETHERVOX_GOVERNOR_EVENT_TOOL_CALL:
      event_str = "TOOL_CALL";
      // Extract tool name from message format: "Calling tool: <tool_name>"
      if (message && strstr(message, "Calling tool: ") && ctx->tools_count < 16) {
        const char* tool_start = message + 14;  // Skip "Calling tool: "
        size_t tool_len = strlen(tool_start);
        if (tool_len > 0 && tool_len < 64) {
          snprintf(ctx->tools_called[ctx->tools_count], 64, "%s", tool_start);
          ctx->tools_count++;
          LOGI("Tracked tool call: %s (total: %u)", tool_start, ctx->tools_count);
        }
      }
      break;
    case ETHERVOX_GOVERNOR_EVENT_TOOL_RESULT:
      event_str = "TOOL_RESULT";
      break;
    case ETHERVOX_GOVERNOR_EVENT_TOOL_ERROR:
      event_str = "TOOL_ERROR";
      break;
    case ETHERVOX_GOVERNOR_EVENT_CONFIDENCE_UPDATE:
      event_str = "CONFIDENCE_UPDATE";
      break;
    case ETHERVOX_GOVERNOR_EVENT_CONTEXT_SUMMARIZING:
      event_str = "CONTEXT_SUMMARIZING";
      break;
    case ETHERVOX_GOVERNOR_EVENT_CONTEXT_CLEARED:
      event_str = "CONTEXT_CLEARED";
      break;
    case ETHERVOX_GOVERNOR_EVENT_MANIFEST_LOADING:
      event_str = "MANIFEST_LOADING";
      break;
    case ETHERVOX_GOVERNOR_EVENT_MANIFEST_READY:
      event_str = "MANIFEST_READY";
      break;
    case ETHERVOX_GOVERNOR_EVENT_MANIFEST_FALLBACK_LEVEL_1:
      event_str = "MANIFEST_FALLBACK_LEVEL_1";
      break;
    case ETHERVOX_GOVERNOR_EVENT_MANIFEST_FALLBACK_LEVEL_2:
      event_str = "MANIFEST_FALLBACK_LEVEL_2";
      break;
    case ETHERVOX_GOVERNOR_EVENT_COMPLETE:
      event_str = "COMPLETE";
      // Also call onComplete callback when governor finishes
      if (ctx->on_complete_method) {
        // Create Java String array for tools
        jclass stringClass = (*ctx->env)->FindClass(ctx->env, "java/lang/String");
        jobjectArray toolsArray = (*ctx->env)->NewObjectArray(ctx->env, ctx->tools_count, stringClass, NULL);
        
        // Fill array with tool names
        for (uint32_t i = 0; i < ctx->tools_count; i++) {
          jstring toolName = (*ctx->env)->NewStringUTF(ctx->env, ctx->tools_called[i]);
          (*ctx->env)->SetObjectArrayElement(ctx->env, toolsArray, i, toolName);
          (*ctx->env)->DeleteLocalRef(ctx->env, toolName);
        }
        
        // Call onComplete with tools array
        (*ctx->env)->CallVoidMethod(ctx->env, ctx->callback_obj, ctx->on_complete_method,
                                    (jboolean)ctx->conversation_ended, toolsArray);
        (*ctx->env)->DeleteLocalRef(ctx->env, toolsArray);
      }
      break;
    default:
      event_str = "UNKNOWN";
  }

  __android_log_print(ANDROID_LOG_INFO, "EthervoxGovernor", "[%s] %s", event_str, message);

  jstring j_event = create_jstring(ctx->env, event_str);
  jstring j_message = create_jstring(ctx->env, message);
  (*ctx->env)->CallVoidMethod(ctx->env, ctx->callback_obj, ctx->on_governor_progress_method,
                              j_event, j_message);
  (*ctx->env)->DeleteLocalRef(ctx->env, j_event);
  (*ctx->env)->DeleteLocalRef(ctx->env, j_message);
}

// Governor streaming dialogue - uses token_callback for real-time token streaming
JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_processDialogueStreamingNative(
    JNIEnv* env, jobject thiz, jstring user_text, jstring language, jboolean skip_memory_storage, jobject callback) {
  (void)thiz;
  (void)language;

  if (!g_governor) {
    LOGE("Governor not initialized");
    // Call error callback
    jclass callback_class = (*env)->GetObjectClass(env, callback);
    jmethodID on_error =
        (*env)->GetMethodID(env, callback_class, "onError", "(Ljava/lang/String;)V");
    if (on_error) {
      jstring error_msg = create_jstring(env, "Governor not initialized");
      (*env)->CallVoidMethod(env, callback, on_error, error_msg);
      (*env)->DeleteLocalRef(env, error_msg);
    }
    return;
  }

  const char* text = (*env)->GetStringUTFChars(env, user_text, NULL);

  LOGI("Processing dialogue (streaming): '%s' (skip_memory: %s)", 
       text, skip_memory_storage ? "true" : "false");

  // Store user message to memory (before processing) - unless skip flag is set
  // Skip for system/startup prompts that shouldn't appear in conversation history
  if (g_memory_store && !skip_memory_storage) {
    const char* tags[] = {"conversation"};
    uint64_t memory_id;
    ethervox_result_t mem_result =
        ethervox_memory_store_add(g_memory_store, text, tags, 1, 0.8f, true, &memory_id);
    if (ethervox_is_error(mem_result)) {
      LOGW("Failed to store user message to memory: Error %d", mem_result);
    } else {
      LOGI("Stored user message to memory (ID: %llu)", (unsigned long long)memory_id);
    }
  } else if (skip_memory_storage) {
    LOGI("Skipping memory storage for system/startup prompt");
  }

  // Get callback methods
  jclass callback_class = (*env)->GetObjectClass(env, callback);
  jmethodID on_token = (*env)->GetMethodID(env, callback_class, "onToken", "(Ljava/lang/String;)V");
  jmethodID on_complete = (*env)->GetMethodID(env, callback_class, "onComplete", "(Z[Ljava/lang/String;)V");
  jmethodID on_error = (*env)->GetMethodID(env, callback_class, "onError", "(Ljava/lang/String;)V");
  jmethodID on_governor_progress = (*env)->GetMethodID(env, callback_class, "onGovernorProgress",
                                                       "(Ljava/lang/String;Ljava/lang/String;)V");

  // Setup streaming context
  jni_stream_context_t stream_ctx = {.env = env,
                                     .callback_obj = callback,
                                     .on_token_method = on_token,
                                     .on_complete_method = on_complete,
                                     .on_error_method = on_error,
                                     .on_governor_progress_method = on_governor_progress,
                                     .conversation_ended = false,
                                     .utf8_buffer_len = 0,
                                     .tools_count = 0,
                                     .skip_memory_storage = skip_memory_storage};
  // Zero out tools_called array
  memset(stream_ctx.tools_called, 0, sizeof(stream_ctx.tools_called));

  // N6.4: Create a fresh cancel token for this generation
  if (g_cancel_token) {
    ethervox_cancel_token_free(g_cancel_token);
    g_cancel_token = NULL;
  }
  ethervox_result_t token_result = ethervox_cancel_token_create(&g_cancel_token);
  if (ethervox_is_error(token_result)) {
    LOGE("Failed to create cancel token");
    (*env)->ReleaseStringUTFChars(env, user_text, text);
    return;
  }

  // Execute with streaming token callback
  char* response = NULL;
  char* error = NULL;
  ethervox_governor_status_t status =
      ethervox_governor_execute(g_governor, text, g_cancel_token, &response, &error,
                                NULL,                               // metrics (optional)
                                native_governor_progress_callback,  // progress callback
                                NULL,                               // event callback (N6.1 - not used yet)
                                native_token_callback,              // token callback for streaming
                                &stream_ctx                         // user data
      );

  (*env)->ReleaseStringUTFChars(env, user_text, text);

  if (status != ETHERVOX_GOVERNOR_SUCCESS || !response) {
    LOGE("Failed to process query: %s", error ? error : "Unknown error");
    if (on_error) {
      jstring error_msg = create_jstring(env, error ? error : "Failed to generate response");
      (*env)->CallVoidMethod(env, callback, on_error, error_msg);
      (*env)->DeleteLocalRef(env, error_msg);
    }
    if (response)
      free(response);
    if (error)
      free(error);
  } else {
    // Store assistant response to memory (after successful processing) with tools
    // Skip if this was a system/startup prompt (indicated by skip_memory_storage flag)
    if (g_memory_store && response && !stream_ctx.skip_memory_storage) {
      const char* tags[] = {"conversation"};
      uint64_t memory_id;
      // Use 0.8f importance for full response, 0.7f if timeout/partial
      float importance = (status == ETHERVOX_GOVERNOR_SUCCESS) ? 0.8f : 0.7f;
      
      // Convert tools_called array to const char* array for the function
      const char* tools_ptr[16];
      for (uint32_t i = 0; i < stream_ctx.tools_count; i++) {
        tools_ptr[i] = stream_ctx.tools_called[i];
      }
      
      // DEBUG: Log tool count and names before storing
      LOGI("[MEMORY] Storing response with %u tools", stream_ctx.tools_count);
      for (uint32_t i = 0; i < stream_ctx.tools_count; i++) {
        LOGI("[MEMORY]   Tool %u: %s", i, stream_ctx.tools_called[i]);
      }
      
      ethervox_result_t mem_result = ethervox_memory_store_add_with_tools(
          g_memory_store, response, tags, 1, importance, false,
          tools_ptr, stream_ctx.tools_count, &memory_id);
      
      if (ethervox_is_error(mem_result)) {
        LOGW("Failed to store assistant response to memory: Error %d", mem_result);
      } else {
        LOGI("Stored assistant response to memory (ID: %llu) with %u tools",
             (unsigned long long)memory_id, stream_ctx.tools_count);
      }
    }

    // NOTE: onComplete is already called via native_governor_progress_callback
    // from inside ethervox_governor_execute when it sends ETHERVOX_GOVERNOR_EVENT_COMPLETE.
    // DO NOT call it again here, as that causes a double-invocation and crashes.
    // Just clean up the response memory.
    free(response);
    if (error)
      free(error);
  }

  LOGI("Streaming dialogue complete");
}

/**
 * Cancel ongoing LLM processing (N6.4)
 * Cancels the active cancel token, stopping generation within 200ms
 */
JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_cancelProcessing(JNIEnv* env,
                                                                                jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_cancel_token) {
    LOGW("cancelProcessing: No active cancel token");
    return;
  }

  ethervox_cancel_token_cancel(g_cancel_token);
  LOGI("cancelProcessing: Generation cancelled via token");
}

// DEPRECATED: Language handling is now done by the governor's language detection
JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_setDialogueLanguage(
    JNIEnv* env, jobject thiz, jstring language) {
  (void)thiz;
  (void)language;

  const char* lang = (*env)->GetStringUTFChars(env, language, NULL);
  LOGW("setDialogueLanguage(%s) deprecated - language detection automatic", lang);
  (*env)->ReleaseStringUTFChars(env, language, lang);
  return 0;  // Success for compatibility
}

// DEPRECATED: Returns default language for compatibility
JNIEXPORT jstring JNICALL
Java_com_droid_ethervox_1core_NativeLib_getDialogueLanguage(JNIEnv* env, jobject thiz) {
  (void)thiz;

  LOGW("getDialogueLanguage() deprecated - returning default 'en'");
  return create_jstring(env, "en");
}

// ===========================================================================
// Test/Utility Functions
// ===========================================================================

JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_stringFromJNI(JNIEnv* env,
                                                                                jobject thiz) {
  (void)thiz;

  const char* hello = "Hello from EthervoxAI native library!";
  LOGI("%s", hello);
  return create_jstring(env, hello);
}

JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_getVersion(JNIEnv* env,
                                                                             jobject thiz) {
  (void)thiz;
  return create_jstring(env, ETHERVOX_VERSION_STRING);
}

JNIEXPORT jstring JNICALL Java_com_droid_ethervox_1core_NativeLib_getBackendVersion(JNIEnv* env,
                                                                                    jobject thiz) {
  (void)thiz;
  // ETHERVOX_BACKEND_VERSION is guaranteed to be defined by CMake
  return create_jstring(env, ETHERVOX_BACKEND_VERSION);
}

JNIEXPORT jstring JNICALL
Java_com_droid_ethervox_1core_NativeLib_getActiveTimerStatus(JNIEnv* env, jobject thiz) {
  (void)thiz;

  char* status = ethervox_timer_get_active_status();
  if (!status) {
    return create_jstring(env, "{\"has_timer\": false}");
  }

  jstring result = create_jstring(env, status);
  free(status);
  return result;
}

JNIEXPORT jobject JNICALL
Java_com_droid_ethervox_1core_NativeLib_getDefaultLlmConfig(JNIEnv* env, jobject thiz) {
  (void)thiz;

  // Return default LLM config values directly
  ethervox_llm_config_t config = {
      .temperature = 0.7f, .max_tokens = 512, .top_p = 0.9f, .context_length = 2048};

  // Find the LlmConfig class
  jclass llmConfigClass = (*env)->FindClass(env, "com/droid/ethervox_core/LlmConfig");
  if (llmConfigClass == NULL) {
    return NULL;
  }

  // Get the constructor (Float, Int, Float, Int)
  jmethodID constructor = (*env)->GetMethodID(env, llmConfigClass, "<init>", "(FIFI)V");
  if (constructor == NULL) {
    (*env)->DeleteLocalRef(env, llmConfigClass);
    return NULL;
  }

  // Create the object
  jobject llmConfigObj =
      (*env)->NewObject(env, llmConfigClass, constructor, (jfloat)config.temperature,
                        (jint)config.max_tokens, (jfloat)config.top_p, (jint)config.context_length);

  (*env)->DeleteLocalRef(env, llmConfigClass);
  return llmConfigObj;
}

JNIEXPORT jstring JNICALL
Java_com_droid_ethervox_1core_NativeLib_getDefaultStartupPrompt(JNIEnv* env, jobject thiz) {
  (void)thiz;
  return (*env)->NewStringUTF(env, DEFAULT_STARTUP_PROMPT);
}

JNIEXPORT jobject JNICALL
Java_com_droid_ethervox_1core_NativeLib_getDefaultGovernorConfig(JNIEnv* env, jobject thiz) {
  (void)thiz;

  // Get adaptive defaults from device profiling
  int optimal_batch = ethervox_device_profile_get_optimal_batch_size();
  int optimal_threads = ethervox_device_profile_get_optimal_threads();

  // Find the GovernorConfig class
  jclass configClass = (*env)->FindClass(env, "com/droid/ethervox_core/GovernorConfig");
  if (configClass == NULL) {
    return NULL;
  }

  // Get the constructor (Int, Int, Int, Float, Int)
  jmethodID constructor = (*env)->GetMethodID(env, configClass, "<init>", "(IIIFI)V");
  if (constructor == NULL) {
    (*env)->DeleteLocalRef(env, configClass);
    return NULL;
  }

  // Create the config object with current runtime values
  jobject configObj = (*env)->NewObject(
      env, configClass, constructor,
      (jint)optimal_batch,                           // batchSize (adaptive)
      (jint)ETHERVOX_GOVERNOR_STARTUP_BATCH_SIZE,   // startupBatchSize
      (jint)ETHERVOX_GOVERNOR_CONTEXT_SIZE,         // contextSize
      (jfloat)ETHERVOX_GOVERNOR_TEMPERATURE,        // temperature
      (jint)optimal_threads                          // threads (adaptive)
  );

  (*env)->DeleteLocalRef(env, configClass);
  return configObj;
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_updateGovernorParams(
    JNIEnv* env, jobject thiz,
    jint batch_size, jint startup_batch_size, jint context_size,
    jfloat temperature, jint threads, jint max_tokens_per_response) {
  (void)env;
  (void)thiz;

  // Validate parameters
  if (batch_size < 64 || batch_size > 2048) {
    LOGE("Invalid batch_size: %d (must be 64-2048)", batch_size);
    return JNI_FALSE;
  }
  if (startup_batch_size < 32 || startup_batch_size > 512) {
    LOGE("Invalid startup_batch_size: %d (must be 32-512)", startup_batch_size);
    return JNI_FALSE;
  }
  if (context_size < 2048 || context_size > 32768) {
    LOGE("Invalid context_size: %d (must be 2048-32768)", context_size);
    return JNI_FALSE;
  }
  if (temperature < 0.0f || temperature > 2.0f) {
    LOGE("Invalid temperature: %.2f (must be 0.0-2.0)", temperature);
    return JNI_FALSE;
  }
  if (threads < 1 || threads > 16) {
    LOGE("Invalid threads: %d (must be 1-16)", threads);
    return JNI_FALSE;
  }
  if (max_tokens_per_response < 20 || max_tokens_per_response > 4096) {
    LOGE("Invalid max_tokens_per_response: %d (must be 20-4096)", max_tokens_per_response);
    return JNI_FALSE;
  }

  // Update runtime configuration
  g_governor_config.batch_size = batch_size;
  g_governor_config.startup_batch_size = startup_batch_size;
  g_governor_config.context_size = context_size;
  g_governor_config.temperature = temperature;
  g_governor_config.threads = threads;
  g_governor_config.max_tokens_per_response = max_tokens_per_response;

  LOGI("Governor params updated: batch=%d, startup_batch=%d, ctx=%d, temp=%.2f, threads=%d, max_tokens=%d",
       batch_size, startup_batch_size, context_size, temperature, threads, max_tokens_per_response);

  // NOTE: These parameters will be applied on next model load or Governor re-initialization
  // Some parameters (like context_size) require model reload to take effect

  return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_droid_ethervox_1core_NativeLib_getSupportedLanguages(JNIEnv* env, jobject thiz) {
  (void)thiz;

  // Return supported languages directly
  static const char* languages[] = {"en", "es", "fr", "de", "it", "pt", "zh", "ja", "ko", NULL};

  // Count languages
  int count = 0;
  while (languages[count] != NULL) {
    count++;
  }

  // Create Java String array
  jclass stringClass = (*env)->FindClass(env, "java/lang/String");
  jobjectArray languageArray = (*env)->NewObjectArray(env, count, stringClass, NULL);

  // Populate array with language codes
  for (int i = 0; i < count; i++) {
    jstring langCode = create_jstring(env, languages[i]);
    (*env)->SetObjectArrayElement(env, languageArray, i, langCode);
    (*env)->DeleteLocalRef(env, langCode);
  }

  return languageArray;
}

// ===========================================================================
// Debug Mode Control
// ===========================================================================

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_setMemoryStorageDir(
    JNIEnv* env, jobject thiz, jstring storage_dir) {
  (void)thiz;

  if (!g_memory_store) {
    LOGE("Memory store not initialized");
    return;
  }

  const char* dir_path = (*env)->GetStringUTFChars(env, storage_dir, NULL);
  if (!dir_path) {
    LOGE("Failed to get storage directory path");
    return;
  }

  // Re-initialize memory store with storage directory
  // This will enable file persistence
  ethervox_result_t mem_init_result = ethervox_memory_init(g_memory_store, NULL, dir_path);
  if (ethervox_is_error(mem_init_result)) {
    LOGE("Failed to set memory storage directory: %s", dir_path);
    (*env)->ReleaseStringUTFChars(env, storage_dir, dir_path);
    return;
  }

  LOGI("Memory storage directory set to: %s", g_memory_store->storage_filepath);

  // Use platform-agnostic function to load previous session
  // This handles all the complexity of finding the most recent file,
  // preserving tags, IDs, and adding the "imported" tag
  uint32_t turns_loaded = 0;
  ethervox_result_t load_result =
      ethervox_memory_load_previous_session(g_memory_store, &turns_loaded);
  if (ethervox_is_success(load_result) && turns_loaded > 0) {
    LOGI("Memory: Loaded %u previous memories from last session", turns_loaded);
  }

  (*env)->ReleaseStringUTFChars(env, storage_dir, dir_path);
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_setDebugMode(JNIEnv* env,
                                                                            jobject thiz,
                                                                            jboolean enabled) {
  g_ethervox_debug_enabled = enabled ? 1 : 0;
  LOGI("Debug mode %s", g_ethervox_debug_enabled ? "enabled" : "disabled");
}

JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_getDebugMode(JNIEnv* env,
                                                                                jobject thiz) {
  return (jboolean)(g_ethervox_debug_enabled ? JNI_TRUE : JNI_FALSE);
}

// C callback that forwards logs to Java
/**
 * Sanitize UTF-8 string to be compatible with Java's Modified UTF-8
 * Replaces invalid or problematic bytes with '?'
 */
static void sanitize_utf8_for_java(char* buffer, size_t max_len) {
  if (!buffer)
    return;

  size_t i = 0;
  while (i < max_len && buffer[i] != '\0') {
    unsigned char c = buffer[i];

    // Check for invalid UTF-8 sequences
    if (c >= 0x80) {
      // Multi-byte UTF-8 sequence
      int bytes = 0;
      if ((c & 0xE0) == 0xC0)
        bytes = 2;
      else if ((c & 0xF0) == 0xE0)
        bytes = 3;
      else if ((c & 0xF8) == 0xF0)
        bytes = 4;

      // Verify continuation bytes
      bool valid = true;
      for (int j = 1; j < bytes && (i + j) < max_len; j++) {
        if ((buffer[i + j] & 0xC0) != 0x80) {
          valid = false;
          break;
        }
      }

      // Replace invalid sequences with '?'
      if (!valid || bytes == 0) {
        buffer[i] = '?';
        i++;
      } else {
        // Valid UTF-8 sequence, but Java Modified UTF-8 doesn't support 4-byte sequences
        // Replace 4-byte sequences with '?'
        if (bytes == 4) {
          buffer[i] = '?';
          for (int j = 1; j < bytes && (i + j) < max_len; j++) {
            memmove(&buffer[i + 1], &buffer[i + bytes], strlen(&buffer[i + bytes]) + 1);
          }
        } else {
          i += bytes;
        }
      }
    } else {
      i++;
    }
  }
}

// JNI log callback wrapper for C4.3 structured logging
static void java_log_callback_wrapper(const ethervox_log_entry_t* entry, void* user_data) {
  (void)user_data;  // Unused
  
  if (!g_jvm || !g_log_callback_obj || !g_log_callback_method || !entry) {
    return;
  }

  // Map ethervox_log_level_t to Android log priority
  int android_level = ANDROID_LOG_INFO;
  switch (entry->level) {
    case ETHERVOX_LOG_LEVEL_TRACE:   android_level = ANDROID_LOG_VERBOSE; break;
    case ETHERVOX_LOG_LEVEL_DEBUG:   android_level = ANDROID_LOG_DEBUG; break;
    case ETHERVOX_LOG_LEVEL_INFO:    android_level = ANDROID_LOG_INFO; break;
    case ETHERVOX_LOG_LEVEL_WARN:    android_level = ANDROID_LOG_WARN; break;
    case ETHERVOX_LOG_LEVEL_ERROR:   android_level = ANDROID_LOG_ERROR; break;
    case ETHERVOX_LOG_LEVEL_FATAL:   android_level = ANDROID_LOG_FATAL; break;
    case ETHERVOX_LOG_LEVEL_OFF:     android_level = ANDROID_LOG_SILENT; break;
  }
  
  // Build tag from subsystem (e.g., "llm", "governor", "audio")
  const char* tag = entry->func ? entry->func : "EthervoxCore";
  const char* message = entry->message ? entry->message : "";

  JNIEnv* env = NULL;
  jint result = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);

  bool detach = false;
  if (result == JNI_EDETACHED) {
    if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK) {
      return;
    }
    detach = true;
  }

  // Sanitize strings for Java Modified UTF-8 compatibility
  char safe_tag[256];
  char safe_message[4096];
  strncpy(safe_tag, tag, sizeof(safe_tag) - 1);
  safe_tag[sizeof(safe_tag) - 1] = '\0';
  strncpy(safe_message, message, sizeof(safe_message) - 1);
  safe_message[sizeof(safe_message) - 1] = '\0';

  sanitize_utf8_for_java(safe_tag, sizeof(safe_tag));
  sanitize_utf8_for_java(safe_message, sizeof(safe_message));

  jstring jTag = (*env)->NewStringUTF(env, safe_tag);
  jstring jMessage = (*env)->NewStringUTF(env, safe_message);

  if (jTag && jMessage) {
    (*env)->CallVoidMethod(env, g_log_callback_obj, g_log_callback_method, android_level, jTag, jMessage);
  }

  if (jTag)
    (*env)->DeleteLocalRef(env, jTag);
  if (jMessage)
    (*env)->DeleteLocalRef(env, jMessage);

  if (detach) {
    (*g_jvm)->DetachCurrentThread(g_jvm);
  }
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_setLogCallback(JNIEnv* env,
                                                                              jobject thiz,
                                                                              jobject callback) {
  // Store JavaVM reference if not already stored
  if (!g_jvm) {
    (*env)->GetJavaVM(env, &g_jvm);
  }

  // Clear old callback
  if (g_log_callback_obj) {
    (*env)->DeleteGlobalRef(env, g_log_callback_obj);
    g_log_callback_obj = NULL;
    g_log_callback_method = NULL;
    g_ethervox_log_callback = NULL;
  }

  // Register new callback
  if (callback) {
    g_log_callback_obj = (*env)->NewGlobalRef(env, callback);

    jclass callbackClass = (*env)->GetObjectClass(env, callback);
    g_log_callback_method = (*env)->GetMethodID(env, callbackClass, "onLog",
                                                "(ILjava/lang/String;Ljava/lang/String;)V");

    if (g_log_callback_method) {
      g_ethervox_log_callback = java_log_callback_wrapper;
      LOGI("Log callback registered successfully");
    } else {
      LOGE("Failed to find onLog method in callback object");
      (*env)->DeleteGlobalRef(env, g_log_callback_obj);
      g_log_callback_obj = NULL;
    }
  } else {
    LOGI("Log callback cleared");
  }
}

JNIEXPORT jobject JNICALL
Java_com_droid_ethervox_1core_NativeLib_getLlamaPerformanceMetrics(JNIEnv* env, jobject thiz) {
  (void)thiz;

#if ETHERVOX_WITH_LLAMA && LLAMA_CPP_AVAILABLE
  // Get Governor
  if (!g_governor) {
    return NULL;
  }

  // Access the llama_context from Governor
  // Note: This requires accessing the internal structure
  // The governor structure has llm_ctx field
  struct ethervox_governor {
    ethervox_governor_config_t config;
    ethervox_tool_registry_t* tool_registry;
    struct llama_model* llm_model;
    struct llama_context* llm_ctx;
    // ... other fields we don't need
  };

  struct ethervox_governor* governor = (struct ethervox_governor*)g_governor;

  if (!governor->llm_ctx) {
    return NULL;
  }

  // Get performance data from llama.cpp
  struct llama_perf_context_data perf = llama_perf_context(governor->llm_ctx);

  // Only return metrics if there's actual data (model has been used)
  // If no tokens have been processed yet, return NULL
  if (perf.n_p_eval == 0 && perf.n_eval == 0) {
    return NULL;
  }

  // Create Java object to return metrics
  jclass metricsClass = (*env)->FindClass(env, "com/droid/ethervox_core/LlamaPerformanceMetrics");
  if (!metricsClass) {
    LOGE("Failed to find LlamaPerformanceMetrics class");
    return NULL;
  }

  jmethodID constructor = (*env)->GetMethodID(env, metricsClass, "<init>", "(DDDDDIII)V");
  if (!constructor) {
    LOGE("Failed to find LlamaPerformanceMetrics constructor");
    return NULL;
  }

  // Calculate tokens per second
  double prompt_tps =
      perf.n_p_eval > 0 && perf.t_p_eval_ms > 0 ? (perf.n_p_eval * 1000.0 / perf.t_p_eval_ms) : 0.0;
  double gen_tps =
      perf.n_eval > 0 && perf.t_eval_ms > 0 ? (perf.n_eval * 1000.0 / perf.t_eval_ms) : 0.0;

  // Debug logging (commented out - too verbose)
  // LOGI("llama_perf: n_eval=%d, t_eval_ms=%.2f, gen_tps=%.2f",
  //      (int)perf.n_eval, perf.t_eval_ms, gen_tps);

  jobject metrics = (*env)->NewObject(env, metricsClass, constructor, perf.t_load_ms,
                                      perf.t_p_eval_ms, perf.t_eval_ms, prompt_tps, gen_tps,
                                      (jint)perf.n_p_eval, (jint)perf.n_eval, (jint)perf.n_reused);

  return metrics;
#else
  return NULL;
#endif
}

// ===========================================================================
// Voice Tools / Transcription
// ===========================================================================

// Global voice session state for transcription
static ethervox_voice_session_t* g_voice_session = NULL;

// Defined below, alongside the rest of the Mode 1/4 conversation JNI glue;
// forward-declared here so the transcription-segment bridge (which needs
// the same attach/detach pattern) can be defined before them.
static JNIEnv* conversation_attach_env(bool* needs_detach);
static void conversation_detach_env(bool needs_detach);

// TASK-N7.8: JNI bridge for streaming transcription-segment events
// (TASK-C3.5's ethervox_voice_tools_set_event_callback(), never previously
// wired to Kotlin - the transport existed, nothing on the Java side called
// it). Mirrors the g_conversation_callback_obj pattern above: a global ref
// to the Kotlin callback, invoked from whichever native thread the event
// arrives on (the transcription dispatch thread - see voice_tools.c) via
// attach/detach, matching conversation_attach_env()/conversation_detach_env().
static jobject g_transcription_segment_callback_obj = NULL;
static jmethodID g_transcription_on_segment = NULL;

static bool jni_transcription_on_segment(const ethervox_event_t* event, void* user_data) {
  (void)user_data;
  if (!event || event->type != ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT) return true;
  if (!g_transcription_segment_callback_obj || !g_transcription_on_segment) return true;

  bool detach = false;
  JNIEnv* env = conversation_attach_env(&detach);
  if (!env) return true;

  const ethervox_event_transcription_segment_t* seg = &event->transcription_segment;
  jstring jText = create_jstring(env, seg->text ? seg->text : "");
  (*env)->CallVoidMethod(env, g_transcription_segment_callback_obj, g_transcription_on_segment,
                         (jint)seg->segment_id, (jint)seg->speaker_id, (jboolean)seg->is_final,
                         jText);
  if (jText) (*env)->DeleteLocalRef(env, jText);
  conversation_detach_env(detach);
  return true;
}

/**
 * Get Android-specific files directory (set from Kotlin)
 * Returns NULL if not on Android or not set
 */
const char* ethervox_get_android_files_dir(void) {
#if defined(__ANDROID__)
  if (g_android_files_dir[0] != '\0') {
    return g_android_files_dir;
  }
#endif
  return NULL;
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_setAndroidFilesDir(
    JNIEnv* env, jobject thiz, jstring filesDir, jstring cacheDir) {
  (void)thiz;

  if (!filesDir) {
    LOGE("Files directory is NULL");
    return;
  }

  // Get files directory string
  const char* files = (*env)->GetStringUTFChars(env, filesDir, NULL);
  if (!files) {
    LOGE("Failed to get files directory string");
    return;
  }

  // Get cache directory string (optional for backward compatibility)
  const char* cache = NULL;
  if (cacheDir) {
    cache = (*env)->GetStringUTFChars(env, cacheDir, NULL);
  }

  // Copy to deprecated g_android_files_dir for backward compatibility
  strncpy(g_android_files_dir, files, sizeof(g_android_files_dir) - 1);
  g_android_files_dir[sizeof(g_android_files_dir) - 1] = '\0';

  // Initialize g_android_paths structure (N6.1)
  // Use g_paths_buffer to store the strings so they persist
  size_t offset = 0;
  
  // data_dir = filesDir
  size_t files_len = strlen(files);
  if (offset + files_len + 1 < sizeof(g_paths_buffer)) {
    strncpy(g_paths_buffer + offset, files, sizeof(g_paths_buffer) - offset - 1);
    g_android_paths.data_dir = g_paths_buffer + offset;
    offset += files_len + 1;
  }

  // cache_dir = cacheDir (or filesDir/cache if not provided)
  if (cache) {
    size_t cache_len = strlen(cache);
    if (offset + cache_len + 1 < sizeof(g_paths_buffer)) {
      strncpy(g_paths_buffer + offset, cache, sizeof(g_paths_buffer) - offset - 1);
      g_android_paths.cache_dir = g_paths_buffer + offset;
      offset += cache_len + 1;
    }
  } else {
    // Fallback: use filesDir/cache
    char cache_fallback[512];
    snprintf(cache_fallback, sizeof(cache_fallback), "%s/cache", files);
    size_t fallback_len = strlen(cache_fallback);
    if (offset + fallback_len + 1 < sizeof(g_paths_buffer)) {
      strncpy(g_paths_buffer + offset, cache_fallback, sizeof(g_paths_buffer) - offset - 1);
      g_android_paths.cache_dir = g_paths_buffer + offset;
      offset += fallback_len + 1;
    }
  }

  // models_dir = filesDir/models
  char models_path[512];
  snprintf(models_path, sizeof(models_path), "%s/models", files);
  size_t models_len = strlen(models_path);
  if (offset + models_len + 1 < sizeof(g_paths_buffer)) {
    strncpy(g_paths_buffer + offset, models_path, sizeof(g_paths_buffer) - offset - 1);
    g_android_paths.models_dir = g_paths_buffer + offset;
    offset += models_len + 1;
  }

  // temp_dir = cacheDir/tmp (or filesDir/tmp if cache not provided)
  char temp_path[512];
  if (cache) {
    snprintf(temp_path, sizeof(temp_path), "%s/tmp", cache);
  } else {
    snprintf(temp_path, sizeof(temp_path), "%s/tmp", files);
  }
  size_t temp_len = strlen(temp_path);
  if (offset + temp_len + 1 < sizeof(g_paths_buffer)) {
    strncpy(g_paths_buffer + offset, temp_path, sizeof(g_paths_buffer) - offset - 1);
    g_android_paths.temp_dir = g_paths_buffer + offset;
    offset += temp_len + 1;
  }

  g_paths_initialized = true;

  LOGI("[Android] Paths configured:");
  LOGI("  data_dir:   %s", g_android_paths.data_dir);
  LOGI("  cache_dir:  %s", g_android_paths.cache_dir);
  LOGI("  models_dir: %s", g_android_paths.models_dir);
  LOGI("  temp_dir:   %s", g_android_paths.temp_dir);
  LOGI("[Android] Granite Speech models: %s/%s/", g_android_paths.models_dir,
       ETHERVOX_GRANITE_SPEECH_SUBDIR);

  (*env)->ReleaseStringUTFChars(env, filesDir, files);
  if (cache) {
    (*env)->ReleaseStringUTFChars(env, cacheDir, cache);
  }
}

JNIEXPORT jint JNICALL
Java_com_droid_ethervox_1core_NativeLib_startVoiceTranscription(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;

  LOGI("[Voice] ========================================");
  LOGI("[Voice] startVoiceTranscription called");
  LOGI("[Voice] ========================================");

  // Initialize voice session if not already done
  if (!g_voice_session) {
    LOGI("[Voice] Voice session is NULL - initializing for first time...");
    g_voice_session = (ethervox_voice_session_t*)malloc(sizeof(ethervox_voice_session_t));
    if (!g_voice_session) {
      LOGE("[Voice] ERROR: Failed to allocate voice session memory");
      return -1;
    }
    LOGI("[Voice] [OK] Voice session memory allocated");

    // Initialize voice tools (requires memory store)
    if (!g_memory_store) {
      LOGE("[Voice] ERROR: Memory store not initialized - required for voice tools");
      free(g_voice_session);
      g_voice_session = NULL;
      return -1;
    }
    LOGI("[Voice] [OK] Memory store exists");

    LOGI("[Voice] Calling ethervox_voice_tools_init...");
    // N6.3: Pass model pool for budget enforcement
    int ret = ethervox_voice_tools_init(g_voice_session, g_memory_store, g_model_pool);
    if (ret != 0) {
      LOGE("[Voice] [FAIL] ERROR: Failed to initialize voice tools: %d", ret);
      LOGE("[Voice] This usually means the Granite Speech Plus model was not found!");
      LOGE("[Voice] Check Settings → Granite Speech Plus Model section to download");
      free(g_voice_session);
      g_voice_session = NULL;
      return ret;
    }
    LOGI("[Voice] [OK] Voice tools initialized successfully!");
    LOGI("[Voice] [OK] Granite Speech Plus model loaded and ready");
  } else {
    LOGI("[Voice] Voice session already initialized (is_initialized=%d)",
         g_voice_session->is_initialized);
  }

  // Verify session is properly initialized
  if (!g_voice_session->is_initialized) {
    LOGE("[Voice] ERROR: Voice session exists but is_initialized=false!");
    LOGE("[Voice] This shouldn't happen - initialization must have failed silently");
    return -1;
  }
  LOGI("[Voice] [OK] Session verification passed");

  // TASK-N7.8: (re-)wire the streaming-segment callback onto this session.
  // ethervox_voice_tools_init() memset()s the whole struct including
  // transcription_event_cb, so a callback registered via
  // registerVoiceTranscriptionCallback() before the very first
  // startVoiceTranscription() call (when g_voice_session was still NULL)
  // needs to be re-applied here once the session actually exists.
  if (g_transcription_segment_callback_obj) {
    ethervox_voice_tools_set_event_callback(g_voice_session, jni_transcription_on_segment, NULL);
  }

  // Start listening (recording + VAD)
  LOGI("[Voice] Starting voice recording...");
  int ret = ethervox_voice_tools_start_listen(g_voice_session);
  if (ret != 0) {
    LOGE("[Voice] [FAIL] ERROR: Failed to start voice recording: %d", ret);
    return ret;
  }

  LOGI("[Voice] [OK] Voice transcription started successfully!");
  LOGI("[Voice] ========================================");
  return 0;
}

/**
 * TASK-N7.8: register/unregister a Kotlin listener for streaming
 * transcription-segment events (TASK-C3.5's event, dispatched once per
 * finalized chunk during recording - the transport already existed in the
 * native backend; this is what makes it reachable from Kotlin at all).
 * Pass null to unregister. Must be called after startVoiceTranscription()
 * has initialized g_voice_session at least once (it registers on whatever
 * session currently exists and re-registers on every start, since
 * ethervox_voice_tools_init() doesn't reset transcription_event_cb - see
 * that function's own memset(session, 0, ...) at the top, which does clear
 * it, so re-registering after every startVoiceTranscription() call is
 * required, not just defensive).
 */
JNIEXPORT void JNICALL
Java_com_droid_ethervox_1core_NativeLib_registerVoiceTranscriptionCallback(
    JNIEnv* env, jobject thiz, jobject callback) {
  (void)thiz;

  if (g_transcription_segment_callback_obj) {
    (*env)->DeleteGlobalRef(env, g_transcription_segment_callback_obj);
    g_transcription_segment_callback_obj = NULL;
    g_transcription_on_segment = NULL;
  }

  if (!callback) {
    if (g_voice_session) {
      ethervox_voice_tools_set_event_callback(g_voice_session, NULL, NULL);
    }
    return;
  }

  if (!g_jvm) {
    (*env)->GetJavaVM(env, &g_jvm);
  }

  g_transcription_segment_callback_obj = (*env)->NewGlobalRef(env, callback);
  jclass callbackClass = (*env)->GetObjectClass(env, callback);
  g_transcription_on_segment =
      (*env)->GetMethodID(env, callbackClass, "onSegment", "(IIZLjava/lang/String;)V");
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
  }

  if (g_voice_session) {
    ethervox_voice_tools_set_event_callback(g_voice_session, jni_transcription_on_segment, NULL);
  }
}

JNIEXPORT jstring JNICALL
Java_com_droid_ethervox_1core_NativeLib_stopVoiceTranscription(JNIEnv* env, jobject thiz) {
  (void)thiz;

  if (!g_voice_session) {
    LOGE("Voice session not initialized");
    return NULL;
  }

  // Stop listening and get transcript
  const char* transcript = NULL;
  int ret = ethervox_voice_tools_stop_listen(g_voice_session, &transcript);

  if (ret != 0) {
    LOGE("Failed to stop voice transcription: %d", ret);
    return NULL;
  }

  if (!transcript || transcript[0] == '\0') {
    LOGI("Transcription completed but no speech detected");
    return (*env)->NewStringUTF(env, "");
  }

  LOGI("Transcription completed: %s", transcript);

  // CRITICAL: Copy the transcript to Java string immediately
  // The transcript pointer points to g_voice_session->full_transcript buffer
  // which will be reused on the next recording session
  jstring result = create_jstring(env, transcript);

  // Clear the session transcript buffer to prevent it from being returned again
  // This must be done AFTER creating the Java string
  if (g_voice_session->full_transcript) {
    g_voice_session->full_transcript[0] = '\0';
    g_voice_session->transcript_len = 0;
    g_voice_session->segment_count = 0;
  }

  return result;
}

JNIEXPORT jstring JNICALL
Java_com_droid_ethervox_1core_NativeLib_getLastTranscriptFilePath(JNIEnv* env, jobject thiz) {
  (void)thiz;

  if (!g_voice_session) {
    LOGE("Voice session not initialized");
    return NULL;
  }

  if (g_voice_session->last_transcript_file[0] == '\0') {
    LOGI("No transcript file available");
    return NULL;
  }

  return create_jstring(env, g_voice_session->last_transcript_file);
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_isVoiceTranscribing(JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;

  if (!g_voice_session) {
    return JNI_FALSE;
  }

  return g_voice_session->is_recording ? JNI_TRUE : JNI_FALSE;
}

// ===========================================================================
// Voice Conversation (Mode 1: full-duplex conversation, Mode 4: voice-to-text)
//
// Both modes are driven by the same cross-platform orchestrator
// (ethervox_conversation_t, src/dialogue/voice_conversation.c) - the only
// difference is tts_enabled, which controls whether conversation_on_speak()
// hands text to onSpeakRequest (Mode 1) or just reports it via
// onResponseText for display (Mode 4, no audio at all). Only one such
// session can be active at a time (Mode 1 and Mode 4 are mutually
// exclusive, same as Mode 2's Base/Plus exclusivity above).
//
// Callbacks fire from voice_conversation.c's own background pthread, not
// this JNI-calling thread, so every callback below must attach/detach from
// the JVM itself (see conversation_attach_env/conversation_detach_env),
// following the same pattern as java_log_callback_wrapper further down this
// file.
// ===========================================================================

static ethervox_conversation_session_t* g_conversation_session = NULL;
static jobject g_conversation_callback_obj = NULL;
static jmethodID g_conv_on_state_changed = NULL;
static jmethodID g_conv_on_user_transcript = NULL;
static jmethodID g_conv_on_response_text = NULL;
static jmethodID g_conv_on_speak_request = NULL;
static jmethodID g_conv_on_error = NULL;

static JNIEnv* conversation_attach_env(bool* needs_detach) {
  *needs_detach = false;
  if (!g_jvm) {
    return NULL;
  }

  JNIEnv* env = NULL;
  jint result = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
  if (result == JNI_EDETACHED) {
    if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK) {
      return NULL;
    }
    *needs_detach = true;
  }
  return env;
}

static void conversation_detach_env(bool needs_detach) {
  if (needs_detach && g_jvm) {
    (*g_jvm)->DetachCurrentThread(g_jvm);
  }
}

static void jni_conversation_on_state_change(ethervox_conversation_state_t state, void* user_data) {
  (void)user_data;
  if (!g_conversation_callback_obj || !g_conv_on_state_changed) {
    return;
  }

  bool detach = false;
  JNIEnv* env = conversation_attach_env(&detach);
  if (!env) {
    return;
  }

  (*env)->CallVoidMethod(env, g_conversation_callback_obj, g_conv_on_state_changed, (jint)state);
  conversation_detach_env(detach);
}

static void jni_conversation_on_user_transcript(const char* text, const char* language,
                                                 void* user_data) {
  (void)user_data;
  if (!g_conversation_callback_obj || !g_conv_on_user_transcript) {
    return;
  }

  bool detach = false;
  JNIEnv* env = conversation_attach_env(&detach);
  if (!env) {
    return;
  }

  jstring jText = create_jstring(env, text ? text : "");
  jstring jLanguage = language ? create_jstring(env, language) : NULL;
  (*env)->CallVoidMethod(env, g_conversation_callback_obj, g_conv_on_user_transcript, jText,
                         jLanguage);
  if (jText) (*env)->DeleteLocalRef(env, jText);
  if (jLanguage) (*env)->DeleteLocalRef(env, jLanguage);
  conversation_detach_env(detach);
}

static void jni_conversation_on_response_text(const char* text, const char* language,
                                               void* user_data) {
  (void)user_data;
  if (!g_conversation_callback_obj || !g_conv_on_response_text) {
    return;
  }

  bool detach = false;
  JNIEnv* env = conversation_attach_env(&detach);
  if (!env) {
    return;
  }

  jstring jText = create_jstring(env, text ? text : "");
  jstring jLanguage = language ? create_jstring(env, language) : NULL;
  (*env)->CallVoidMethod(env, g_conversation_callback_obj, g_conv_on_response_text, jText,
                         jLanguage);
  if (jText) (*env)->DeleteLocalRef(env, jText);
  if (jLanguage) (*env)->DeleteLocalRef(env, jLanguage);
  conversation_detach_env(detach);
}

// Mode 1 only (tts_enabled=true) - hands the Governor's response text off to
// the platform TTS engine (Android TextToSpeech). Fire-and-forget: Kotlin
// must call voiceConversationNotifySpeakingDone() from its
// UtteranceProgressListener.onDone()/onError() once playback finishes, or
// the conversation thread stalls in SPEAKING until its internal 30s
// safety-net timeout (see ethervox_conversation_notify_speaking_done's doc
// comment in conversation.h).
static void jni_conversation_on_speak_request(const char* text, const char* language,
                                               int speaker_id, bool allow_interrupt,
                                               void* user_data) {
  (void)user_data;
  if (!g_conversation_callback_obj || !g_conv_on_speak_request) {
    return;
  }

  bool detach = false;
  JNIEnv* env = conversation_attach_env(&detach);
  if (!env) {
    return;
  }

  jstring jText = create_jstring(env, text ? text : "");
  jstring jLanguage = language ? create_jstring(env, language) : NULL;
  (*env)->CallVoidMethod(env, g_conversation_callback_obj, g_conv_on_speak_request, jText,
                         jLanguage, (jint)speaker_id, (jboolean)allow_interrupt);
  if (jText) (*env)->DeleteLocalRef(env, jText);
  if (jLanguage) (*env)->DeleteLocalRef(env, jLanguage);
  conversation_detach_env(detach);
}

static void jni_conversation_on_error(const char* message, void* user_data) {
  (void)user_data;
  if (!g_conversation_callback_obj || !g_conv_on_error) {
    return;
  }

  bool detach = false;
  JNIEnv* env = conversation_attach_env(&detach);
  if (!env) {
    return;
  }

  jstring jMessage = create_jstring(env, message ? message : "");
  (*env)->CallVoidMethod(env, g_conversation_callback_obj, g_conv_on_error, jMessage);
  (*env)->DeleteLocalRef(env, jMessage);
  conversation_detach_env(detach);
}

// Shared start/stop logic for both voiceConversationStart (Mode 1) and
// voiceQueryStart (Mode 4); tts_enabled is the only difference between them.
// barge_in_enabled comes from Kotlin's AcousticEchoCanceler.isAvailable()
// check (see NativeLib.kt's doc comments) - the C side does not attempt its
// own AEC capability detection.
static jint start_voice_conversation_session(JNIEnv* env, jobject callback, bool tts_enabled,
                                              bool barge_in_enabled) {
  if (g_conversation_session) {
    LOGW("[VoiceConversation] Session already active - stop it before starting a new one");
    return -1;
  }

  if (!g_governor) {
    LOGE("[VoiceConversation] Governor not initialized");
    return -1;
  }

  if (!g_jvm) {
    (*env)->GetJavaVM(env, &g_jvm);
  }

  g_conversation_callback_obj = (*env)->NewGlobalRef(env, callback);
  jclass callbackClass = (*env)->GetObjectClass(env, callback);
  g_conv_on_state_changed = (*env)->GetMethodID(env, callbackClass, "onStateChanged", "(I)V");
  g_conv_on_user_transcript = (*env)->GetMethodID(
      env, callbackClass, "onUserTranscript", "(Ljava/lang/String;Ljava/lang/String;)V");
  g_conv_on_response_text = (*env)->GetMethodID(
      env, callbackClass, "onResponseText", "(Ljava/lang/String;Ljava/lang/String;)V");
  g_conv_on_speak_request = (*env)->GetMethodID(
      env, callbackClass, "onSpeakRequest", "(Ljava/lang/String;Ljava/lang/String;IZ)V");
  g_conv_on_error = (*env)->GetMethodID(env, callbackClass, "onError", "(Ljava/lang/String;)V");

  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
  }

  ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
  // Mode 1/4 are both manually triggered (a "Talk"/"Voice query" button, not
  // a wake word), so always_listening=true regardless of the desktop-only
  // default in get_default_config().
  config.always_listening = true;
  config.tts_enabled = tts_enabled;
  config.barge_in_enabled = barge_in_enabled;
  config.on_state_change = jni_conversation_on_state_change;
  config.on_user_transcript = jni_conversation_on_user_transcript;
  config.on_response_text = jni_conversation_on_response_text;
  config.on_error = jni_conversation_on_error;
  if (tts_enabled) {
    config.on_speak_request = jni_conversation_on_speak_request;
  }

  // Resolve Granite Speech Plus model paths against the Android app files
  // dir (same convention ethervox_voice_tools_init uses for Mode 2 in
  // src/plugins/voice_tools/voice_tools.c) - conversation_thread() honors
  // these directly rather than falling back to its getenv("HOME") desktop
  // default (see the fix in voice_conversation.c). A single Plus (model,
  // mmproj) pair now backs every voice mode: voice_conversation.c hardcodes
  // stt_config.backend = ETHERVOX_STT_BACKEND_GRANITE_SPEECH (non-plus) for
  // Mode 1/4, which selects the plain ASR-only prompt in
  // granite_speech_backend.c - functionally equivalent to the old separate
  // BASE model per IBM's model card, just running on the Plus weights.
  static char speech_model_path[512];
  static char speech_mmproj_path[512];
  const char* files_dir = ethervox_get_android_files_dir();
  if (files_dir) {
    snprintf(speech_model_path, sizeof(speech_model_path),
             "%s/models/%s/granite-speech-4.1-2b-plus.Q4_K_M.gguf", files_dir,
             ETHERVOX_GRANITE_SPEECH_SUBDIR);
    snprintf(speech_mmproj_path, sizeof(speech_mmproj_path),
             "%s/models/%s/mmproj-granite-speech-4.1-2b-plus-Q4_K_M.gguf", files_dir,
             ETHERVOX_GRANITE_SPEECH_SUBDIR);
    config.stt.model_path = speech_model_path;
    config.stt.mmproj_path = speech_mmproj_path;
    LOGI("[VoiceConversation] Granite Speech Plus model path: %s", speech_model_path);
  } else {
    LOGW("[VoiceConversation] Android files dir not set - falling back to desktop model path");
  }

  g_conversation_session = ethervox_conversation_init(&config, g_governor);
  if (!g_conversation_session) {
    LOGE("[VoiceConversation] ethervox_conversation_init failed");
    (*env)->DeleteGlobalRef(env, g_conversation_callback_obj);
    g_conversation_callback_obj = NULL;
    return -1;
  }

  ethervox_result_t result = ethervox_conversation_start(g_conversation_session);
  if (ethervox_is_error(result)) {
    LOGE("[VoiceConversation] ethervox_conversation_start failed: %d", result);
    ethervox_conversation_cleanup(g_conversation_session);
    g_conversation_session = NULL;
    (*env)->DeleteGlobalRef(env, g_conversation_callback_obj);
    g_conversation_callback_obj = NULL;
    return -1;
  }

  LOGI("[VoiceConversation] Session started (tts_enabled=%d)", tts_enabled);
  return 0;
}

static void stop_voice_conversation_session(JNIEnv* env) {
  if (!g_conversation_session) {
    return;
  }

  ethervox_conversation_stop(g_conversation_session);
  ethervox_conversation_cleanup(g_conversation_session);
  g_conversation_session = NULL;

  if (g_conversation_callback_obj) {
    (*env)->DeleteGlobalRef(env, g_conversation_callback_obj);
    g_conversation_callback_obj = NULL;
  }
  g_conv_on_state_changed = NULL;
  g_conv_on_user_transcript = NULL;
  g_conv_on_response_text = NULL;
  g_conv_on_speak_request = NULL;
  g_conv_on_error = NULL;

  LOGI("[VoiceConversation] Session stopped");
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_voiceConversationStart(
    JNIEnv* env, jobject thiz, jobject callback, jboolean bargeInEnabled) {
  (void)thiz;
  return start_voice_conversation_session(env, callback, /*tts_enabled=*/true,
                                           /*barge_in_enabled=*/bargeInEnabled);
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_voiceConversationStop(JNIEnv* env,
                                                                                      jobject thiz) {
  (void)thiz;
  stop_voice_conversation_session(env);
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_voiceConversationInterrupt(
    JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;
  if (g_conversation_session) {
    ethervox_conversation_interrupt(g_conversation_session);
  }
}

// Called from Kotlin's UtteranceProgressListener.onDone()/onError() once
// platform TTS finishes playing an onSpeakRequest utterance - see the doc
// comment on jni_conversation_on_speak_request above.
JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_voiceConversationNotifySpeakingDone(
    JNIEnv* env, jobject thiz) {
  (void)env;
  (void)thiz;
  if (g_conversation_session) {
    ethervox_conversation_notify_speaking_done(g_conversation_session);
  }
}

JNIEXPORT jint JNICALL Java_com_droid_ethervox_1core_NativeLib_voiceQueryStart(JNIEnv* env,
                                                                                jobject thiz,
                                                                                jobject callback,
                                                                                jboolean bargeInEnabled) {
  (void)thiz;
  return start_voice_conversation_session(env, callback, /*tts_enabled=*/false,
                                           /*barge_in_enabled=*/bargeInEnabled);
}

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_voiceQueryStop(JNIEnv* env,
                                                                               jobject thiz) {
  (void)thiz;
  stop_voice_conversation_session(env);
}

// ===========================================================================
// Mobile Optimization Features (Minimal Mode, Secret Mode)
// ===========================================================================

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_setPrivacyMode(JNIEnv* env,
                                                                              jobject thiz,
                                                                              jboolean enabled) {
  (void)env;
  (void)thiz;

  ethervox_memory_set_privacy_mode(enabled ? true : false);

  if (enabled) {
    LOGI("[JNI] SECRET MODE enabled - memory logging disabled");
  } else {
    LOGI("[JNI] Normal mode - memory logging enabled");
  }
}

JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_getPrivacyMode(JNIEnv* env,
                                                                                  jobject thiz) {
  (void)env;
  (void)thiz;

  return ethervox_memory_get_privacy_mode() ? JNI_TRUE : JNI_FALSE;
}

// ===========================================================================
// Memory Pool Admission Control (N6.2c)
// ===========================================================================

JNIEXPORT jlongArray JNICALL Java_com_droid_ethervox_1core_NativeLib_checkModelWouldFit(
    JNIEnv* env, jobject thiz, jstring modelPath, jint contextSize, jboolean useGpu) {
  (void)thiz;
  
  if (!g_model_pool) {
    LOGE("Model pool not initialized");
    return NULL;
  }
  
  const char* path = (*env)->GetStringUTFChars(env, modelPath, NULL);
  if (!path) {
    return NULL;
  }
  
  // Build model config for admission check
  ethervox_model_config_t config = {0};
  config.model_path = path;
  config.mmproj_path = NULL;
  config.context_size = (uint32_t)contextSize;
  config.n_threads = sysconf(_SC_NPROCESSORS_ONLN);  // Use all cores
  config.n_seq_max = 2;  // Default for text-only models
  config.use_gpu = useGpu ? true : false;
  config.kv_unified = false;
  config.role = "temp_check";  // Temporary role for estimation
  config.residency = ETHERVOX_RESIDENCY_RESIDENT;
  config.ttl_seconds = 0;
  
  bool fits = false;
  uint64_t required_bytes = 0;
  ethervox_result_t result = ethervox_model_pool_would_fit(g_model_pool, &config, &fits, &required_bytes);
  
  (*env)->ReleaseStringUTFChars(env, modelPath, path);
  
  if (ethervox_is_error(result)) {
    LOGE("Failed to check would_fit: error code %d", result);
    return NULL;
  }
  
  // Return [fits (0 or 1), requiredBytes]
  jlongArray result_array = (*env)->NewLongArray(env, 2);
  if (!result_array) {
    return NULL;
  }
  
  jlong values[2];
  values[0] = fits ? 1 : 0;
  values[1] = (jlong)required_bytes;
  (*env)->SetLongArrayRegion(env, result_array, 0, 2, values);
  
  LOGI("Model admission check: %s fits=%d, required=%.2f GB",
       config.model_path,
       fits,
       required_bytes / (1024.0 * 1024.0 * 1024.0));
  
  return result_array;
}

JNIEXPORT jlongArray JNICALL Java_com_droid_ethervox_1core_NativeLib_getMemoryPoolUsage(
    JNIEnv* env, jobject thiz) {
  (void)thiz;
  
  if (!g_model_pool) {
    return NULL;
  }
  
  uint64_t used_bytes = 0;
  uint64_t budget_bytes = 0;
  ethervox_result_t result = ethervox_model_pool_memory_usage(g_model_pool, &used_bytes, &budget_bytes);
  
  if (ethervox_is_error(result)) {
    return NULL;
  }
  
  // Return [used, budget]
  jlongArray result_array = (*env)->NewLongArray(env, 2);
  if (!result_array) {
    return NULL;
  }
  
  jlong values[2];
  values[0] = (jlong)used_bytes;
  values[1] = (jlong)budget_bytes;
  (*env)->SetLongArrayRegion(env, result_array, 0, 2, values);
  
  return result_array;
}

// ===========================================================================
// Memory Pressure Callbacks (N6.2d)
// ===========================================================================

JNIEXPORT void JNICALL Java_com_droid_ethervox_1core_NativeLib_onMemoryPressure(
    JNIEnv* env, jobject thiz, jint level) {
  (void)env;
  (void)thiz;
  
  if (!g_model_pool) {
    LOGE("Model pool not initialized - cannot respond to memory pressure");
    return;
  }
  
  // Android ComponentCallbacks2 constants:
  // TRIM_MEMORY_RUNNING_LOW = 10
  // TRIM_MEMORY_RUNNING_CRITICAL = 15
  
  uint64_t bytes_to_free = 0;
  const char* level_name = "UNKNOWN";
  
  if (level == 10) {  // TRIM_MEMORY_RUNNING_LOW
    bytes_to_free = 500ULL * 1024 * 1024;  // Try to free 500 MB
    level_name = "RUNNING_LOW";
  } else if (level == 15) {  // TRIM_MEMORY_RUNNING_CRITICAL
    bytes_to_free = 0;  // 0 means evict as much as possible
    level_name = "RUNNING_CRITICAL";
  } else {
    LOGW("Unhandled memory pressure level: %d", level);
    return;
  }
  
  LOGI("[MemoryPressure] OS signaled %s (level %d) - attempting eviction", level_name, level);
  
  // Always protect the "main" role (governor) from eviction
  const char* protected_roles[] = { "main", NULL };
  
  uint64_t freed_bytes = 0;
  ethervox_result_t result = ethervox_model_pool_evict_lru(
      g_model_pool,
      bytes_to_free,
      protected_roles,
      &freed_bytes
  );
  
  if (ethervox_is_success(result)) {
    if (freed_bytes > 0) {
      LOGI("[MemoryPressure] Evicted %.2f MB in response to %s",
           freed_bytes / (1024.0 * 1024.0),
           level_name);
    } else {
      LOGI("[MemoryPressure] No evictable models found (all resident or protected)");
    }
  } else {
    LOGE("[MemoryPressure] Eviction failed with error code: %d", result);
  }
}

JNIEXPORT jboolean JNICALL Java_com_droid_ethervox_1core_NativeLib_loadGovernorModelMinimal(
    JNIEnv* env, jobject thiz, jstring modelPath, jobject callback) {
  (void)thiz;

  if (!g_governor) {
    LOGE("Cannot load Governor model - governor not initialized");
    return JNI_FALSE;
  }

  const char* path = (*env)->GetStringUTFChars(env, modelPath, NULL);

  LOGI("[JNI] Loading Governor model in MINIMAL MODE (fast mobile loading)");
  LOGI("[JNI] Model path: %s", path);

  // NOTE: Minimal mode must be set during platformInit() via governor config
  // Cannot change config after governor is initialized
  // This function is now equivalent to loadGovernorModel
  // TODO: Add proper API to change governor mode at runtime

  // === CRITICAL: Initialize Tool Manifest System BEFORE loading model ===
  // Even in minimal mode, we prepare the manifest for potential future use
  if (!g_manifest_registry) {
    LOGI("[JNI] Initializing manifest registry BEFORE model load");

    tool_manifest_registry_t* manifest = NULL;
    ethervox_result_t manifest_result =
        ethervox_governor_setup_manifest(g_governor, path, &manifest);

    if (ethervox_is_success(manifest_result) && manifest) {
      g_manifest_registry = manifest;
      ethervox_governor_set_manifest(g_governor, manifest);
      LOGI("[JNI] Manifest initialized (minimal mode, may not be used)");
    }
  } else {
    ethervox_governor_set_manifest(g_governor, g_manifest_registry);
  }

  // Setup progress callback if provided
  jni_load_context_t load_ctx = {0};
  ethervox_load_progress_callback progress_callback = NULL;
  void* progress_user_data = NULL;
  
  if (callback) {
    jclass callback_class = (*env)->GetObjectClass(env, callback);
    jmethodID on_governor_progress = (*env)->GetMethodID(env, callback_class, "onGovernorProgress",
                                                        "(Ljava/lang/String;Ljava/lang/String;)V");
    if (on_governor_progress) {
      load_ctx.env = env;
      load_ctx.callback_obj = callback;
      load_ctx.on_governor_progress_method = on_governor_progress;
      progress_callback = native_load_progress_callback;
      progress_user_data = &load_ctx;
    }
  }

  // N6.1: Pass cache_dir string for KV cache (not whole paths struct)
  const char* cache_dir = g_paths_initialized ? g_android_paths.cache_dir : g_android_files_dir;
  ethervox_result_t result = ethervox_governor_load_model(
      g_governor, path, cache_dir, NULL, progress_callback, progress_user_data
  );

  (*env)->ReleaseStringUTFChars(env, modelPath, path);

  if (ethervox_is_success(result)) {
    LOGI("[JNI] Governor model loaded successfully");
    LOGW("[JNI] MINIMAL MODE must be set via config in platformInit - cannot change at load time");
    return JNI_TRUE;
  } else {
    LOGE("Failed to load Governor model");
    return JNI_FALSE;
  }
}

// ============================================================================
// Additional Conversation Summary JNI Functions
// ============================================================================

/**
 * Get the last generated conversation summary text for UI display
 */
JNIEXPORT jstring JNICALL
Java_com_droid_ethervox_1core_NativeLib_getLastSummary(
    JNIEnv* env, jobject thiz
) {
    (void)thiz;
    const char* summary = ethervox_get_last_summary();
    return (*env)->NewStringUTF(env, summary);
}

/**
 * Restore conversation context from memory
 */
JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_restoreContextFromMemory(
    JNIEnv* env, jobject thiz
) {
    (void)thiz;
    (void)env;
    
    if (!g_governor || !g_memory_store) {
        LOGE("[JNI] Governor or memory store not initialized");
        return JNI_FALSE;
    }
    
    LOGI("[JNI] Restoring conversation context from memory...");
    
    ethervox_result_t result = ethervox_restore_context_from_memory(
        g_governor,
        g_memory_store
    );
    
    return ethervox_is_success(result) ? JNI_TRUE : JNI_FALSE;
}

/**
 * Load conversation summary from KV cache file (if exists)
 */
JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_loadConversationSummary(
    JNIEnv* env, jobject thiz
) {
    (void)thiz;
    (void)env;
    
    if (!g_governor) {
        LOGE("[JNI] Governor not initialized");
        return JNI_FALSE;
    }
    
    if (g_android_files_dir[0] == '\0') {
        LOGE("[JNI] Android files directory not set");
        return JNI_FALSE;
    }
    
    char cache_dir[512];
    snprintf(cache_dir, sizeof(cache_dir), "%s/cache", g_android_files_dir);
    
    LOGI("[JNI] Loading conversation summary from: %s", cache_dir);
    
    ethervox_result_t result = ethervox_load_conversation_summary(
        g_governor,
        cache_dir
    );
    
    if (!ethervox_is_success(result)) {
        LOGE("[JNI] Failed to load conversation summary: %d", result);
        return JNI_FALSE;
    }
    
    // After loading summary, restore recent context from memory
    if (g_memory_store) {
        ethervox_restore_context_from_memory(g_governor, g_memory_store);
    }
    
    return JNI_TRUE;
}

// ============================================================================
// Host-registered tools (N5.2 - C2.2 API)
// ============================================================================

// Global ref to NativeLib instance for tool callbacks
static jobject g_native_lib_instance = NULL;

/**
 * Context passed to host tool callbacks via user_data.
 * Contains tool name needed to route execution back to correct Kotlin implementation.
 */
typedef struct host_tool_context {
    char* tool_name;  // Tool name for routing
    struct host_tool_context* next;  // Linked list for cleanup
} host_tool_context_t;

// Head of linked list tracking all allocated contexts (for cleanup)
static host_tool_context_t* g_host_tool_contexts = NULL;

/**
 * Host tool execution callback invoked by ethervox_core.
 * 
 * This is called by the core when the governor needs to execute a host tool:
 * 1. Model requests tool execution
 * 2. Core invokes this callback with arguments
 * 3. We extract tool name from user_data, attach to JVM thread
 * 4. Call back into Kotlin via invokeHostToolFromNative(toolName, argumentsJson)
 * 5. Kotlin HostToolRegistry finds the HostTool instance and executes it
 * 6. Result is returned back through JNI to core -> model
 * 
 * Threading: Called from core's tool execution thread (not Android main thread).
 * Memory: result_out and error_out are malloc'd strings - caller uses ethervox_string_free().
 */
static ethervox_result_t host_tool_callback(
    const char* arguments_json,
    void* user_data,
    char** result_out,
    char** error_out
) {
    if (!user_data) {
        LOGE("Host tool callback: user_data is NULL");
        *error_out = strdup("Internal error: tool context missing");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    host_tool_context_t* context = (host_tool_context_t*)user_data;
    const char* tool_name = context->tool_name;
    
    if (!g_jvm || !g_native_lib_instance) {
        LOGE("Host tool callback: JVM or NativeLib instance not initialized");
        *error_out = strdup("JVM not initialized");
        return ETHERVOX_ERROR_INVALID_STATE;
    }
    
    // Attach current thread to JVM
    JNIEnv* env = NULL;
    int attach_result = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
    if (attach_result != JNI_OK || !env) {
        LOGE("Failed to attach thread to JVM: %d", attach_result);
        *error_out = strdup("Failed to attach to JVM");
        return ETHERVOX_ERROR_INVALID_STATE;
    }
    
    // Find NativeLib class and invokeHostToolFromNative method
    jclass native_lib_class = (*env)->GetObjectClass(env, g_native_lib_instance);
    if (!native_lib_class) {
        LOGE("Failed to get NativeLib class");
        *error_out = strdup("Failed to get NativeLib class");
        (*g_jvm)->DetachCurrentThread(g_jvm);
        return ETHERVOX_ERROR_INVALID_STATE;
    }
    
    jmethodID invoke_method = (*env)->GetMethodID(
        env,
        native_lib_class,
        "invokeHostToolFromNative",
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"
    );
    
    if (!invoke_method) {
        LOGE("Failed to find invokeHostToolFromNative method");
        *error_out = strdup("Method not found");
        (*g_jvm)->DetachCurrentThread(g_jvm);
        return ETHERVOX_ERROR_INVALID_STATE;
    }
    
    // Convert C strings to Java strings
    jstring j_tool_name = (*env)->NewStringUTF(env, tool_name);
    jstring j_arguments = (*env)->NewStringUTF(env, arguments_json);
    
    if (!j_tool_name || !j_arguments) {
        LOGE("Failed to create Java strings");
        *error_out = strdup("Failed to create Java strings");
        (*g_jvm)->DetachCurrentThread(g_jvm);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    // Call Kotlin method
    jstring j_result = (jstring)(*env)->CallObjectMethod(
        env,
        g_native_lib_instance,
        invoke_method,
        j_tool_name,
        j_arguments
    );
    
    // Check for exceptions during execution
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        *error_out = strdup("Tool execution threw exception");
        (*g_jvm)->DetachCurrentThread(g_jvm);
        return ETHERVOX_ERROR_FAILED;
    }
    
    // Convert result back to C string
    if (j_result) {
        const char* result_str = (*env)->GetStringUTFChars(env, j_result, NULL);
        if (result_str) {
            *result_out = strdup(result_str);
            (*env)->ReleaseStringUTFChars(env, j_result, result_str);
            
            if (!*result_out) {
                *error_out = strdup("Failed to allocate result string");
                (*g_jvm)->DetachCurrentThread(g_jvm);
                return ETHERVOX_ERROR_OUT_OF_MEMORY;
            }
        } else {
            *error_out = strdup("Failed to get result string");
            (*g_jvm)->DetachCurrentThread(g_jvm);
            return ETHERVOX_ERROR_FAILED;
        }
    } else {
        *error_out = strdup("Tool returned null");
        (*g_jvm)->DetachCurrentThread(g_jvm);
        return ETHERVOX_ERROR_FAILED;
    }
    
    (*g_jvm)->DetachCurrentThread(g_jvm);
    return ETHERVOX_SUCCESS;
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_registerHostTool(
    JNIEnv* env,
    jobject thiz,
    jstring j_name,
    jstring j_description,
    jstring j_schema_json,
    jboolean j_is_mutating
) {
    // Store JavaVM and NativeLib instance for callbacks (first call only)
    if (!g_jvm) {
        (*env)->GetJavaVM(env, &g_jvm);
    }
    
    if (!g_native_lib_instance) {
        g_native_lib_instance = (*env)->NewGlobalRef(env, thiz);
    }
    
    if (!g_manifest_registry) {
        LOGE("registerHostTool: Manifest registry not initialized");
        return JNI_FALSE;
    }
    
    // Convert Java strings to C
    const char* name = (*env)->GetStringUTFChars(env, j_name, NULL);
    const char* description = (*env)->GetStringUTFChars(env, j_description, NULL);
    const char* schema_json = (*env)->GetStringUTFChars(env, j_schema_json, NULL);
    bool is_mutating = (j_is_mutating == JNI_TRUE);
    
    // Allocate context for callback (stores tool name for routing)
    host_tool_context_t* context = (host_tool_context_t*)malloc(sizeof(host_tool_context_t));
    if (!context) {
        LOGE("registerHostTool: Failed to allocate context");
        (*env)->ReleaseStringUTFChars(env, j_name, name);
        (*env)->ReleaseStringUTFChars(env, j_description, description);
        (*env)->ReleaseStringUTFChars(env, j_schema_json, schema_json);
        return JNI_FALSE;
    }
    
    context->tool_name = strdup(name);
    if (!context->tool_name) {
        LOGE("registerHostTool: Failed to duplicate tool name");
        free(context);
        (*env)->ReleaseStringUTFChars(env, j_name, name);
        (*env)->ReleaseStringUTFChars(env, j_description, description);
        (*env)->ReleaseStringUTFChars(env, j_schema_json, schema_json);
        return JNI_FALSE;
    }
    
    // Add to linked list for cleanup
    context->next = g_host_tool_contexts;
    g_host_tool_contexts = context;
    
    // Create host tool descriptor
    ethervox_host_tool_t tool = {
        .name = name,
        .description = description,
        .parameters_schema_json = schema_json,
        .is_mutating = is_mutating,
        .invoke = host_tool_callback,
        .user_data = context  // Pass context so callback can route to correct tool
    };
    
    // Register tool with core
    ethervox_result_t result = ethervox_tool_registry_register_host_tool(
        g_manifest_registry,
        &tool
    );
    
    // Release Java strings
    (*env)->ReleaseStringUTFChars(env, j_name, name);
    (*env)->ReleaseStringUTFChars(env, j_description, description);
    (*env)->ReleaseStringUTFChars(env, j_schema_json, schema_json);
    
    if (!ethervox_is_success(result)) {
        LOGE("registerHostTool: Registration failed: %d", result);
        free(context->tool_name);
        free(context);
        return JNI_FALSE;
    }
    
    LOGI("Registered host tool: %s (mutating=%d)", context->tool_name, is_mutating);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_droid_ethervox_1core_NativeLib_clearHostTools(JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;
    
    if (!g_manifest_registry) {
        LOGE("clearHostTools: Manifest registry not initialized");
        return;
    }
    
    // Clear tools from registry
    ethervox_tool_registry_clear_host_tools(g_manifest_registry);
    
    // Free all allocated contexts
    host_tool_context_t* context = g_host_tool_contexts;
    while (context) {
        host_tool_context_t* next = context->next;
        free(context->tool_name);
        free(context);
        context = next;
    }
    g_host_tool_contexts = NULL;
    
    LOGI("Cleared all host tools and freed contexts");
}

JNIEXPORT void JNICALL
Java_com_droid_ethervox_1core_NativeLib_setHostToolTimeout(JNIEnv* env, jobject thiz, jlong timeout_ms) {
    (void)env;
    (void)thiz;
    
    if (!g_manifest_registry) {
        LOGE("setHostToolTimeout: Manifest registry not initialized");
        return;
    }
    
    ethervox_tool_registry_set_timeout(g_manifest_registry, (uint32_t)timeout_ms);
    LOGI("Set host tool timeout: %ldms", (long)timeout_ms);
}
