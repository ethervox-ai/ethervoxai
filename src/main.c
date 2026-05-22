/**
 * @file main.c
 * @brief Main entry point for EthervoxAI command-line tool
 *
 * Interactive REPL with Governor orchestration and memory tools
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>  // for _mkdir
#include <fcntl.h>   // for _O_WRONLY
#include <io.h>      // for _access
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define usleep(us) Sleep((us) / 1000)
#define sleep(s) Sleep((s) * 1000)
// Windows doesn't have /dev/null, use NUL instead
#define open_devnull() _open("NUL", _O_WRONLY)
// gettimeofday is not available on Windows, use GetSystemTimeAsFileTime
static inline int gettimeofday(struct timeval* tp, void* tzp) {
  (void)tzp;  // unused
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  uint64_t time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
  time -= 116444736000000000ULL;  // Convert from 1601 epoch to 1970 epoch
  tp->tv_sec = (long)(time / 10000000ULL);
  tp->tv_usec = (long)((time % 10000000ULL) / 10);
  return 0;
}
#else
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#define open_devnull() open("/dev/null", O_WRONLY)
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <readline/history.h>
#include <readline/readline.h>
#elif defined(__linux__)
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "ethervox/audio.h"
#include "ethervox/audio_recording.h"
#include "ethervox/audio_stream_player.h"
#include "ethervox/bug_reporter.h"
#include "ethervox/compute_tools.h"
#include "ethervox/config.h"
#include "ethervox/conversation.h"
#include "ethervox/conversation_tools.h"
#include "ethervox/dialogue.h"
#include "ethervox/error.h"
#include "ethervox/file_tools.h"
#include "ethervox/get_tool_info.h"
#include "ethervox/governor.h"
#include "ethervox/integration_tests.h"
#include "ethervox/language_detector.h"
#include "ethervox/llm_tool_tests.h"
#include "ethervox/logging.h"
#include "ethervox/memory_tools.h"
#include "ethervox/model_downloader.h"
#if HAVE_LIBCURL
#include "ethervox/weather_tools.h"
#endif
#include "ethervox/platform.h"
#include "ethervox/settings.h"
#include "ethervox/settings_menu.h"
#include "ethervox/startup_prompt_tools.h"
#include "ethervox/system_info_tools.h"
#include "ethervox/tool_manifest.h"
#include "ethervox/tool_prompt_optimizer.h"
#include "ethervox/tts.h"
#include "ethervox/unit_conversion.h"
#include "ethervox/voice_tools.h"
#include "ethervox/voice_training.h"
#include "ethervox/wake_word.h"

// External debug flag from logging.c (declared in config.h)
// extern int g_ethervox_debug_enabled; // Already declared in config.h

// Global state for signal handling
static volatile bool g_running = true;
static bool g_debug_enabled = false;     // Debug logging disabled by default (opt-in)
static bool g_quiet_mode = true;         // Quiet mode by default
static bool g_markdown_enabled = true;   // Markdown formatting enabled by default
static bool g_streaming_enabled = true;  // Token streaming enabled by default
static bool g_engineering_mode = false;  // Engineering mode disabled by default

// Voice system: Two separate pipelines
// 1. Transcription pipeline (Whisper STT) - for meeting transcription, dictation
//    Triggered manually with /transcribe command, high-accuracy but slower
static ethervox_voice_session_t* g_transcription_session = NULL;
static volatile sig_atomic_t g_sigint_stop_transcribe = 0;

// 2. Conversation pipeline (Vosk STT + Piper TTS) - for natural LLM interaction
//    Triggered by wake word detection, real-time and responsive
static ethervox_conversation_session_t* g_conversation_session = NULL;

// Governor (shared between CLI and conversation)
static ethervox_governor_t* g_governor = NULL;
static pthread_mutex_t g_governor_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global persistent settings (Whisper/conversation/wake word)
static ethervox_persistent_settings_t g_settings;

// Global TTS instance (for standalone speak tool, shared with conversation)
// Defined in src/dialogue/global_tts.c
extern ethervox_tts_context_t* g_global_tts;
extern pthread_mutex_t g_tts_mutex;

// Streaming audio player for real-time TTS playback
static audio_stream_player_t* g_stream_player = NULL;

// TTS chunk callback for streaming playback
static void tts_stream_callback(const float* samples, size_t sample_count, void* user_data) {
  audio_stream_player_t* player = (audio_stream_player_t*)user_data;
  if (player) {
    audio_stream_player_write(player, samples, sample_count);
  }
}

// Wake word detection (triggers conversation pipeline)
static ethervox_wake_runtime_t* g_wake_runtime = NULL;
static bool g_wake_enabled = false;

// Wake word listening thread
static pthread_t g_wake_thread;
static bool g_wake_thread_running = false;
static pthread_mutex_t g_wake_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Token streaming callback - displays tokens as they're generated
 */
static void stream_token_callback(const char* token, void* user_data) {
  (void)user_data;  // Unused

  // Apply cyan color for streaming output
  printf("\033[36m%s\033[0m", token);
  fflush(stdout);  // Ensure immediate display
}

/**
 * Standalone speak callback for use without /convon
 * Allows the LLM to use the speak tool even in CLI mode
 */
static ethervox_result_t standalone_speak_callback(const char* text, const char* language,
                                                   bool wait_for_response, bool allow_interrupt,
                                                   void* user_data) {
  (void)language;
  (void)wait_for_response;
  (void)allow_interrupt;
  (void)user_data;

  if (!text) {
    ETHERVOX_ERROR_SET(ETHERVOX_ERROR_NULL_POINTER, "Text parameter is NULL");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  pthread_mutex_lock(&g_tts_mutex);

  if (!g_global_tts) {
    pthread_mutex_unlock(&g_tts_mutex);
    // TTS not initialized - just print to console
    printf("\n========================================================\n");
    printf("🤖 Assistant: %s\n", text);
    printf("========================================================\n\n");
    return ETHERVOX_SUCCESS;
  }

  printf("\n🔊 Synthesizing: \"%s\"\n", text);

  // Synthesize
  ethervox_tts_audio_t output = {0};
  ethervox_result_t result = ethervox_tts_synthesize_text(g_global_tts, text, &output);

  pthread_mutex_unlock(&g_tts_mutex);

  if (ethervox_is_success(result) && output.samples && output.sample_count > 0) {
    printf("   [OK] Synthesized %zu samples at %d Hz\n", output.sample_count, output.sample_rate);

    // Save and play
    char temp_file[512];
    snprintf(temp_file, sizeof(temp_file), "%s/.ethervox/temp_speak.wav",
             getenv("HOME") ? getenv("HOME") : ".");

    if (ethervox_is_success(ethervox_audio_write_wav(temp_file, output.samples,
                                                     (int)output.sample_count, output.sample_rate,
                                                     output.channels))) {
      printf("   🎵 Playing audio...\n");

#ifdef __APPLE__
      char cmd[600];
      snprintf(cmd, sizeof(cmd), "afplay %s", temp_file);
      system(cmd);
#elif __linux__
      char cmd[600];
      snprintf(cmd, sizeof(cmd), "aplay %s 2>/dev/null", temp_file);
      system(cmd);
#endif
      printf("   [OK] Playback complete\n\n");
    }

    free(output.samples);
    return ETHERVOX_SUCCESS;
  }

  printf("   ❌ TTS synthesis failed\n\n");
  return ETHERVOX_ERROR_TTS_SYNTHESIS_FAILED;
}

/**
 * Standalone listen callback (not yet implemented)
 */
static ethervox_result_t standalone_listen_callback(char** user_input, int timeout_ms,
                                                    const char* prompt_hint, void* user_data) {
  (void)user_input;
  (void)timeout_ms;
  (void)prompt_hint;
  (void)user_data;
  // Not implemented for CLI mode - listen tool only works in /convon
  return ETHERVOX_ERROR_NOT_IMPLEMENTED;
}

/**
 * Wake word listening thread - continuously monitors microphone
 */
static void* wake_word_listen_thread(void* arg) {
  (void)arg;

  // Initialize audio for wake word detection
  ethervox_audio_runtime_t audio_runtime = {0};
  ethervox_audio_config_t audio_config = {0};
  audio_config.sample_rate = 16000;
  audio_config.channels = 1;
  audio_config.bits_per_sample = 16;
  audio_config.buffer_size = 4096;

  ethervox_result_t result = ethervox_audio_register_platform_driver(&audio_runtime);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "[Wake] Failed to register audio driver: %s\n", ethervox_error_string(result));
    if (ctx && ctx->message) {
      fprintf(stderr, "       %s at %s:%d\n", ctx->message, ctx->file, ctx->line);
    }
    g_wake_thread_running = false;
    return NULL;
  }

  result = audio_runtime.driver.init(&audio_runtime, &audio_config);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    if (ctx && ctx->message) {
      fprintf(stderr, "[Wake] Failed to initialize audio: %s\n", ctx->message);
    } else {
      fprintf(stderr, "[Wake] Failed to initialize audio: %s\n", ethervox_error_string(result));
    }
    g_wake_thread_running = false;
    return NULL;
  }

  result = audio_runtime.driver.start_capture(&audio_runtime);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    if (ctx && ctx->message) {
      fprintf(stderr, "[Wake] Failed to start audio capture: %s\n", ctx->message);
    } else {
      fprintf(stderr, "[Wake] Failed to start audio capture: %s\n", ethervox_error_string(result));
    }
    audio_runtime.driver.cleanup(&audio_runtime);
    g_wake_thread_running = false;
    return NULL;
  }

  printf("[Wake] Microphone listening started (calibrating for ~5 seconds...)\n");

  int audio_chunks_processed = 0;
  bool calibration_complete = false;

  // Main listening loop
  while (g_wake_thread_running) {
    // Check if wake word detection is enabled
    pthread_mutex_lock(&g_wake_mutex);
    bool enabled = g_wake_enabled;
    ethervox_wake_runtime_t* runtime = g_wake_runtime;
    pthread_mutex_unlock(&g_wake_mutex);

    if (!enabled || !runtime) {
      usleep(100000);  // 100ms
      continue;
    }

    // Read audio from microphone (100ms chunks)
    ethervox_audio_buffer_t buffer;
    buffer.size = 1600;  // 100ms at 16kHz
    buffer.channels = 1;
    buffer.data = (float*)calloc(buffer.size, sizeof(float));

    // Set timestamp for wake word detector
    struct timeval tv;
    gettimeofday(&tv, NULL);
    buffer.timestamp_us = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;

    if (!buffer.data) {
      usleep(100000);
      continue;
    }

    int samples = audio_runtime.driver.read_audio(&audio_runtime, &buffer);

    // Show when calibration is complete
    if (samples > 0) {
      audio_chunks_processed++;
      if (!calibration_complete && audio_chunks_processed == 50) {
        printf(
            "[Wake] [OK] Background calibration complete - actively listening for 'hey "
            "ethervox'\n");
        calibration_complete = true;
      }
    }

    if (samples > 0) {
      // Process with wake word detector
      ethervox_wake_result_t wake_result = {0};

      pthread_mutex_lock(&g_wake_mutex);
      ethervox_result_t result = ethervox_wake_process(runtime, &buffer, &wake_result);
      pthread_mutex_unlock(&g_wake_mutex);

      // Debug: Show wake word processing results occasionally
      static int debug_counter = 0;
      if (g_debug_enabled && ++debug_counter % 50 == 0) {  // Every 5 seconds
        printf("[Wake Debug] result=%s, detected=%d, confidence=%.3f, samples=%d\n",
               ethervox_is_success(result) ? "SUCCESS" : ethervox_error_string(result),
               wake_result.detected, wake_result.confidence, samples);
      }

      if (ethervox_is_success(result) && wake_result.detected) {
        // Wake word detected!
        printf("\n🎤 Wake word detected! (confidence: %.2f)\n", wake_result.confidence);

        // Trigger conversation if available
        if (g_conversation_session) {
          ethervox_result_t trigger_result = ethervox_conversation_trigger(g_conversation_session);
          if (ethervox_is_error(trigger_result)) {
            const ethervox_error_context_t* ctx = ethervox_error_get_context();
            fprintf(stderr, "[Wake] Failed to trigger conversation: %s\n",
                    ctx && ctx->message ? ctx->message : ethervox_error_string(trigger_result));
          }
        } else {
          printf("💡 Voice conversation not enabled. Use /convon first.\n");
        }
      }
    }

    free(buffer.data);
    usleep(10000);  // 10ms between reads
  }

  // Cleanup
  audio_runtime.driver.stop_capture(&audio_runtime);
  audio_runtime.driver.cleanup(&audio_runtime);
  printf("[Wake] Microphone listening stopped\n");

  return NULL;
}

#if defined(__APPLE__) || defined(__linux__)
// Command completion for readline
static const char* commands[] = {"/help",          "/test",           "/test_voices",
                                 "/testllm",       "/testwhisper",    "/optimize_tool_prompts",
                                 "/load",          "/tools",          "/search",
                                 "/summary",       "/summarizeCache", "/export",
                                 "/archive",       "/stats",          "/startup",
                                 "/debug",         "/markdown",       "/toggle_tool_calls",
                                 "/clear",         "/reset",          "/paste",
                                 "/paths",         "/setpath",        "/safemode",
                                 "/secret",        "/transcribe",     "/stoptranscribe",
                                 "/setlang",       "/translate",      "/wakeword",
                                 "/wakeon",        "/wakeoff",        "/wakerecord",
                                 "/conversation",  "/convon",         "/convoff",
                                 "/convtrigger",   "/voice_training", "/speak",
                                 "/speak-direct",  "/models",         "/modelstatus",
                                 "/modeldownload", "/modeldelete",    "/config",
                                 "/report",        "/quit",           NULL};

static char* command_generator(const char* text, int state) {
  static int list_index, len;
  const char* name;

  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  while ((name = commands[list_index++])) {
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }

  return NULL;
}

static char** command_completion(const char* text, int start, int end) {
  (void)end;  // Unused

  // Only complete commands at the start of the line
  if (start == 0) {
    return rl_completion_matches(text, command_generator);
  }

  // For file paths after /load, /export, /startup edit
  return NULL;  // Use default filename completion
}
#endif
static char g_loaded_model_path[512] = {0};                 // Track loaded model path for /reset
static tool_manifest_registry_t g_manifest_registry = {0};  // Tool Manifest System registry

// Model reload callback for settings menu
static ethervox_result_t reload_model_callback(const char* model_path, void* user_data) {
  ethervox_governor_t* gov = (ethervox_governor_t*)user_data;
  if (!gov || !model_path) {
    ETHERVOX_ERROR_SET(ETHERVOX_ERROR_NULL_POINTER,
                       "Governor or model_path is NULL in reload callback");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Reload settings from disk and update global settings
  ethervox_persistent_settings_t updated_settings;
  ethervox_result_t load_result = ethervox_settings_load(&updated_settings, NULL);
  if (ethervox_is_success(load_result)) {
    g_settings = updated_settings;

    // Apply to governor via new config
    // NOTE: Governor uses LLM settings for model params (gpu_layers, context, threads)
    //       and governor-specific settings for iteration control
    ethervox_governor_config_t new_config = ethervox_governor_default_config();
    new_config.gpu_layers = g_settings.llm.gpu_layers;         // Use LLM setting, not governor
    new_config.context_size = g_settings.llm.context_length;   // Use LLM setting
    new_config.n_threads = g_settings.llm.n_threads;           // Use LLM setting
    new_config.temperature = g_settings.governor.temperature;  // Use governor setting
    new_config.max_iterations = g_settings.governor.max_iterations;
    new_config.max_tokens_per_response = g_settings.governor.max_tokens_per_iteration;
    new_config.confidence_threshold = g_settings.governor.confidence_threshold;

    // Update governor's config (need to expose this via API)
    ethervox_result_t update_result = ethervox_governor_update_config(gov, &new_config);

    if (g_debug_enabled) {
      if (ethervox_is_success(update_result)) {
        printf("Applied updated settings: gpu_layers=%u, context=%u, threads=%d\n",
               new_config.gpu_layers, new_config.context_size, new_config.n_threads);
      } else {
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        fprintf(stderr, "Failed to update governor config: %s\n",
                ctx && ctx->message ? ctx->message : ethervox_error_string(update_result));
      }
    }
  }

  return ETHERVOX_SUCCESS;
}

// TTS reload callback for settings menu
static ethervox_result_t tts_reload_callback(const void* tts_settings, void* user_data) {
  (void)user_data;  // Unused
  return ethervox_reload_global_tts(tts_settings, NULL, NULL);
}

// Helper function to initialize ethervox_settings_t for the menu
// Shared between /settings command and -settings flag
static void init_menu_settings(ethervox_settings_t* settings, const char* model_path,
                               const char* memory_dir) {
  // Initialize structure with defaults
  settings->debug_enabled = g_debug_enabled;
  settings->quiet_mode = g_quiet_mode;
  settings->streaming_enabled = g_streaming_enabled;
  settings->engineering_mode = g_engineering_mode;
  settings->log_level = g_quiet_mode ? 0 : (g_debug_enabled ? 2 : 1);
  settings->wake_word_enabled = false;  // Will be updated if wake word runtime exists

  // Zero out string fields
  memset(settings->model_path, 0, sizeof(settings->model_path));
  memset(settings->whisper_model_path, 0, sizeof(settings->whisper_model_path));
  memset(settings->memory_dir, 0, sizeof(settings->memory_dir));
  memset(settings->audio_device, 0, sizeof(settings->audio_device));
  memset(settings->wake_word, 0, sizeof(settings->wake_word));
  memset(settings->git_commit, 0, sizeof(settings->git_commit));
  memset(settings->git_branch, 0, sizeof(settings->git_branch));

// Copy git info
#ifdef ETHERVOX_GIT_COMMIT
  strncpy(settings->git_commit, ETHERVOX_GIT_COMMIT, sizeof(settings->git_commit) - 1);
#endif
#ifdef ETHERVOX_GIT_BRANCH
  strncpy(settings->git_branch, ETHERVOX_GIT_BRANCH, sizeof(settings->git_branch) - 1);
#endif

  // Copy Governor model path
  if (g_loaded_model_path[0] != '\0') {
    strncpy(settings->model_path, g_loaded_model_path, sizeof(settings->model_path) - 1);
  } else if (model_path && model_path[0] != '\0') {
    strncpy(settings->model_path, model_path, sizeof(settings->model_path) - 1);
  } else {
    strcpy(settings->model_path, "(no model loaded)");
  }

  // Memory directory
  if (memory_dir && memory_dir[0] != '\0') {
    strncpy(settings->memory_dir, memory_dir, sizeof(settings->memory_dir) - 1);
  } else {
    snprintf(settings->memory_dir, sizeof(settings->memory_dir), "%s/.ethervox/memory",
             getenv("HOME") ? getenv("HOME") : ".");
  }

  // Copy Whisper model path from voice session or persistent settings
  if (g_transcription_session && g_transcription_session->model_path) {
    strncpy(settings->whisper_model_path, g_transcription_session->model_path,
            sizeof(settings->whisper_model_path) - 1);
  } else if (g_settings.whisper.model_name[0] != '\0') {
    strncpy(settings->whisper_model_path, g_settings.whisper.model_name,
            sizeof(settings->whisper_model_path) - 1);
  } else {
    strcpy(settings->whisper_model_path, "(not loaded)");
  }

  // Populate wake word settings
  if (g_wake_runtime && g_wake_runtime->is_initialized) {
    settings->wake_word_enabled = g_wake_enabled;
    strncpy(settings->wake_word, g_wake_runtime->config.wake_word, sizeof(settings->wake_word) - 1);
  } else if (g_settings.wake_word.wake_phrase[0] != '\0') {
    strncpy(settings->wake_word, g_settings.wake_word.wake_phrase, sizeof(settings->wake_word) - 1);
  } else {
    strcpy(settings->wake_word, "hey ethervox");
  }

  // Audio device (placeholder for future implementation)
  strcpy(settings->audio_device, "(default)");
}

// Default startup prompt text (used if no custom prompt file exists)
// Optimized for IBM Granite - show what to output, not just instructions
// Non-static so it can be accessed from JNI
const char* DEFAULT_STARTUP_PROMPT = "Greet the user with a short creative greeting.";

static void signal_handler(int sig) {
  if (sig == SIGINT && g_transcription_session &&
      (g_transcription_session->is_recording || g_transcription_session->capture_thread)) {
    g_sigint_stop_transcribe = 1;
    return;
  }
  (void)sig;
  g_running = false;
  printf("\n\nShutting down gracefully...\n");
}

static void print_banner(void) {
  printf("\n");
  printf("╔══════════════════════════════════════════════════════════════╗\n");
  printf("║                     EthervoxAI v%s                        ║\n",
         ETHERVOX_VERSION_STRING);
  printf("║          Governor LLM with Memory Tools Plugin               ║\n");
  printf("╚══════════════════════════════════════════════════════════════╝\n");
  printf("Type /help for a list of commands. \n");
  printf("\n");
  if (g_debug_enabled) {
    printf("\n");

#ifdef ETHERVOX_PLATFORM_RPI
    printf("Platform: Raspberry Pi\n");
#elif defined(ETHERVOX_PLATFORM_ESP32)
    printf("Platform: ESP32\n");
#elif defined(ETHERVOX_PLATFORM_MACOS)
    printf("Platform: macOS\n");
#elif defined(ETHERVOX_PLATFORM_LINUX)
    printf("Platform: Linux\n");
#elif defined(ETHERVOX_PLATFORM_WINDOWS)
    printf("Platform: Windows\n");
#elif defined(ETHERVOX_PLATFORM_ANDROID)
    printf("Platform: Android\n");
#else
    printf("Platform: Unknown\n");
#endif
    printf("\n");
  }
}

static void print_help(void) {
  printf("\nAvailable Commands:\n");
  printf("  /help              Show this help message\n");
  printf("  /settings          Open interactive settings menu\n");
  printf(
      "  /config            View/manage persistent configuration (Whisper, conversation, wake "
      "word)\n");
  printf("  /report            Submit bug report or feature request to GitHub\n");
  printf("  /test              Run comprehensive integration tests\n");
  printf("  /test_voices       Demonstrate all configured TTS voices\n");
  printf(
      "  /testllm [name] [-v]  Run LLM tests, optionally filter by test name (-v for verbose)\n");
  printf("                      Examples: /testllm calculator, /testllm memory\n");
  printf("  /optimize_tool_prompts  Optimize tool prompts (incremental: only new tools, ~10s)\n");
  printf("  /load <path>       Load Governor model\n");
  printf("  /tools             Show loaded Governor tools\n");
  printf("  /search <query>    Search conversation memory\n");
  printf("  /summary [n]       Summarize last n turns (default: 10)\n");
  printf("  /summarizeCache    Generate conversation summary and clear KV cache\n");
  printf("  /export <file>     Export memory to JSON file\n");
  printf("  /archive           Move old session files to archive/\n");
  printf("  /paths             List configured user paths\n");
  printf("  /setpath <label> <path>  Set a user path (e.g., /setpath Notes ~/Notes)\n");
  printf("  /safemode          Toggle file write permissions (safe mode on/off)\n");
  printf("  /testwhisper       Test Whisper with JFK sample audio\n");
  printf("  /transcribe        Start voice recording with Whisper STT\n");
  printf(
      "  /stoptranscribe    Stop recording and get transcript (saves to "
      "~/.ethervox/transcripts/)\n");
  printf("  /setlang <code>    Set STT language (e.g., en, es, zh, auto for detection)\n");
  printf(
      "  /translate         Toggle auto-translation to English on/off (for non-English speech)\n");
  printf("  /wakeword          Show wake word status and usage\n");
  printf("  /wakeon            Enable wake word detection (continuous listening)\n");
  printf("  /wakeoff           Disable wake word detection\n");
  printf("  /wakerecord        Record a wake word template for better accuracy\n");
  printf("  /conversation      Show voice conversation status\n");
  printf("  /convon            Enable voice conversation (Vosk + Piper)\n");
  printf("  /convoff           Disable voice conversation\n");
  printf("  /convtrigger       Manually trigger a conversation (for testing)\n");
  printf(
      "  /voice_training    Interactive pronunciation training (LLM generates text, you speak, "
      "system learns)\n");
  printf("  /models            List all available models and their status\n");
  printf("  /modelstatus <type> Check status of models (governor/whisper/vosk/piper)\n");
  printf("  /modeldownload <type> <name> Download a specific model\n");
  printf("  /modeldelete <type> <name>   Delete a model to free disk space\n");
  printf("  /stats             Show memory statistics\n");
  printf("  /startup <cmd>     Manage startup prompt (edit/show/reset)\n");
  printf("  /debug             Toggle debug logging on/off\n");
  printf("  /markdown          Toggle markdown formatting on/off\n");
  printf("  /toggle_tool_calls Toggle tool execution on/off (for meta-prompting)\n");
  printf("  /secret            Toggle secret mode (disable memory logging for privacy)\n");
  printf("  /clear             Clear conversation memory\n");
  printf("  /reset             Reset conversation (reload model)\n");
  printf("  /paste             Enter paste mode for multi-line input\n");
  printf("  /quit              Exit the program\n");
  printf("\nRuntime Directory: ~/.ethervox/\n");
  printf("  models/            Recommended location for GGUF model files\n");
  printf("  memory/            Conversation memory (persistent .jsonl files)\n");
  printf("  transcripts/       Voice recordings (saved by /stoptranscribe command)\n");
  printf("  reports/           Test reports, optimization logs, and crash logs\n");
  printf("  startup_prompt.txt Custom startup instruction\n");
  printf("  tool_prompts_*.json Optimized per-model tool descriptions\n");
  printf("\nPaste Mode: Type /paste to enter, then paste multi-line text.\n");
  printf("Exit with /end on its own line or Ctrl+D.\n");
  printf("\nOr just type a message to chat with the Governor.\n\n");
}

// Ensure ~/.ethervox directory structure exists
static void ensure_ethervox_directories(void) {
  const char* home = getenv("HOME");
  if (!home) {
    return;  // Can't create home-based directories
  }

  char path[512];

  // Create ~/.ethervox/
  snprintf(path, sizeof(path), "%s/.ethervox", home);
#ifdef _WIN32
  _mkdir(path);
#else
  mkdir(path, 0755);
#endif

  // Create ~/.ethervox/models/
  snprintf(path, sizeof(path), "%s/.ethervox/models", home);
#ifdef _WIN32
  _mkdir(path);
#else
  mkdir(path, 0755);
#endif

  // Create ~/.ethervox/memory/
  snprintf(path, sizeof(path), "%s/.ethervox/memory", home);
#ifdef _WIN32
  _mkdir(path);
#else
  mkdir(path, 0755);
#endif

  // Create ~/.ethervox/reports/
  snprintf(path, sizeof(path), "%s/.ethervox/reports", home);
#ifdef _WIN32
  _mkdir(path);
#else
  mkdir(path, 0755);
#endif
}

// Component test functions
static bool test_platform(ethervox_platform_t* platform) {
  printf("[TEST] Platform detection... ");
  ethervox_result_t result = ethervox_platform_register_hal(platform);
  if (ethervox_is_error(result)) {
    printf("[FAIL] FAILED\n");
    return false;
  }
  printf("[OK] OK (%s)\n", platform->info.platform_name);
  return true;
}

static bool test_memory(const char* test_dir) {
  printf("[TEST] Memory initialization... ");
  ethervox_memory_store_t test_mem;
  ethervox_result_t result = ethervox_memory_init(&test_mem, "test", test_dir);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    printf("[FAIL] FAILED: %s\n",
           ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    return false;
  }
  printf("[OK] OK\n");

  printf("[TEST] Memory storage... ");
  const char* tags[] = {"test"};
  uint64_t id;
  result = ethervox_memory_store_add(&test_mem, "Test entry", tags, 1, 0.8f, true, &id);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    printf("[FAIL] FAILED: %s\n",
           ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    ethervox_memory_cleanup(&test_mem);
    return false;
  }
  printf("[OK] OK (ID: %llu)\n", (unsigned long long)id);

  printf("[TEST] Memory search... ");
  ethervox_memory_search_result_t* results = NULL;
  uint32_t count = 0;
  result = ethervox_memory_search(&test_mem, "Test", NULL, 0, 5, &results, &count);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    printf("[FAIL] FAILED: %s\n",
           ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    ethervox_memory_cleanup(&test_mem);
    return false;
  }
  printf("[OK] OK (%u results)\n", count);
  free(results);

  ethervox_memory_cleanup(&test_mem);
  return true;
}

static bool test_governor(void) {
  printf("[TEST] Governor initialization... ");
  ethervox_tool_registry_t reg;
  ethervox_result_t result = ethervox_tool_registry_init(&reg, 8);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    printf("[FAIL] FAILED: %s\n",
           ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    return false;
  }

  ethervox_governor_t* gov = NULL;
  result = ethervox_governor_init(&gov, NULL, &reg);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    printf("[FAIL] FAILED: %s\n",
           ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    ethervox_tool_registry_cleanup(&reg);
    return false;
  }
  printf("[OK] OK\n");

  ethervox_governor_cleanup(gov);
  ethervox_tool_registry_cleanup(&reg);
  return true;
}

static bool test_memory_tools_registration(ethervox_governor_t* gov, ethervox_memory_store_t* mem) {
  printf("[TEST] Memory tools registration... ");
  // Memory tools are registered during init, just verify memory works
  if (!gov || !mem) {
    printf("[FAIL] FAILED (NULL pointers)\n");
    return false;
  }
  printf("[OK] OK\n");
  return true;
}

static void run_component_tests(const char* memory_dir) {
  printf("\n================================================\n");
  printf("  Component Tests\n");
  printf("================================================\n\n");

  int passed = 0;
  int total = 0;

  ethervox_platform_t platform = {0};
  total++;
  if (test_platform(&platform))
    passed++;

  total++;
  if (test_memory(memory_dir))
    passed++;

  total++;
  if (test_governor())
    passed++;

  printf("\n================================================\n");
  printf("  Results: %d/%d tests passed\n", passed, total);
  printf("================================================\n\n");
}

static void print_stats(ethervox_memory_store_t* memory) {
  printf("\n=== Memory Statistics ===\n");
  printf("Session ID:       %s\n", memory->session_id);
  printf("Current entries:  %u\n", memory->entry_count);
  printf("Total stored:     %llu\n", (unsigned long long)memory->total_memories_stored);
  printf("Total searches:   %llu\n", (unsigned long long)memory->total_searches);
  printf("Total exports:    %llu\n", (unsigned long long)memory->total_exports);
  if (memory->storage_filepath[0]) {
    printf("Storage file:     %s\n", memory->storage_filepath);
  } else {
    printf("Storage:          Memory-only (no persistence)\n");
  }
  printf("\n");
}

// Paste mode: accumulate multi-line input until /end or Ctrl+D
static char* read_paste_input(void) {
  // Allocate buffer for accumulated input (64KB max)
  size_t buffer_size = 65536;
  char* buffer = malloc(buffer_size);
  if (!buffer) {
    fprintf(stderr, "Memory allocation failed for paste mode\n");
    return NULL;
  }

  buffer[0] = '\0';
  size_t total_len = 0;

#if defined(__APPLE__) || defined(__linux__)
  // Readline-based paste mode
  printf("(Paste mode - type /end or press Ctrl+D to submit)\n");

  while (1) {
    char* line = readline("paste> ");

    if (!line) {
      // Ctrl+D (EOF) - exit paste mode and submit
      printf("\n");
      break;
    }

    // Check for /end command
    if (strcmp(line, "/end") == 0) {
      free(line);
      printf("\n");
      break;
    }

    // Append line to buffer with newline
    size_t line_len = strlen(line);
    if (total_len + line_len + 2 > buffer_size) {
      fprintf(stderr, "Paste buffer exceeded 64KB limit\n");
      free(line);
      free(buffer);
      return NULL;
    }

    if (total_len > 0) {
      buffer[total_len++] = '\n';
    }
    strcpy(buffer + total_len, line);
    total_len += line_len;
    buffer[total_len] = '\0';

    free(line);
  }
#else
  // Fallback paste mode (no readline)
  printf("(Paste mode - type /end or press Ctrl+D to submit)\n");
  char line[2048];

  while (1) {
    printf("paste> ");
    fflush(stdout);

    if (!fgets(line, sizeof(line), stdin)) {
      // Ctrl+D (EOF) - exit paste mode and submit
      printf("\n");
      break;
    }

    // Remove newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
      len--;
    }

    // Check for /end command
    if (strcmp(line, "/end") == 0) {
      printf("\n");
      break;
    }

    // Append line to buffer with newline
    if (total_len + len + 2 > buffer_size) {
      fprintf(stderr, "Paste buffer exceeded 64KB limit\n");
      free(buffer);
      return NULL;
    }

    if (total_len > 0) {
      buffer[total_len++] = '\n';
    }
    strcpy(buffer + total_len, line);
    total_len += len;
    buffer[total_len] = '\0';
  }
#endif

  return buffer;
}

static void handle_search(ethervox_memory_store_t* memory, const char* query) {
  ethervox_memory_search_result_t* results = NULL;
  uint32_t result_count = 0;

  ethervox_result_t ret =
      ethervox_memory_search(memory, query, NULL, 0, 10, &results, &result_count);

  if (ethervox_is_error(ret) || result_count == 0) {
    if (ethervox_is_error(ret)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      if (ctx && ctx->message) {
        printf("Search error: %s\n", ctx->message);
      }
    } else {
      printf("No results found for: %s\n", query);
    }
    return;
  }

  printf("\n=== Search Results (%u found) ===\n", result_count);
  for (uint32_t i = 0; i < result_count; i++) {
    printf("\n[%.2f] %s:\n", results[i].relevance,
           results[i].entry.is_user_message ? "User" : "Assistant");
    printf("  %s\n", results[i].entry.text);

    if (results[i].entry.tag_count > 0) {
      printf("  Tags: ");
      for (uint32_t t = 0; t < results[i].entry.tag_count; t++) {
        printf("%s%s", results[i].entry.tags[t], t < results[i].entry.tag_count - 1 ? ", " : "");
      }
      printf("\n");
    }
  }
  printf("\n");

  free(results);
}

static void handle_summary(ethervox_memory_store_t* memory, int window_size) {
  char* summary = NULL;
  char** key_points = NULL;
  uint32_t kp_count = 0;

  ethervox_result_t ret =
      ethervox_memory_summarize(memory, window_size, NULL, &summary, &key_points, &kp_count);

  if (ethervox_is_error(ret) || !summary) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    if (ctx && ctx->message) {
      printf("Failed to generate summary: %s\n", ctx->message);
    } else {
      printf("Failed to generate summary\n");
    }
    return;
  }

  printf("\n=== Conversation Summary ===\n");
  printf("%s\n", summary);

  if (kp_count > 0) {
    printf("\nKey Points:\n");
    for (uint32_t i = 0; i < kp_count; i++) {
      printf("  • %s\n", key_points[i]);
      free(key_points[i]);
    }
    free(key_points);
  }
  printf("\n");

  free(summary);
}

static void handle_export(ethervox_memory_store_t* memory, const char* filepath) {
  uint64_t bytes_written = 0;

  ethervox_result_t ret = ethervox_memory_export(memory, filepath, "json", &bytes_written);

  if (ethervox_is_error(ret)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    if (ctx && ctx->message) {
      printf("Failed to export memory: %s\n", ctx->message);
    } else {
      printf("Failed to export memory to: %s\n", filepath);
    }
    return;
  }

  printf("Exported %llu bytes to: %s\n", (unsigned long long)bytes_written, filepath);
}

static void store_message(ethervox_memory_store_t* memory, const char* text, bool is_user,
                          float importance) {
  const char* tags[] = {"conversation"};
  uint64_t memory_id;

  ethervox_result_t result =
      ethervox_memory_store_add(memory, text, tags, 1, importance, is_user, &memory_id);
  if (ethervox_is_error(result) && g_debug_enabled) {
    fprintf(stderr, "[Memory] Failed to store message: Error %d occurred\n", result);
  }
}

// Convert markdown formatting to ANSI escape codes for terminal display
static char* markdown_to_ansi(const char* text) {
  if (!text)
    return NULL;

  size_t len = strlen(text);
  // Allocate generous buffer (3x original for escape codes)
  char* output = malloc(len * 3 + 1);
  if (!output)
    return strdup(text);

  const char* src = text;
  char* dst = output;
  bool in_bold = false;
  bool in_italic = false;
  bool in_code = false;

  while (*src) {
    // Bold: **text**
    if (src[0] == '*' && src[1] == '*') {
      if (in_bold) {
        // End bold - need to reapply other active styles
        strcpy(dst, "\033[22m");  // Turn off bold specifically
        dst += 5;
        if (in_italic) {
          strcpy(dst, "\033[3m");  // Reapply italic
          dst += 4;
        }
        in_bold = false;
      } else {
        // Start bold
        strcpy(dst, "\033[1m");
        dst += 4;
        in_bold = true;
      }
      src += 2;
      continue;
    }

    // Italic: *text* (but not if it's part of **)
    if (*src == '*' && !(src > text && src[-1] == '*') && !(src[1] == '*')) {
      if (in_italic) {
        // End italic/underline
        strcpy(dst, "\033[24m");  // Turn off underline
        dst += 5;
        if (in_bold) {
          strcpy(dst, "\033[1m");  // Reapply bold
          dst += 4;
        }
        in_italic = false;
      } else {
        // Start italic/underline (use underline for better compatibility)
        strcpy(dst, "\033[4m");
        dst += 4;
        in_italic = true;
      }
      src++;
      continue;
    }

    // Code/monospace: `text`
    if (*src == '`') {
      if (in_code) {
        // End code
        strcpy(dst, "\033[22m\033[39m");  // Turn off dim and restore color
        dst += 10;
        // Reapply bold/italic if needed
        if (in_bold) {
          strcpy(dst, "\033[1m");
          dst += 4;
        }
        if (in_italic) {
          strcpy(dst, "\033[3m");
          dst += 4;
        }
        in_code = false;
      } else {
        strcpy(dst, "\033[2m\033[90m");  // Dim + gray
        dst += 9;
        in_code = true;
      }
      src++;
      continue;
    }

    // Copy regular character
    *dst++ = *src++;
  }

  // Reset any remaining formatting
  if (in_bold || in_italic || in_code) {
    strcpy(dst, "\033[22m\033[24m\033[39m");  // Reset bold, underline, color
    dst += 15;
  }

  *dst = '\0';
  return output;
}

static void print_tools(ethervox_governor_t* g_governor) {
  if (!g_governor) {
    printf("No Governor instance available\n");
    return;
  }

  ethervox_tool_registry_t* registry = ethervox_governor_get_registry(g_governor);
  if (!registry) {
    printf("No tool registry available\n");
    return;
  }

  printf("\n=== Governor Tools (%u loaded) ===\n\n", registry->tool_count);

  // Group tools by category
  printf("Compute Tools:\n");
  for (uint32_t i = 0; i < registry->tool_count; i++) {
    const ethervox_tool_t* tool = &registry->tools[i];
    if (strstr(tool->name, "calculator_") || strstr(tool->name, "time_") ||
        strstr(tool->name, "percentage_")) {
      printf("  %-25s %s\n", tool->name, tool->description);
    }
  }

  printf("\nMemory Tools:\n");
  for (uint32_t i = 0; i < registry->tool_count; i++) {
    const ethervox_tool_t* tool = &registry->tools[i];
    if (strstr(tool->name, "memory_")) {
      printf("  %-25s %s\n", tool->name, tool->description);
    }
  }

  printf("\nFile Tools:\n");
  for (uint32_t i = 0; i < registry->tool_count; i++) {
    const ethervox_tool_t* tool = &registry->tools[i];
    if (strstr(tool->name, "file_")) {
      const char* access_note = tool->requires_confirmation ? " [WRITE ACCESS]" : "";
      printf("  %-25s %s%s\n", tool->name, tool->description, access_note);
    }
  }

  printf("\n");
}
static void print_transcript_summary(const ethervox_voice_session_t* session,
                                     const char* transcript) {
  if (!transcript || transcript[0] == '\0') {
    printf("⚠️  Transcript is empty (no speech detected yet).\n\n");
    return;
  }
  printf("📝 Transcript:\n");
  printf("==================================================================\n");
  printf("%s\n", transcript);
  printf("==================================================================\n\n");
  if (!session) {
    return;
  }
  if (session->last_transcript_file[0] != '\0') {
    printf("Transcript saved to: %s\n", session->last_transcript_file);
  } else {
    printf("Error - Transcript not saved to file (no home directory found)\n");
    printf("   But stored in memory - use /search voice to retrieve\n\n");
  }
}

static bool stop_transcription_and_show(ethervox_voice_session_t* session) {
  if (!session) {
    return false;
  }
  const char* transcript = NULL;
  ethervox_result_t result = ethervox_voice_tools_stop_listen(session, &transcript);
  if (ethervox_is_success(result)) {
    print_transcript_summary(session, transcript);
    return true;
  }
  return false;
}

static void handle_pending_sigint_stop(void) {
  if (!g_sigint_stop_transcribe) {
    return;
  }
  g_sigint_stop_transcribe = 0;
  if (!g_transcription_session) {
    g_running = false;
    printf("\n\nShutting down gracefully...\n");
    return;
  }
  printf("\n⏹️  Ctrl+C detected - stopping recording and transcribing with Whisper...\n\n");
  if (!stop_transcription_and_show(g_transcription_session)) {
    printf("[FAIL] Failed to stop recording after Ctrl+C\n");
  } else {
    printf("(Recording cancelled via Ctrl+C – continue with new commands)\n");
  }
}

static void process_command(const char* line, ethervox_memory_store_t* memory,
                            ethervox_governor_t* g_governor, ethervox_path_config_t* path_config,
                            ethervox_file_tools_config_t* file_config, void* voice_session,
                            bool* quit_flag) {
  // Trim leading whitespace
  while (*line == ' ' || *line == '\t')
    line++;

  if (line[0] == '\0')
    return;

  // Trim trailing whitespace by creating a copy
  char trimmed[2048];
  strncpy(trimmed, line, sizeof(trimmed) - 1);
  trimmed[sizeof(trimmed) - 1] = '\0';
  size_t len = strlen(trimmed);
  while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\t')) {
    trimmed[--len] = '\0';
  }
  line = trimmed;

  // Debug: Log what we're processing (but not /debug itself to avoid confusion)
  if (g_debug_enabled && line[0] == '/' && strcmp(line, "/debug") != 0) {
    fprintf(stderr, "[DEBUG] Processing slash command: '%s' (len=%zu)\n", line, strlen(line));
  }

  if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) {
    *quit_flag = true;
    return;
  }

  if (strcmp(line, "/help") == 0) {
    print_help();
    return;
  }

  if (strcmp(line, "/settings") == 0) {
    if (ethervox_settings_menu_available()) {
      ethervox_settings_t settings;
      init_menu_settings(&settings, NULL, NULL);

      // Show menu with model path and reload callback
      const char* current_model = (g_loaded_model_path[0] != '\0') ? g_loaded_model_path : NULL;
      if (ethervox_settings_menu_show(&settings, current_model, reload_model_callback, g_governor,
                                      tts_reload_callback, NULL) == 0) {
        // Apply changed settings
        g_debug_enabled = settings.debug_enabled;
        g_quiet_mode = settings.quiet_mode;
        g_streaming_enabled = settings.streaming_enabled;

        // Apply wake word settings
        if (settings.wake_word_enabled && !g_wake_enabled) {
          // User enabled wake word in settings - initialize if needed
          if (!g_wake_runtime) {
            g_wake_runtime = (ethervox_wake_runtime_t*)calloc(1, sizeof(ethervox_wake_runtime_t));
            if (g_wake_runtime) {
              ethervox_wake_config_t config = ethervox_wake_get_default_config();
              ethervox_result_t result = ethervox_wake_init(g_wake_runtime, &config);
              if (ethervox_is_success(result)) {
                g_wake_enabled = true;
                printf("[OK] Wake word detection enabled\n");
              } else {
                const ethervox_error_context_t* ctx = ethervox_error_get_context();
                printf("[ERROR] Failed to initialize wake word: %s\n",
                       ctx && ctx->message ? ctx->message : ethervox_error_string(result));
                free(g_wake_runtime);
                g_wake_runtime = NULL;
              }
            }
          } else {
            g_wake_enabled = true;
            printf("[OK] Wake word detection enabled\n");
          }
        } else if (!settings.wake_word_enabled && g_wake_enabled) {
          // User disabled wake word
          g_wake_enabled = false;
          printf("[OK] Wake word detection disabled\n");
        }

        // Note: engineering_mode and log_level are local to main(), can't be changed here

        printf("\n[OK] Settings applied (debug=%s, quiet=%s)\n", g_debug_enabled ? "on" : "off",
               g_quiet_mode ? "on" : "off");
      }
    } else {
      printf("Settings menu not available on this platform\n");
    }
    return;
  }

  if (strcmp(line, "/config") == 0) {
    ethervox_settings_print(&g_settings);
    printf("Settings file: %s\n", ethervox_settings_get_default_path());
    printf("\nCommands:\n");
    printf("  /config save       Save current settings to file\n");
    printf("  /config load       Reload settings from file\n");
    printf("  /config reset      Reset to default settings\n");
    printf("  /config export     Export settings as JSON\n");
    return;
  }

  if (strncmp(line, "/config ", 8) == 0) {
    const char* subcmd = line + 8;

    if (strcmp(subcmd, "save") == 0) {
      ethervox_result_t result = ethervox_settings_save(&g_settings, NULL);
      if (ethervox_is_success(result)) {
        printf("[OK] Settings saved to %s\n", ethervox_settings_get_default_path());
      } else {
        printf("[FAIL] Failed to save settings: %s\n", ethervox_error_string(result));
      }
    } else if (strcmp(subcmd, "load") == 0) {
      ethervox_result_t result = ethervox_settings_load(&g_settings, NULL);
      if (ethervox_is_success(result)) {
        printf("[OK] Settings loaded from %s\n", ethervox_settings_get_default_path());
        ethervox_settings_print(&g_settings);
      } else {
        printf("[FAIL] Failed to load settings: %s\n", ethervox_error_string(result));
      }
    } else if (strcmp(subcmd, "reset") == 0) {
      g_settings = ethervox_settings_get_defaults();
      printf("[OK] Settings reset to defaults\n");
      ethervox_settings_print(&g_settings);
    } else if (strcmp(subcmd, "export") == 0) {
      char* json = ethervox_settings_export(&g_settings);
      if (json) {
        printf("\n%s\n", json);
        free(json);
      } else {
        printf("[FAIL] Failed to export settings\n");
      }
    } else {
      printf("Unknown /config subcommand: %s\n", subcmd);
      printf("Try: save, load, reset, or export\n");
    }
    return;
  }

  if (strcmp(line, "/report") == 0) {
    printf("\n╔═══════════════════════════════════╗\n");
    printf("║  Bug & Feature Report Submission  ║\n");
    printf("╚═══════════════════════════════════╝\n\n");

    // Prompt for type
    char type_input[10];
    printf("Report type - (b)ug or (f)eature? ");
    fflush(stdout);
    if (!fgets(type_input, sizeof(type_input), stdin)) {
      printf("[FAIL] Input cancelled\n");
      return;
    }

    ethervox_report_type_t type = (type_input[0] == 'f' || type_input[0] == 'F')
                                      ? ETHERVOX_REPORT_FEATURE
                                      : ETHERVOX_REPORT_BUG;

    // Get title
    char title[128];
    printf("Title: ");
    fflush(stdout);
    if (!fgets(title, sizeof(title), stdin)) {
      printf("[FAIL] Input cancelled\n");
      return;
    }
    title[strcspn(title, "\n")] = 0;  // Remove newline

    if (strlen(title) == 0) {
      printf("[FAIL] Title cannot be empty\n");
      return;
    }

    // Get description (multi-line)
    printf("\nDescription (type /end on its own line to finish):\n");
    char description[2048] = {0};
    char line_buf[256];
    while (fgets(line_buf, sizeof(line_buf), stdin)) {
      if (strcmp(line_buf, "/end\n") == 0)
        break;
      size_t remaining = sizeof(description) - strlen(description) - 1;
      if (remaining > 0) {
        strncat(description, line_buf, remaining);
      }
    }

    if (strlen(description) == 0) {
      printf("[FAIL] Description cannot be empty\n");
      return;
    }

    // Ask about system info
    char include_sys[10];
    printf("\nInclude system information? (Y/n): ");
    fflush(stdout);
    if (!fgets(include_sys, sizeof(include_sys), stdin)) {
      include_sys[0] = 'Y';  // Default to yes if cancelled
    }
    bool include_info = (include_sys[0] != 'n' && include_sys[0] != 'N');

    // Submit report
    printf("\n🚀 Submitting %s report...\n", type == ETHERVOX_REPORT_BUG ? "bug" : "feature");

    ethervox_report_result_t result;
    ethervox_result_t submit_result =
        ethervox_report_submit(type, title, description, include_info, &result);
    if (ethervox_is_success(submit_result)) {
      printf("\n[OK] Report submitted successfully!\n");
      printf("  Issue URL: %s\n\n", result.issue_url);
      printf("Thank you for helping improve EthervoxAI! 🙏\n");
    } else {
      printf("\n[FAIL] Failed to submit report: %s\n", result.error_message);
      if (result.http_status > 0) {
        printf("  HTTP Status: %d\n", result.http_status);
      }
      printf("\nPlease try again or send an email to Tim with details: timkos@live.at\n");
      printf(
          "\nInclude any details that might help reproduce the issue along with the version "
          "number.\n");
    }
    return;
  }

  if (strcmp(line, "/archive") == 0) {
    uint32_t archived = 0;
    ethervox_result_t result = ethervox_memory_archive_sessions(memory, &archived);
    if (ethervox_is_success(result)) {
      printf("[OK] Archived %u session file(s) to archive/ subdirectory\n", archived);
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("[FAIL] Failed to archive sessions: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    }
    return;
  }

  if (strcmp(line, "/paths") == 0) {
    ethervox_user_path_t* paths = NULL;
    uint32_t count = 0;

    if (ethervox_is_success(ethervox_path_config_list(path_config, &paths, &count))) {
      if (count == 0) {
        printf("No paths configured. Use /setpath to add paths.\n");
      } else {
        printf("\n╭---------------------------------------------╮\n");
        printf("|          Configured User Paths              |\n");
        printf("+---------------------------------------------+\n");
        for (uint32_t i = 0; i < count; i++) {
          const char* status = paths[i].verified ? "[OK]" : "[FAIL]";
          printf("| %s %-15s |\n", status, paths[i].label);
          printf("|   %s\n", paths[i].path);
          if (paths[i].description[0] != '\0') {
            printf("|   %s\n", paths[i].description);
          }
          if (i < count - 1) {
            printf("+---------------------------------------------+\n");
          }
        }
        printf("╰---------------------------------------------╯\n\n");

        // Check for unverified paths and suggest configuration
        uint32_t unverified = 0;
        for (uint32_t i = 0; i < count; i++) {
          if (!paths[i].verified)
            unverified++;
        }
        if (unverified > 0) {
          printf("Note: %u path(s) marked [FAIL] don't exist. Use /setpath to configure.\n",
                 unverified);
        }
      }
      free(paths);
    } else {
      printf("[FAIL] Failed to list paths\n");
    }
    return;
  }

  if (strncmp(line, "/setpath ", 9) == 0) {
    const char* args = line + 9;

    // Parse: /setpath <label> <path> [description]
    char label[64] = {0};
    char path[ETHERVOX_FILE_MAX_PATH] = {0};
    char description[256] = {0};

    // Find first space (after label)
    const char* space1 = strchr(args, ' ');
    if (!space1) {
      printf("Usage: /setpath <label> <path> [description]\n");
      printf("Example: /setpath Notes /Users/tim/Notes \"My personal notes\"\n");
      return;
    }

    size_t label_len = space1 - args;
    if (label_len >= sizeof(label))
      label_len = sizeof(label) - 1;
    strncpy(label, args, label_len);

    // Skip whitespace
    const char* path_start = space1 + 1;
    while (*path_start == ' ')
      path_start++;

    // Check if path is quoted
    const char* path_end;
    if (*path_start == '"') {
      path_start++;
      path_end = strchr(path_start, '"');
      if (!path_end) {
        printf("Error: Unmatched quote in path\n");
        return;
      }
    } else {
      // Find next space or end of string
      path_end = strchr(path_start, ' ');
      if (!path_end)
        path_end = path_start + strlen(path_start);
    }

    size_t path_len = path_end - path_start;
    if (path_len >= sizeof(path))
      path_len = sizeof(path) - 1;
    strncpy(path, path_start, path_len);

    // Check for optional description
    const char* desc_start = path_end;
    if (*desc_start == '"')
      desc_start++;  // Skip closing quote
    while (*desc_start == ' ')
      desc_start++;

    if (*desc_start != '\0') {
      // Remove surrounding quotes if present
      if (*desc_start == '"') {
        desc_start++;
        const char* desc_end = strrchr(desc_start, '"');
        if (desc_end) {
          size_t desc_len = desc_end - desc_start;
          if (desc_len >= sizeof(description))
            desc_len = sizeof(description) - 1;
          strncpy(description, desc_start, desc_len);
        }
      } else {
        strncpy(description, desc_start, sizeof(description) - 1);
      }
    }

    ethervox_result_t result =
        ethervox_path_config_set(path_config, label, path, description[0] ? description : NULL);
    if (ethervox_is_success(result)) {
      printf("[OK] Path configured: %s -> %s\n", label, path);
    } else {
      printf("[FAIL] Failed to set path: %s\n", ethervox_error_string(result));
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      if (ctx && ctx->message) {
        printf("  %s\n", ctx->message);
      }
    }
    return;
  }

  if (strcmp(line, "/stats") == 0) {
    print_stats(memory);
    return;
  }

  if (strcmp(line, "/tools") == 0) {
    print_tools(g_governor);
    return;
  }

  if (strcmp(line, "/debug") == 0) {
    g_debug_enabled = !g_debug_enabled;
    g_ethervox_debug_enabled = g_debug_enabled;  // Also control legacy debug flag
    if (g_debug_enabled) {
      ethervox_log_set_level(ETHERVOX_LOG_LEVEL_DEBUG);
      printf("Debug logging: ENABLED (level=%d)\n", ethervox_log_get_level());
      fprintf(stderr, "[TEST] If you see this, stderr logging works\n");
    } else {
      // Restore to INFO if not in quiet mode, OFF if in quiet mode
      ethervox_log_set_level(g_quiet_mode ? ETHERVOX_LOG_LEVEL_OFF : ETHERVOX_LOG_LEVEL_INFO);
      printf("Debug logging: DISABLED\n");
    }
    return;
  }

  if (strcmp(line, "/markdown") == 0) {
    g_markdown_enabled = !g_markdown_enabled;
    printf("Markdown formatting: %s\n", g_markdown_enabled ? "ENABLED" : "DISABLED");
    return;
  }

  if (strcmp(line, "/toggle_tool_calls") == 0) {
    static bool tool_execution_enabled = true;
    tool_execution_enabled = !tool_execution_enabled;
    ethervox_governor_set_tool_execution(g_governor, tool_execution_enabled);
    printf("Tool execution: %s\n", tool_execution_enabled ? "ENABLED" : "DISABLED");
    if (!tool_execution_enabled) {
      printf("  Tools will be shown in responses but not executed\n");
      printf("  Use this when asking LLM to write tool syntax examples\n");
    }
    return;
  }

  if (strcmp(line, "/secret") == 0) {
    static bool secret_mode_enabled = false;
    secret_mode_enabled = !secret_mode_enabled;

    ethervox_memory_set_privacy_mode(secret_mode_enabled);

    if (secret_mode_enabled) {
      printf("🔒 SECRET MODE: ENABLED\n");
      printf("   Conversations will NOT be saved to memory.\n");
      printf("   This session is private - nothing will be logged to disk.\n");
      printf("   Use /secret again to return to normal mode.\n");
    } else {
      printf("💾 SECRET MODE: DISABLED\n");
      printf("   Normal memory logging resumed.\n");
      printf("   Conversations will be saved to ~/.ethervox/memory/\n");
    }
    return;
  }

  if (strcmp(line, "/safemode") == 0) {
    if (!file_config) {
      printf("[FAIL] File tools not initialized\n");
      return;
    }

    // Toggle safe mode
    bool was_safe = (file_config->access_mode == ETHERVOX_FILE_ACCESS_READ_ONLY);
    file_config->access_mode =
        was_safe ? ETHERVOX_FILE_ACCESS_READ_WRITE : ETHERVOX_FILE_ACCESS_READ_ONLY;

    if (file_config->access_mode == ETHERVOX_FILE_ACCESS_READ_ONLY) {
      printf("🔒 Safe Mode: ENABLED (read-only)\n");
      printf("   LLM can explore and read files but cannot modify them.\n");
      printf("   This is similar to 'plan mode' - use for safe exploration.\n");
    } else {
      printf("🔓 Safe Mode: DISABLED (read-write)\n");
      printf("   LLM can read and write files when needed.\n");
      printf("   ⚠️  Exercise caution with file modifications.\n");
    }
    return;
  }

  // Voice commands
  if (strcmp(line, "/testwhisper") == 0) {
    // Test Whisper infrastructure with JFK sample
    printf("\n🧪 Testing Whisper Infrastructure...\n\n");

    if (!voice_session) {
      printf("❌ Voice tools not initialized\n");
      printf("   This should have been initialized at startup.\n");
      printf("   Check the startup logs for errors.\n");
      return;
    }

    ethervox_voice_session_t* session = (ethervox_voice_session_t*)voice_session;
    ethervox_result_t test_result = ethervox_whisper_test_jfk(&session->stt_runtime);

    if (ethervox_is_success(test_result)) {
      printf("\n✅ Whisper test PASSED!\n");
      printf("   The STT infrastructure is working correctly.\n");
      printf("   You can now use /transcribe for live recording.\n");
    } else {
      printf("\n❌ Whisper test FAILED\n");
      printf("   Check the logs above for details.\n");
    }
    return;
  }

  if (strcmp(line, "/transcribe") == 0) {
    if (!voice_session) {
      printf("[FAIL] Voice tools not initialized\n");
      return;
    }

    if (ethervox_voice_tools_is_recording(voice_session)) {
      printf("⚠️  Already recording! Use /stoptranscribe to finish.\n");
      return;
    }

    printf("🎤 Starting voice recording with Whisper STT...\n");
    printf("   Speak now. Use /stoptranscribe when finished.\n");
    printf("   (Speaker detection enabled - pauses and energy shifts tracked)\n\n");

    ethervox_result_t result = ethervox_voice_tools_start_listen(voice_session);
    if (ethervox_is_error(result)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("[FAIL] Failed to start voice recording: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    }
    return;
  }

  // Set language
  if (strncmp(line, "/setlang", 8) == 0) {
    if (!voice_session) {
      printf("⚠️  Voice tools not initialized\n");
      return;
    }

    const char* lang = line + 8;
    // Skip whitespace
    while (*lang == ' ' || *lang == '\t')
      lang++;

    if (*lang == '\0') {
      printf("Usage: /setlang <language>\n");
      printf("Examples:\n");
      printf("  /setlang auto    - Auto-detect language\n");
      printf("  /setlang en      - English\n");
      printf("  /setlang es      - Spanish\n");
      printf("  /setlang zh      - Chinese\n");
      printf("  /setlang fr      - French\n");
      printf("  /setlang de      - German\n");
      printf("  /setlang ja      - Japanese\n");
      printf("  /setlang ko      - Korean\n");
      printf("  /setlang ar      - Arabic\n");
      printf("  /setlang hi      - Hindi\n");
      return;
    }

    ethervox_voice_session_t* session = (ethervox_voice_session_t*)voice_session;
    ethervox_result_t result = ethervox_stt_set_language(&session->stt_runtime, lang);
    if (ethervox_is_success(result)) {
      if (strcmp(lang, "auto") == 0) {
        printf("🌍 Language detection: AUTO (will detect language automatically)\n");
      } else {
        printf("🌍 Language set to: %s\n", lang);
      }
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("❌ Failed to set language to %s: %s\n", lang,
             ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    }
    return;
  }

  // Toggle translation
  if (strcmp(line, "/translate") == 0) {
    if (!voice_session) {
      printf("⚠️  Voice tools not initialized\n");
      return;
    }

    ethervox_voice_session_t* session = (ethervox_voice_session_t*)voice_session;

    // Toggle the translation setting
    session->stt_runtime.config.translate_to_english =
        !session->stt_runtime.config.translate_to_english;

    if (session->stt_runtime.config.translate_to_english) {
      printf("🌐 Translation ENABLED: Non-English speech will be translated to English\n");
    } else {
      printf("🎤 Translation DISABLED: Speech will be transcribed in original language\n");
    }

    // Note: This only affects new recordings, not ongoing ones
    if (session->is_recording) {
      printf("⚠️  Note: Change will apply to next recording session\n");
    }

    return;
  }

  if (strcmp(line, "/stoptranscribe") == 0) {
    if (!voice_session) {
      printf("[FAIL] Voice tools not initialized\n");
      return;
    }

    printf("⏹️  Stopping recording and transcribing with Whisper...\n\n");
    ethervox_voice_session_t* session = (ethervox_voice_session_t*)voice_session;
    if (!stop_transcription_and_show(session)) {
      printf("⚠️  Not currently recording. Use /transcribe to start.\n");
    }
    return;
  }

  // Wake word commands
  if (strcmp(line, "/wakeword") == 0) {
    printf("\n🎤 Wake Word Detection Status\n");
    printf("=============================\n");
    if (g_wake_runtime && g_wake_runtime->is_initialized) {
      printf("State:       %s\n", g_wake_enabled ? "ENABLED (listening)" : "DISABLED");
      printf("Wake word:   \"%s\"\n", g_wake_runtime->config.wake_word);
      printf("Sensitivity: %.2f (0.0 = strict, 1.0 = loose)\n", g_wake_runtime->config.sensitivity);
      printf("Method:      Keyword spotting (VAD + syllable + template matching)\n");
      if (g_wake_enabled) {
        printf("\n💡 Wake word is actively listening in the background\n");
        printf("   Say \"%s\" to trigger /transcribe\n", g_wake_runtime->config.wake_word);
      } else {
        printf("\n💡 Use /wakeon to enable continuous wake word listening\n");
      }
    } else {
      printf("State:       NOT INITIALIZED\n");
      printf("\n💡 Use /wakeon to initialize and enable wake word detection\n");
    }
    printf("\nCommands:\n");
    printf("  /wakeon       Enable wake word detection\n");
    printf("  /wakeoff      Disable wake word detection\n");
    printf("  /wakerecord   Record a template for better accuracy\n");
    printf("\n");
    return;
  }

  if (strcmp(line, "/wakeon") == 0) {
    if (!g_wake_runtime) {
      printf("🎤 Initializing wake word detector...\n");
      g_wake_runtime = (ethervox_wake_runtime_t*)calloc(1, sizeof(ethervox_wake_runtime_t));
      if (!g_wake_runtime) {
        printf("❌ Failed to allocate wake word runtime\n");
        return;
      }

      ethervox_wake_config_t config = ethervox_wake_get_default_config();
      config.sensitivity = 0.6f;  // Default sensitivity

      ethervox_result_t result = ethervox_wake_init(g_wake_runtime, &config);
      if (ethervox_is_error(result)) {
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        printf("❌ Failed to initialize wake word detector: %s\n",
               ctx && ctx->message ? ctx->message : ethervox_error_string(result));
        free(g_wake_runtime);
        g_wake_runtime = NULL;
        return;
      }
    }

    // Start the wake word listening thread if not already running
    if (!g_wake_thread_running) {
      g_wake_thread_running = true;
      if (pthread_create(&g_wake_thread, NULL, wake_word_listen_thread, NULL) != 0) {
        printf("❌ Failed to start wake word listening thread\n");
        g_wake_thread_running = false;
        return;
      }
      pthread_detach(g_wake_thread);
      printf("[OK] Wake word listening thread started\n");
    }

    pthread_mutex_lock(&g_wake_mutex);
    g_wake_enabled = true;
    pthread_mutex_unlock(&g_wake_mutex);

    printf("[OK] Wake word detection ENABLED\n");
    printf("  Listening for: \"%s\"\n", g_wake_runtime->config.wake_word);
    printf("  Sensitivity: %.2f\n", g_wake_runtime->config.sensitivity);
    printf("\n💡 Say the wake word to automatically trigger conversation\n");
    printf("   Use /wakerecord to improve accuracy with a template\n");
    return;
  }

  if (strcmp(line, "/wakeoff") == 0) {
    if (!g_wake_runtime) {
      printf("⚠️  Wake word detection not initialized\n");
      return;
    }

    pthread_mutex_lock(&g_wake_mutex);
    g_wake_enabled = false;
    pthread_mutex_unlock(&g_wake_mutex);

    printf("[OK] Wake word detection DISABLED\n");
    printf("  (microphone still listening, use /wakeon to re-enable)\n");
    return;
  }

  if (strcmp(line, "/wakerecord") == 0) {
    printf("\n🎙️  Wake Word Template Recording\n");
    printf("=================================\n");

    if (!g_wake_runtime) {
      printf("❌ Wake word detector not initialized\n");
      printf("   Use /wakeon first to initialize the detector\n");
      return;
    }

    if (!voice_session) {
      printf("❌ Voice tools not initialized\n");
      return;
    }

    printf("This will record you saying the wake word to create a reference template.\n");
    printf("Template matching improves accuracy from ~75%% to ~90%%.\n\n");
    printf("Instructions:\n");
    printf("  1. Recording will start in 3 seconds\n");
    printf("  2. Say \"%s\" clearly\n", g_wake_runtime->config.wake_word);
    printf("  3. Recording stops automatically after 3 seconds\n");
    printf("\nPress Enter to start...");

    char dummy[10];
    if (fgets(dummy, sizeof(dummy), stdin) == NULL) {
      printf("❌ Recording cancelled\n");
      return;
    }

    printf("🔴 Recording in 3...\n");
    sleep(1);
    printf("🔴 Recording in 2...\n");
    sleep(1);
    printf("🔴 Recording in 1...\n");
    sleep(1);
    printf("🔴 RECORDING NOW - Say \"%s\"\n", g_wake_runtime->config.wake_word);

    ethervox_voice_session_t* session = (ethervox_voice_session_t*)voice_session;

    // Start recording
    ethervox_result_t result = ethervox_voice_tools_start_listen(session);
    if (ethervox_is_error(result)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("❌ Failed to start recording: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(result));
      return;
    }

    // Record for 3 seconds
    sleep(3);

    // Stop recording
    const char* transcript = NULL;
    ethervox_result_t stop_result = ethervox_voice_tools_stop_listen(session, &transcript);
    if (ethervox_is_error(stop_result)) {
      fprintf(stderr, "[WARN] Error stopping recording\n");
    }

    printf("[OK] Recording stopped\n");
    printf("\n💡 Template recording feature requires direct audio access.\n");
    printf("   For now, the detector will use heuristics (syllable + energy pattern).\n");
    printf("   Expected accuracy: ~75-80%% in quiet environments.\n");
    printf("\n   To enable template matching:\n");
    printf("   1. Audio buffer access needs to be exposed from voice_tools\n");
    printf("   2. Extract wake word segment from recording\n");
    printf("   3. Call ethervox_wake_record_template() with audio data\n");
    printf("\n");
    return;
  }

  // ========== VOICE CONVERSATION COMMANDS ==========

  if (strcmp(line, "/conversation") == 0) {
    printf("\n🗣️  Voice Conversation Status\n");
    printf("========================================================\n");

    if (!g_conversation_session) {
      printf("Status: Not initialized\n\n");
      printf("Voice conversation provides real-time interaction with the LLM:\n");
      printf("  • Wake word detection → Triggers listening\n");
      printf("  • Vosk STT → Processes speech in real-time (<500ms)\n");
      printf("  • Governor → Generates response\n");
      printf("  • Piper TTS → Speaks response naturally\n\n");
      printf("💡 Use /convon to enable voice conversation\n");
    } else {
      ethervox_conversation_state_t state = ethervox_conversation_get_state(g_conversation_session);
      const char* state_str = "Unknown";
      switch (state) {
        case ETHERVOX_CONV_STATE_UNINITIALIZED:
          state_str = "Uninitialized";
          break;
        case ETHERVOX_CONV_STATE_IDLE:
          state_str = "Idle (waiting for wake word)";
          break;
        case ETHERVOX_CONV_STATE_LISTENING:
          state_str = "Listening (capturing speech)";
          break;
        case ETHERVOX_CONV_STATE_PROCESSING:
          state_str = "Processing (querying LLM)";
          break;
        case ETHERVOX_CONV_STATE_SPEAKING:
          state_str = "Speaking (playing response)";
          break;
        case ETHERVOX_CONV_STATE_ERROR:
          state_str = "Error";
          break;
      }

      bool is_active = ethervox_conversation_is_active(g_conversation_session);
      printf("Status: %s\n", state_str);
      printf("Active: %s\n", is_active ? "Yes" : "No");
      printf("\nConfiguration:\n");
      printf("  STT Backend: Vosk (real-time)\n");
      printf("  TTS Backend: Piper (neural)\n");
      printf("  Listen timeout: %d ms\n", 5000);  // From default config
      printf("  Conversation timeout: %d ms\n", 30000);
      printf("\n");
      printf("Commands:\n");
      printf("  /convon       Enable voice conversation\n");
      printf("  /convoff      Disable voice conversation\n");
      printf("  /convtrigger  Manually trigger (for testing)\n");
      printf("\n");
    }
    return;
  }

  if (strcmp(line, "/convon") == 0) {
    if (!g_governor) {
      printf("❌ Governor not initialized. Please wait for system startup.\n");
      return;
    }

    if (!g_conversation_session) {
      printf("🎤 Initializing voice conversation session...\n");

      ethervox_conversation_config_t config = ethervox_conversation_get_default_config();

      // Initialize with governor instance
      g_conversation_session = ethervox_conversation_init(&config, g_governor);
      if (!g_conversation_session) {
        printf("❌ Failed to initialize conversation session\n");
        return;
      }

      printf("[OK] Conversation session initialized with Governor\n");
    }

    // Start the conversation thread
    ethervox_result_t result = ethervox_conversation_start(g_conversation_session);
    if (ethervox_is_error(result)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("❌ Failed to start conversation thread: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(result));
      return;
    }

    printf("[OK] Voice conversation ENABLED\n");
    printf("  Listening for wake word triggers...\n");
    printf("\n💡 Make sure wake word detection is enabled (/wakeon)\n");
    printf("   Say the wake word to start a conversation!\n");
    return;
  }

  if (strcmp(line, "/convoff") == 0) {
    if (!g_conversation_session) {
      printf("⚠️  Voice conversation not initialized\n");
      return;
    }

    ethervox_result_t result = ethervox_conversation_stop(g_conversation_session);
    if (ethervox_is_error(result)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("❌ Failed to stop conversation: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(result));
      return;
    }

    printf("[OK] Voice conversation DISABLED\n");
    printf("  (session still initialized, use /convon to re-enable)\n");
    return;
  }

  if (strcmp(line, "/convtrigger") == 0) {
    if (!g_conversation_session) {
      printf("⚠️  Voice conversation not initialized. Use /convon first.\n");
      return;
    }

    printf("\n🎤 Manually triggering conversation...\n");
    printf("========================================================\n");
    printf("Speak now! The system will listen for 5 seconds.\n");
    printf("(Vosk STT will process your speech in real-time)\n");
    printf("========================================================\n\n");

    ethervox_result_t result = ethervox_conversation_trigger(g_conversation_session);
    if (ethervox_is_error(result)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      if (ctx && ctx->message) {
        printf("❌ Failed to trigger conversation: %s\n", ctx->message);
      } else {
        printf("❌ Failed to trigger conversation: %s\n", ethervox_error_string(result));
      }
      printf("   Make sure the conversation thread is running (/convon)\n");
    } else {
      printf("[OK] Conversation triggered successfully!\n");
      printf("  Listening to microphone...\n\n");
    }
    return;
  }

  if (strncmp(line, "/speak ", 7) == 0) {
    const char* text = line + 7;  // Skip "/speak "

    if (strlen(text) == 0) {
      printf("❌ Usage: /speak <text>\n");
      printf("   Example: /speak The word rhythm is often mispronounced\n");
      return;
    }

    printf("\n🔊 Synthesizing: \"%s\"\n", text);

    // Use global TTS instance
    pthread_mutex_lock(&g_tts_mutex);
    ethervox_tts_context_t* tts = g_global_tts;
    pthread_mutex_unlock(&g_tts_mutex);

    if (!tts) {
      printf("❌ TTS not initialized. Check that Piper model is configured in /config.\n");
      return;
    }

    // Synthesize the text
    ethervox_tts_audio_t output = {0};
    ethervox_result_t result = ethervox_tts_synthesize_text(tts, text, &output);

    if (ethervox_is_success(result) && output.samples && output.sample_count > 0) {
      printf("   [OK] Synthesized %zu samples at %d Hz\n", output.sample_count, output.sample_rate);
      printf("   🎵 Playing audio...\n");

      // Save to temp file
      char temp_file[512];
      snprintf(temp_file, sizeof(temp_file), "%s/.ethervox/temp_speak.wav",
               getenv("HOME") ? getenv("HOME") : ".");

      if (ethervox_is_success(ethervox_audio_write_wav(temp_file, output.samples,
                                                       (int)output.sample_count, output.sample_rate,
                                                       output.channels))) {
#ifdef __APPLE__
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "afplay %s", temp_file);
        system(cmd);
#elif __linux__
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "aplay %s 2>/dev/null", temp_file);
        system(cmd);
#endif
        printf("   [OK] Playback complete\n");
      }

      // Cleanup audio samples
      if (output.samples) {
        free(output.samples);
      }
    } else {
      printf("   ❌ TTS synthesis failed (error: %d)\n", result);
    }

    // Note: Don't destroy TTS - it's the global instance
    printf("\n");
    return;
  }

  if (strncmp(line, "/speak-direct ", 14) == 0) {
    const char* ipa = line + 14;  // Skip "/speak-direct "

    if (strlen(ipa) == 0) {
      printf("❌ Usage: /speak-direct <ipa_phonemes>\n");
      printf("   Example: /speak-direct həlˈoʊ ðˈɛɹ\n");
      printf("   This bypasses the phonemizer and sends IPA directly to Piper\n");
      return;
    }

    printf("\n🔊 Synthesizing from IPA: \"%s\"\n", ipa);

    // Use global TTS instance
    pthread_mutex_lock(&g_tts_mutex);
    ethervox_tts_context_t* tts = g_global_tts;
    pthread_mutex_unlock(&g_tts_mutex);

    if (!tts) {
      printf("❌ TTS not initialized. Check that Piper model is configured in /config.\n");
      return;
    }

    // Synthesize from IPA directly (bypass phonemizer)
    ethervox_tts_audio_t output = {0};
    ethervox_result_t result = ethervox_tts_synthesize_ipa(tts, ipa, &output);

    if (ethervox_is_success(result) && output.samples && output.sample_count > 0) {
      printf("   [OK] Synthesized %zu samples at %d Hz\n", output.sample_count, output.sample_rate);
      printf("   🎵 Playing audio...\n");

      // Save to temp file
      char temp_file[512];
      snprintf(temp_file, sizeof(temp_file), "%s/.ethervox/temp_speak_direct.wav",
               getenv("HOME") ? getenv("HOME") : ".");

      if (ethervox_is_success(ethervox_audio_write_wav(temp_file, output.samples,
                                                       (int)output.sample_count, output.sample_rate,
                                                       output.channels))) {
        printf("   💾 Saved to: %s\n", temp_file);

        // Play using system command (macOS/Linux)
#ifdef __APPLE__
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "afplay %s", temp_file);
        system(cmd);
#elif __linux__
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "aplay %s 2>/dev/null", temp_file);
        system(cmd);
#endif
        printf("   [OK] Playback complete\n");
      }

      // Cleanup audio samples
      if (output.samples) {
        free(output.samples);
      }
    } else {
      printf("   ❌ TTS synthesis failed (error: %d)\n", result);
    }

    printf("\n");
    return;
  }

  if (strcmp(line, "/voice_training") == 0) {
    if (!g_governor) {
      printf("❌ Governor not initialized. Please wait for system startup.\n");
      return;
    }

    printf("\n🎓 Starting interactive voice training mode...\n");
    printf("   This will help improve TTS pronunciation.\n\n");

    // Get phonemizer, TTS, and STT contexts
    phonemizer_t* phonemizer = NULL;
    void* tts = NULL;  // ethervox_tts_context_t*
    void* stt = NULL;  // ethervox_stt_context_t*
    bool created_tts = false;

    // Try to get TTS from conversation session first
    if (g_conversation_session) {
      tts = ethervox_conversation_get_tts(g_conversation_session);
      stt = ethervox_conversation_get_stt(g_conversation_session);

      if (tts) {
        phonemizer = (phonemizer_t*)ethervox_conversation_get_phonemizer(g_conversation_session);
        if (phonemizer) {
          printf("  ✅ Using TTS and phonemizer from active conversation session\n");
        } else {
          printf("  ⚠️  Conversation TTS exists but phonemizer is NULL - will create standalone\n");
          // Don't use this TTS, create standalone below
          tts = NULL;
        }
      }
    }

    // If no usable TTS/phonemizer available, create a standalone one for training
    if (!tts || !phonemizer) {
      printf("  🔧 Initializing standalone TTS for training...\n");

      // Load settings to get TTS configuration
      ethervox_persistent_settings_t settings;
      ethervox_result_t load_result = ethervox_settings_load(&settings, NULL);
      printf("  [DEBUG] Settings load result: %d (%s)\n", load_result,
             ethervox_error_string(load_result));

      if (ethervox_is_success(load_result)) {
        printf("  [DEBUG] TTS engine: '%s'\n", settings.tts.engine);
        printf("  [DEBUG] Piper model path: '%s'\n", settings.tts.piper_model_path);
        printf("  [DEBUG] Path length: %zu\n", strlen(settings.tts.piper_model_path));

        if (strcmp(settings.tts.engine, "piper") == 0 &&
            strlen(settings.tts.piper_model_path) > 0) {
          ethervox_tts_config_t tts_config = ethervox_tts_default_config();
          tts_config.backend = ETHERVOX_TTS_BACKEND_PIPER;
          tts_config.model_path = settings.tts.piper_model_path;
          tts_config.speaking_rate = settings.tts.speed;
          tts_config.phoneme_variance = settings.tts.phoneme_variance;
          tts_config.prosody_variance = settings.tts.prosody_variance;
          tts_config.sample_rate = 16000;
          tts_config.channels = 1;

          tts = ethervox_tts_create(&tts_config);
          if (tts && ethervox_tts_is_ready(tts)) {
            created_tts = true;
            phonemizer = (phonemizer_t*)ethervox_tts_get_phonemizer(tts);
            printf("  ✅ Standalone TTS initialized: %s\n", settings.tts.piper_model_path);

            if (phonemizer) {
              printf("  ✅ Phonemizer available for pronunciation analysis\n");
            }
          } else {
            printf("  ⚠️  Failed to initialize TTS\n");
            if (tts) {
              ethervox_tts_destroy(tts);
              tts = NULL;
            }
          }
        } else {
          printf("  ⚠️  Piper TTS not configured in settings\n");
          printf("     Check ~/.ethervox/settings.json\n");
        }
      } else {
        printf("  ⚠️  Failed to load settings\n");
      }
    }

    printf("\n");

    // Run training session
    ethervox_result_t training_result =
        ethervox_voice_training_run(g_governor, phonemizer, tts, stt);
    if (ethervox_is_error(training_result)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      fprintf(stderr, "\n❌ Voice training failed: %s\n",
              ctx && ctx->message ? ctx->message : ethervox_error_string(training_result));
    }

    // Clean up standalone TTS if we created it
    if (created_tts && tts) {
      ethervox_tts_destroy(tts);
    }

    printf("\n[OK] Training session ended. Returning to main prompt.\n");
    return;
  }

  // ========== MODEL MANAGEMENT COMMANDS ==========

  if (strcmp(line, "/models") == 0) {
    printf("\n╭-----------------------------------------------------------------╮\n");
    printf("|                      Model Management                           |\n");
    printf("╰-----------------------------------------------------------------╯\n\n");

    // Get disk usage
    uint64_t total_usage = 0;
    ethervox_result_t disk_result = ethervox_model_get_disk_usage(&total_usage);
    if (ethervox_is_success(disk_result)) {
      printf("Total disk usage: %.2f MB\n\n", total_usage / 1024.0 / 1024.0);
    } else if (g_debug_enabled) {
      fprintf(stderr, "[WARNING] Failed to get disk usage: Error %d occurred\n", disk_result);
    }

    // Check all model types
    const char* model_types[] = {"Governor LLM", "Whisper STT", "Vosk STT", "Piper TTS"};
    ethervox_model_type_t types[] = {ETHERVOX_MODEL_TYPE_GOVERNOR, ETHERVOX_MODEL_TYPE_WHISPER,
                                     ETHERVOX_MODEL_TYPE_VOSK, ETHERVOX_MODEL_TYPE_PIPER};

    for (int i = 0; i < 4; i++) {
      printf("=== %s ===\n", model_types[i]);

      ethervox_model_info_t* models = NULL;
      uint32_t count = 0;

      if (ethervox_is_success(ethervox_model_list(types[i], &models, &count))) {
        for (uint32_t j = 0; j < count; j++) {
          const char* status_icon = "❌";
          if (models[j].status == ETHERVOX_MODEL_STATUS_FOUND) {
            status_icon = "✅";
          } else if (models[j].status == ETHERVOX_MODEL_STATUS_INCOMPLETE) {
            status_icon = "⚠️ ";
          }

          const char* default_marker = models[j].is_default ? " [DEFAULT]" : "";

          printf("%s %s%s\n", status_icon, models[j].name, default_marker);
          printf("   %s\n", models[j].description);
          printf("   Status: %s", ethervox_model_status_string(models[j].status));

          if (models[j].status == ETHERVOX_MODEL_STATUS_FOUND) {
            printf(", Size: %.2f MB\n", models[j].size_bytes / 1024.0 / 1024.0);
          } else {
            printf(", Expected: %.2f MB\n", models[j].size_bytes / 1024.0 / 1024.0);
          }

          if (j < count - 1)
            printf("\n");
        }
        free(models);
      }

      printf("\n");
    }

    printf("Commands:\n");
    printf("  /modelstatus <type>            Check specific model type\n");
    printf("  /modeldownload <type> <name>   Download a model\n");
    printf("  /modeldelete <type> <name>     Delete a model\n");
    printf("\nTypes: g_governor, whisper, vosk, piper\n\n");
    return;
  }

  if (strncmp(line, "/modelstatus ", 13) == 0) {
    const char* type_str = line + 13;

    ethervox_model_type_t type;
    if (strcmp(type_str, "governor") == 0) {
      type = ETHERVOX_MODEL_TYPE_GOVERNOR;
    } else if (strcmp(type_str, "whisper") == 0) {
      type = ETHERVOX_MODEL_TYPE_WHISPER;
    } else if (strcmp(type_str, "vosk") == 0) {
      type = ETHERVOX_MODEL_TYPE_VOSK;
    } else if (strcmp(type_str, "piper") == 0) {
      type = ETHERVOX_MODEL_TYPE_PIPER;
    } else {
      printf("❌ Unknown model type: %s\n", type_str);
      printf("Valid types: g_governor, whisper, vosk, piper\n");
      return;
    }

    printf("\n=== %s Models ===\n\n", ethervox_model_type_string(type));

    ethervox_model_info_t* models = NULL;
    uint32_t count = 0;

    if (ethervox_is_success(ethervox_model_list(type, &models, &count))) {
      for (uint32_t i = 0; i < count; i++) {
        printf("%s%s\n", models[i].name, models[i].is_default ? " [DEFAULT]" : "");
        printf("  Description: %s\n", models[i].description);
        printf("  Status:      %s\n", ethervox_model_status_string(models[i].status));

        if (models[i].status == ETHERVOX_MODEL_STATUS_FOUND) {
          printf("  Path:        %s\n", models[i].path);
          printf("  Size:        %.2f MB\n", models[i].size_bytes / 1024.0 / 1024.0);
        } else {
          printf("  URL:         %s\n", models[i].url);
          printf("  Expected:    %.2f MB\n", models[i].size_bytes / 1024.0 / 1024.0);
        }

        if (i < count - 1)
          printf("\n");
      }
      free(models);
    } else {
      printf("Failed to list models\n");
    }

    printf("\n");
    return;
  }

  if (strncmp(line, "/modeldownload ", 15) == 0) {
    const char* args = line + 15;

    // Parse: /modeldownload <type> <name>
    char type_str[32] = {0};
    const char* name_start = strchr(args, ' ');

    if (!name_start) {
      printf("Usage: /modeldownload <type> <name>\n");
      printf("Example: /modeldownload governor granite-3.0-2b-instruct-Q4_K_M.gguf\n");
      printf("         /modeldownload whisper ggml-base.en.bin\n");
      printf("         /modeldownload vosk vosk-model-small-en-us-0.15\n");
      return;
    }

    size_t type_len = name_start - args;
    if (type_len >= sizeof(type_str))
      type_len = sizeof(type_str) - 1;
    strncpy(type_str, args, type_len);

    const char* model_name = name_start + 1;

    ethervox_model_type_t type;
    if (strcmp(type_str, "governor") == 0) {
      type = ETHERVOX_MODEL_TYPE_GOVERNOR;
    } else if (strcmp(type_str, "whisper") == 0) {
      type = ETHERVOX_MODEL_TYPE_WHISPER;
    } else if (strcmp(type_str, "vosk") == 0) {
      type = ETHERVOX_MODEL_TYPE_VOSK;
    } else if (strcmp(type_str, "piper") == 0) {
      type = ETHERVOX_MODEL_TYPE_PIPER;
    } else {
      printf("❌ Unknown model type: %s\n", type_str);
      return;
    }

    // Check if already exists
    ethervox_model_status_t status = ethervox_model_check_status(type, model_name, NULL);
    if (status == ETHERVOX_MODEL_STATUS_FOUND) {
      printf("⚠️  Model already exists: %s\n", model_name);
      printf("   Use /modeldelete first if you want to re-download\n");
      return;
    }

    // Check disk space
    if (!ethervox_model_check_disk_space(type, model_name)) {
      printf("❌ Insufficient disk space for model: %s\n", model_name);
      return;
    }

    printf("📥 Downloading %s model: %s\n", ethervox_model_type_string(type), model_name);
    printf("   This may take several minutes depending on model size...\n\n");

    if (ethervox_is_success(ethervox_model_download(type, model_name, NULL, NULL))) {
      printf("\n✅ Download complete: %s\n", model_name);

      // Verify the download
      ethervox_model_info_t info;
      if (ethervox_model_check_status(type, model_name, &info) == ETHERVOX_MODEL_STATUS_FOUND) {
        printf("   Path: %s\n", info.path);
        printf("   Size: %.2f MB\n", info.size_bytes / 1024.0 / 1024.0);
      }
    } else {
      printf("\n❌ Download failed: %s\n", model_name);
    }

    return;
  }

  if (strncmp(line, "/modeldelete ", 13) == 0) {
    const char* args = line + 13;

    // Parse: /modeldelete <type> <name>
    char type_str[32] = {0};
    const char* name_start = strchr(args, ' ');

    if (!name_start) {
      printf("Usage: /modeldelete <type> <name>\n");
      printf("Example: /modeldelete governor granite-3.0-2b-instruct-Q4_K_M.gguf\n");
      return;
    }

    size_t type_len = name_start - args;
    if (type_len >= sizeof(type_str))
      type_len = sizeof(type_str) - 1;
    strncpy(type_str, args, type_len);

    const char* model_name = name_start + 1;

    ethervox_model_type_t type;
    if (strcmp(type_str, "governor") == 0) {
      type = ETHERVOX_MODEL_TYPE_GOVERNOR;
    } else if (strcmp(type_str, "whisper") == 0) {
      type = ETHERVOX_MODEL_TYPE_WHISPER;
    } else if (strcmp(type_str, "vosk") == 0) {
      type = ETHERVOX_MODEL_TYPE_VOSK;
    } else if (strcmp(type_str, "piper") == 0) {
      type = ETHERVOX_MODEL_TYPE_PIPER;
    } else {
      printf("❌ Unknown model type: %s\n", type_str);
      return;
    }

    // Check if exists
    ethervox_model_info_t info;
    ethervox_model_status_t status = ethervox_model_check_status(type, model_name, &info);
    if (status != ETHERVOX_MODEL_STATUS_FOUND) {
      printf("⚠️  Model not found: %s\n", model_name);
      return;
    }

    printf("🗑️  Deleting model: %s\n", model_name);
    printf("   This will free %.2f MB of disk space\n", info.size_bytes / 1024.0 / 1024.0);
    printf("   Are you sure? (y/N): ");
    fflush(stdout);

    char confirm[10];
    if (fgets(confirm, sizeof(confirm), stdin) && (confirm[0] == 'y' || confirm[0] == 'Y')) {
      if (ethervox_is_success(ethervox_model_delete(type, model_name))) {
        printf("✅ Model deleted: %s\n", model_name);
      } else {
        printf("❌ Failed to delete model\n");
      }
    } else {
      printf("Deletion cancelled\n");
    }

    return;
  }

  if (strcmp(line, "/test") == 0) {
    printf("\n");
    run_integration_tests(g_governor);
    printf("\n");
    return;
  }

  if (strcmp(line, "/test_voices") == 0 || strcmp(line, "/testvoices") == 0) {
    printf("\n");

    if (!g_global_tts) {
      fprintf(stderr, "❌ TTS not initialized. Check that Piper model is configured.\n");
      fprintf(stderr, "   Run /settings to configure TTS engine.\n");
      return;
    }

    printf("🎭 Voice Demonstration Mode\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    // Load settings to check configured voices
    ethervox_persistent_settings_t test_settings = {0};
    if (ethervox_is_error(ethervox_settings_load(&test_settings, NULL))) {
      fprintf(stderr, "⚠️  Could not load settings, using defaults\n");
    }

    // Test sentences for each language
    typedef struct {
      const char* lang_code;
      const char* lang_name;
      const char* text;
      const char* voice_field;
    } voice_test_t;

    voice_test_t tests[] = {
        {"en", "English", "Hello! This is the English voice. Welcome to EthervoxAI.",
         test_settings.tts.voice_en},
        {"de", "German", "Hallo! Das ist die deutsche Stimme. Willkommen bei EthervoxAI.",
         test_settings.tts.voice_de},
        {"es", "Spanish", "¡Hola! Esta es la voz en español. Bienvenido a EthervoxAI.",
         test_settings.tts.voice_es},
        {"zh", "Chinese", "你好！这是中文语音。欢迎使用EthervoxAI。", test_settings.tts.voice_zh}};

    int total_tests = sizeof(tests) / sizeof(tests[0]);
    int successful_tests = 0;

    printf("🎵 Using temp file playback\n\n");

    for (int i = 0; i < total_tests; i++) {
      printf("---------------------------------------------------------\n");
      printf("Language: %s (%s)\n", tests[i].lang_name, tests[i].lang_code);

      // Check if voice is configured
      if (!tests[i].voice_field || strlen(tests[i].voice_field) == 0) {
        printf("⚠️  No voice configured in settings (skipping)\n\n");
        continue;
      }

      printf("Voice:    %s\n", tests[i].voice_field);
      printf("Text:     \"%s\"\n\n", tests[i].text);

      // Detect language and switch voice
      const char* detected_lang =
          ethervox_detect_and_switch_voice(tests[i].text, NULL, (void**)&g_global_tts);

      if (strcmp(detected_lang, tests[i].lang_code) != 0) {
        printf("⚠️  Expected %s but detected %s\n", tests[i].lang_code, detected_lang);
      }

      // Synthesize
      ethervox_tts_audio_t output = {0};
      ethervox_result_t result = ethervox_tts_synthesize_text(g_global_tts, tests[i].text, &output);

      if (ethervox_is_success(result) && output.samples && output.sample_count > 0) {
        printf("[OK] Synthesized %zu samples at %d Hz\n", output.sample_count, output.sample_rate);

        // Play audio via temp file
        printf("🔊 Playing...\n");
        char temp_file[512];
        snprintf(temp_file, sizeof(temp_file), "%s/.ethervox/temp_voice_test_%s.wav",
                 getenv("HOME") ? getenv("HOME") : ".", tests[i].lang_code);

        if (ethervox_is_success(ethervox_audio_write_wav(temp_file, output.samples,
                                                         (int)output.sample_count,
                                                         output.sample_rate, output.channels))) {
#ifdef __APPLE__
          char cmd[600];
          snprintf(cmd, sizeof(cmd), "afplay %s", temp_file);
          system(cmd);
#elif __linux__
          char cmd[600];
          snprintf(cmd, sizeof(cmd), "aplay %s 2>/dev/null", temp_file);
          system(cmd);
#endif
          printf("[OK] Playback complete\n\n");
          successful_tests++;
        } else {
          fprintf(stderr, "❌ Failed to save/play audio\n\n");
        }

        free(output.samples);
      } else {
        fprintf(stderr, "❌ Synthesis failed (error: %d)\n\n", result);
      }
    }

    // Summary
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Voice Test Summary: %d/%d successful\n\n", successful_tests, total_tests);

    return;
  }

  if (strncmp(line, "/testllm", 8) == 0) {
    printf("\n");
    // Check for verbose flag and test name
    bool verbose = (strstr(line, "-v") != NULL || strstr(line, "--verbose") != NULL);

    // Parse test name (everything after /testllm, excluding flags)
    const char* test_name = NULL;
    char* args = (char*)(line + 8);
    while (*args && isspace(*args))
      args++;  // Skip whitespace

    if (*args && strncmp(args, "-v", 2) != 0 && strncmp(args, "--verbose", 9) != 0) {
      // Extract test name (up to flag or end)
      static char test_buf[128];
      int i = 0;
      while (args[i] && !isspace(args[i]) && i < 127) {
        test_buf[i] = args[i];
        i++;
      }
      test_buf[i] = '\0';
      if (i > 0)
        test_name = test_buf;
    }

    run_llm_tool_tests(g_governor, memory, g_loaded_model_path, verbose, test_name);
    printf("\n");
    return;
  }

  if (strcmp(line, "/optimize_tool_prompts") == 0) {
    printf("\n");
    if (strlen(g_loaded_model_path) == 0) {
      printf("[FAIL] No model loaded. Use /load <path> first.\n");
      return;
    }

    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" Tool Prompt Optimization - Enhanced Version (JSON Output)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Model: %s\n", g_loaded_model_path);
    printf("\n");
    printf("This will:\n");
    printf("1. Export current tool registry to binary manifest\n");
    printf("2. Ask the LLM to optimize each tool description (~15 words)\n");
    printf("3. Save optimized prompts to JSON cache\n");
    printf("4. Reduce system prompt from ~15K to ~150 tokens (99%% reduction!)\n");
    printf("\n");

    // Step 1: Initialize manifest system (exports manifest + loads it)
    // Only initialize if not already loaded (avoid wiping existing file pointer)
    printf("Step 1: Initializing Tool Manifest System...\n");
    if (!g_manifest_registry.manifest_file) {
      ethervox_result_t init_result = ethervox_governor_init_with_manifest(
          g_governor, g_loaded_model_path, &g_manifest_registry);
      if (ethervox_is_error(init_result)) {
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        printf("[FAIL] Failed to initialize manifest system: %s\n",
               ctx && ctx->message ? ctx->message : ethervox_error_string(init_result));
        printf("  Cannot run optimization without manifest\n");
        printf("\n");
        return;
      }

      // Check if manifest was actually loaded
      if (!g_manifest_registry.manifest_file) {
        printf("[FAIL] Failed to load manifest file\n");
        printf("  Cannot run optimization without manifest\n");
        printf("\n");
        return;
      }
    } else {
      printf("[OK] Using existing manifest: %u tools\n", g_manifest_registry.header.tool_count);
    }

    // Set manifest for get_tool_info meta-tool
    ethervox_get_tool_info_set_manifest(&g_manifest_registry);

    // Step 2: Run optimizer v2 (JSON output, batch processing)
    printf("\nStep 2: Running optimization (this may take 30-60 seconds)...\n");
    ethervox_result_t ret =
        ethervox_optimize_tool_prompts(g_governor, g_loaded_model_path, &g_manifest_registry,
                                       true);  // optimize_new_only = true (incremental)

    if (ethervox_is_success(ret)) {
      printf("\n[OK] Optimization complete!\n");
      printf("\nNext steps:\n");
      printf("1. Restart EthervoxAI to use optimized prompts\n");
      printf("2. Your system prompt will now use ~150 tokens instead of ~15K\n");
      printf("3. Tool schemas will be injected on-demand only when called\n");
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("\n[FAIL] Optimization failed: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(ret));
      printf("  The assistant will still work with fallback level 1 (binary one-liners)\n");
    }
    printf("\n");
    return;
  }

  if (strncmp(line, "/load ", 6) == 0) {
    const char* model_path = line + 6;
    printf("Loading model: %s\n", model_path);

    ethervox_result_t ret = ethervox_governor_load_model(g_governor, model_path, NULL, NULL);
    if (ethervox_is_success(ret)) {
      snprintf(g_loaded_model_path, sizeof(g_loaded_model_path), "%s", model_path);

      // Initialize manifest system after model load
      ethervox_result_t result = ethervox_governor_init_with_manifest(
          g_governor, g_loaded_model_path, &g_manifest_registry);
      if (ethervox_is_success(result)) {
        // Set manifest for get_tool_info meta-tool
        ethervox_get_tool_info_set_manifest(&g_manifest_registry);
      } else {
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        fprintf(stderr, "[WARNING] Failed to init manifest: %s\n",
                ctx && ctx->message ? ctx->message : ethervox_error_string(result));
      }

      printf("[OK] Model loaded successfully\n");
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("[FAIL] Failed to load model: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(ret));
    }
    return;
  }

  if (strncmp(line, "/search ", 8) == 0) {
    handle_search(memory, line + 8);
    return;
  }

  if (strncmp(line, "/summary", 8) == 0) {
    int window = 10;
    if (strlen(line) > 9) {
      window = atoi(line + 9);
    }
    handle_summary(memory, window);
    return;
  }

  if (strncmp(line, "/export ", 8) == 0) {
    handle_export(memory, line + 8);
    return;
  }

  if (strcmp(line, "/clear") == 0) {
    uint32_t pruned = 0;
    ethervox_result_t result =
        ethervox_memory_forget(memory, 0, 1.1f, &pruned);  // Remove all (importance > 1.0)
    if (ethervox_is_success(result)) {
      printf("Cleared %u memories\n", pruned);
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      fprintf(stderr, "Failed to clear memories: %s\n",
              ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    }
    return;
  }

  if (strcmp(line, "/reset") == 0) {
    printf("To reset conversation history, restart the application.\n");
    printf("The KV cache maintains conversation context across queries.\n");
    return;
  }

  if (strcmp(line, "/summarizeCache") == 0) {
    if (!g_governor) {
      printf("❌ Governor not initialized\n");
      return;
    }

    printf("\n📝 Generating conversation summary and clearing KV cache...\n");

    ethervox_result_t result = ethervox_governor_summarize_and_clear_cache(g_governor, true);

    if (ethervox_is_success(result)) {
      printf("[OK] Cache summarized and cleared successfully\n");
      printf("  Summary has been stored in memory with tags: [context_summary, manual_clear]\n");
      printf("  Use /search to find it later\n\n");
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("[FAIL] Failed to summarize cache: %s\n\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    }
    return;
  }

  // Debug: If we got here and line starts with /, something's wrong
  if (line[0] == '/') {
    fprintf(stderr, "[WARN] Unrecognized slash command: '%s' (len=%zu, bytes:", line, strlen(line));
    for (size_t i = 0; i < strlen(line) && i < 20; i++) {
      fprintf(stderr, " %02X", (unsigned char)line[i]);
    }
    fprintf(stderr, ")\n");
  }

  if (strncmp(line, "/startup ", 9) == 0) {
    const char* action = line + 9;

    // Default startup prompt file location
    const char* home_dir = getenv("HOME");
    char prompt_file[512];
    if (home_dir) {
      snprintf(prompt_file, sizeof(prompt_file), "%s/.ethervox/startup_prompt.txt", home_dir);
    } else {
      snprintf(prompt_file, sizeof(prompt_file), "./.ethervox_startup_prompt.txt");
    }

    if (strncmp(action, "edit", 4) == 0) {
      // Get or create editor command
      const char* editor = getenv("EDITOR");
      if (!editor)
        editor = "nano";  // Default to nano

      char cmd[768];
      snprintf(cmd, sizeof(cmd), "%s %s", editor, prompt_file);

      printf("Opening startup prompt in %s...\n", editor);
      printf("Default prompt: %s\n\n", DEFAULT_STARTUP_PROMPT);

      int ret = system(cmd);
      if (ret == 0) {
        printf("Startup prompt saved to: %s\n", prompt_file);
      } else {
        printf("Failed to open editor\n");
      }
    } else if (strncmp(action, "show", 4) == 0) {
      FILE* fp = fopen(prompt_file, "r");
      if (fp) {
        printf("Current startup prompt:\n");
        printf("=============================================\n");
        char buf[2048];
        while (fgets(buf, sizeof(buf), fp)) {
          printf("%s", buf);
        }
        printf("\n=============================================\n");
        fclose(fp);
      } else {
        printf("No custom startup prompt found.\n");
        printf("Default: %s\n", DEFAULT_STARTUP_PROMPT);
      }
    } else if (strncmp(action, "reset", 5) == 0) {
      if (remove(prompt_file) == 0) {
        printf("Custom startup prompt removed. Will use default.\n");
      } else {
        printf("No custom startup prompt to remove.\n");
      }
    } else {
      printf("Usage: /startup <edit|show|reset>\n");
      printf("  edit  - Edit custom startup prompt (uses $EDITOR or nano)\n");
      printf("  show  - Display current startup prompt\n");
      printf("  reset - Remove custom prompt and use default\n");
    }
    return;
  }

  if (strcmp(line, "/paste") == 0) {
    printf("Entering paste mode...\n");
    char* pasted = read_paste_input();
    if (!pasted || pasted[0] == '\0') {
      printf("Paste mode cancelled (empty input).\n");
      if (pasted)
        free(pasted);
      return;
    }
    // Recursively process the pasted content as a command
    process_command(pasted, memory, g_governor, path_config, file_config, voice_session, quit_flag);
    free(pasted);
    return;
  }

  // Store user message
  store_message(memory, line, true, 0.8f);

  // Suppress llama.cpp/ggml stderr logs in quiet mode (but not when debug is enabled)
  int stderr_backup = -1;
  if (g_quiet_mode && !g_debug_enabled) {
    stderr_backup = dup(STDERR_FILENO);
    int dev_null = open_devnull();
    if (dev_null != -1) {
      dup2(dev_null, STDERR_FILENO);
      close(dev_null);
    }
  }

  // Send to Governor for processing
  char* response = NULL;
  char* error = NULL;
  ethervox_confidence_metrics_t metrics;

  ethervox_governor_status_t status = ethervox_governor_execute(
      g_governor, line, &response, &error, &metrics,
      NULL,                                                // progress_callback
      g_streaming_enabled ? stream_token_callback : NULL,  // token_callback
      NULL                                                 // user_data
  );

  // Restore stderr if we redirected it
  if (g_quiet_mode && !g_debug_enabled && stderr_backup != -1) {
    fflush(stderr);  // Flush any pending stderr output before restoring
    dup2(stderr_backup, STDERR_FILENO);
    close(stderr_backup);
  }

  if (status == ETHERVOX_GOVERNOR_SUCCESS && response) {
    // If streaming was enabled, response was already printed token-by-token
    // Just print newlines for spacing
    if (g_streaming_enabled) {
      printf("\n\n");
    } else {
      // Non-streaming mode: print full response at once
      printf("\033[36m");  // Cyan color for assistant response
      fflush(stdout);

      // Apply markdown formatting if enabled
      if (g_markdown_enabled) {
        char* formatted = markdown_to_ansi(response);
        if (formatted) {
          printf("\033[36m%s\033[0m\n\n", formatted);
          free(formatted);
        } else {
          printf("%s\033[0m\n\n", response);
        }
      } else {
        printf("%s\033[0m\n\n", response);
      }
    }

    // Store assistant response
    store_message(memory, response, false, 0.8f);

    free(response);
  } else if (status == ETHERVOX_GOVERNOR_ERROR) {
    printf("\033[0m");  // Reset color before error
    if (error) {
      printf("[Error: %s]\n\n", error);
      free(error);
    } else {
      printf("[Error: Governor execution failed]\n\n");
    }
  } else {
    // TIMEOUT, NEED_CLARIFICATION, USER_DENIED, etc.
    printf("\033[0m");  // Reset color before status message

    // Print partial response if available (common with TIMEOUT)
    if (response && response[0] != '\0') {
      if (g_markdown_enabled) {
        char* formatted = markdown_to_ansi(response);
        if (formatted) {
          printf("\033[36m%s\033[0m\n\n", formatted);
          free(formatted);
        } else {
          printf("\033[36m%s\033[0m\n\n", response);
        }
      } else {
        printf("\033[36m%s\033[0m\n\n", response);
      }
      store_message(memory, response, false, 0.7f);  // Lower confidence for partial
    }

    // Show status message
    const char* status_msg = "Unknown status";
    switch (status) {
      case ETHERVOX_GOVERNOR_NEED_CLARIFICATION:
        status_msg = "Need clarification";
        break;
      case ETHERVOX_GOVERNOR_TIMEOUT:
        status_msg = "Response truncated (iteration/context limit)";
        break;
      case ETHERVOX_GOVERNOR_USER_DENIED:
        status_msg = "Tool execution denied by user";
        break;
      default:
        break;
    }
    printf("[Status: %s]\n\n", status_msg);

    if (response)
      free(response);
    if (error)
      free(error);
  }
}

int main(int argc, char** argv) {
  // Setup signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Ensure ~/.ethervox directory structure exists
  ensure_ethervox_directories();

  // Set default log level to OFF (quiet mode by default)
  g_ethervox_debug_enabled = 0;
  ethervox_log_set_level(ETHERVOX_LOG_LEVEL_OFF);

  // Parse command-line arguments (do this before banner for -settings flag)
  const char* model_path = NULL;
  const char* memory_dir = NULL;
  const char* startup_prompt_file = NULL;
  bool interactive = true;

  bool run_tests = false;
  bool run_llm_tests = false;     // LLM tests mode: engineering + /testllm
  const char* test_name = NULL;   // Optional test name filter for -testllm
  bool run_optimization = false;  // Tool optimization mode: engineering + /optimize_tool_prompts
  bool quiet_mode = true;         // Default to quiet mode
  bool no_persist = false;
  bool skip_startup_prompt = false;
  bool engineering_mode = false;          // Engineering mode: no help, no startup, debug on
  bool minimal_mode = false;              // Minimal mode: fast loading, tools disabled
  bool auto_start_conversation = false;   // Auto-start voice conversation with -convon
  bool settings_mode = false;             // Settings mode: open settings menu before loading model
  bool tts_speak_mode = false;            // TTS-only mode: synthesize text and exit
  bool tts_speak_direct_mode = false;     // TTS-only mode: synthesize IPA and exit
  bool tts_test_voices_mode = false;      // Test all configured TTS voices
  const char* tts_text = NULL;            // Text to synthesize in TTS mode
  const char* audio_file = NULL;          // Optional output file for -af flag
  const char* tts_voice_override = NULL;  // Language code override for -voice flag

  // Track explicit flag usage for conflict detection
  bool debug_flag_set = false;
  bool quiet_flag_set = false;

  // Dafualt to auto-load model unless --noautoload specified
  bool auto_load_model = true;
  if (!model_path) {
    // Default model filename - will be searched in ~/.ethervox/models/governor/ first
    model_path = "governor/granite-4.0-h-tiny-Q4_K_M.gguf";
  }

  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "--model") == 0 || strcmp(argv[i], "-model") == 0) && i + 1 < argc) {
      model_path = argv[++i];
    } else if ((strcmp(argv[i], "--memory") == 0 || strcmp(argv[i], "-memory") == 0) &&
               i + 1 < argc) {
      memory_dir = argv[++i];
    } else if (strcmp(argv[i], "--no-persist") == 0 || strcmp(argv[i], "-no-persist") == 0) {
      no_persist = true;
      memory_dir = NULL;  // Memory-only mode
    } else if (strcmp(argv[i], "--noautoload") == 0 || strcmp(argv[i], "-noautoload") == 0) {
      auto_load_model = false;
      if (model_path) {
        model_path = NULL;  // Default model path
      }
    } else if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-debug") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      quiet_mode = false;
      g_quiet_mode = false;
      g_debug_enabled = true;
      debug_flag_set = true;
    } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-quiet") == 0 ||
               strcmp(argv[i], "-q") == 0) {
      quiet_mode = true;
      g_quiet_mode = true;  // Set global flag
      g_debug_enabled = false;
      quiet_flag_set = true;
    } else if (strcmp(argv[i], "-engineering") == 0 || strcmp(argv[i], "--engineering") == 0) {
      engineering_mode = true;
      g_engineering_mode = true;
      g_quiet_mode = false;
      g_debug_enabled = true;
      quiet_mode = false;
      skip_startup_prompt = true;
    } else if ((strcmp(argv[i], "--startup-prompt") == 0 ||
                strcmp(argv[i], "-startup-prompt") == 0) &&
               i + 1 < argc) {
      startup_prompt_file = argv[++i];
    } else if (strcmp(argv[i], "--no-startup-prompt") == 0 ||
               strcmp(argv[i], "-no-startup-prompt") == 0) {
      skip_startup_prompt = true;
    } else if (strcmp(argv[i], "--minimal") == 0 || strcmp(argv[i], "-minimal") == 0) {
      minimal_mode = true;
      printf("⚡ Minimal mode enabled - fast loading, tools disabled\n");
    } else if (strcmp(argv[i], "-convon") == 0 || strcmp(argv[i], "--convon") == 0) {
      auto_start_conversation = true;
      printf("🎤 -convon flag parsed: will auto-start conversation after initialization\n");
    } else if (strcmp(argv[i], "--test") == 0 || strcmp(argv[i], "-test") == 0) {
      run_tests = true;
    } else if (strcmp(argv[i], "--testllm") == 0 || strcmp(argv[i], "-testllm") == 0) {
      run_llm_tests = true;
      engineering_mode = true;
      g_quiet_mode = false;
      g_debug_enabled = true;
      quiet_mode = false;
      skip_startup_prompt = true;
      interactive = false;
      // Check if next arg is test name (not a flag)
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        test_name = argv[++i];
      }
    } else if (strcmp(argv[i], "--optimize_tool_prompts") == 0 ||
               strcmp(argv[i], "-optimize_tool_prompts") == 0) {
      run_optimization = true;
      engineering_mode = true;
      g_quiet_mode = true;  // Suppress logs during optimization
      g_debug_enabled = false;
      quiet_mode = true;
      skip_startup_prompt = true;
      interactive = false;
    } else if (strcmp(argv[i], "--settings") == 0 || strcmp(argv[i], "-settings") == 0) {
      settings_mode = true;
      skip_startup_prompt = true;
    } else if ((strcmp(argv[i], "--speak") == 0 || strcmp(argv[i], "-speak") == 0) &&
               i + 1 < argc) {
      tts_speak_mode = true;
      tts_text = argv[++i];
      interactive = false;
      skip_startup_prompt = true;
    } else if ((strcmp(argv[i], "--speak-direct") == 0 || strcmp(argv[i], "-speak-direct") == 0) &&
               i + 1 < argc) {
      tts_speak_direct_mode = true;
      tts_text = argv[++i];
      interactive = false;
      skip_startup_prompt = true;
    } else if (strcmp(argv[i], "--test-voices") == 0 || strcmp(argv[i], "-test-voices") == 0) {
      tts_test_voices_mode = true;
      interactive = false;
      skip_startup_prompt = true;
    } else if ((strcmp(argv[i], "--af") == 0 || strcmp(argv[i], "-af") == 0) && i + 1 < argc) {
      audio_file = argv[++i];
    } else if ((strcmp(argv[i], "--voice") == 0 || strcmp(argv[i], "-voice") == 0) &&
               i + 1 < argc) {
      tts_voice_override = argv[++i];
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("Usage: %s [options]\n\n", argv[0]);
      printf("Options:\n");
      printf("  -model <path>      Path to Governor GGUF model\n");
      printf("  -noautoload        Suppress auto-loading default Governor model on startup\n");
      printf("  -memory <dir>      Directory for memory persistence\n");
      printf("  -no-persist        Run in memory-only mode (no files)\n");
      printf("  -startup-prompt <file>  Custom startup prompt file\n");
      printf("  -no-startup-prompt      Skip automatic startup prompt\n");
      printf("  -minimal           Fast loading mode (~50 tokens, tools disabled, 90%% faster)\n");
      printf("  -convon            Auto-start voice conversation mode on startup\n");
      printf("  -settings          Open settings menu before loading model\n");
      printf("  -debug, -d         Enable debug logging (default: off)\n");
      printf("  -quiet, -q         Disable debug logging (explicit quiet)\n");
      printf("  -engineering       Engineering mode: suppress help/startup, enable debug\n");
      printf("  -test              Run component tests before starting\n");
      printf("  -testllm [name]    Run LLM tool tests, optionally filter by test name\n");
      printf(
          "  -optimize_tool_prompts  Optimize tool prompts (engineering mode + "
          "/optimize_tool_prompts)\n");
      printf("  -speak <text>      TTS-only mode: synthesize text with streaming playback\n");
      printf(
          "  -speak-direct <ipa>  TTS-only mode: synthesize IPA phonemes directly (no "
          "phonemizer)\n");
      printf("  -voice <lang>      Override language detection (en, de, es, zh) for TTS\n");
      printf("  -test-voices       Demonstrate all configured TTS voices\n");
      printf("  -af <file>         Save audio output to file (use with -speak/-speak-direct)\n");
      printf("  -help, -h          Show this help message\n");
      printf("\n");
      _exit(0);
    }
  }

  // Validate flag combinations
  if (minimal_mode && run_llm_tests) {
    fprintf(stderr, "Error: --minimal and --testllm are incompatible\n");
    fprintf(stderr, "  --minimal disables tools, but --testllm requires tools to test\n");
    fprintf(stderr, "  Please use one or the other.\n");
    return 1;
  }

  if (minimal_mode && run_optimization) {
    fprintf(stderr, "Error: --minimal and --optimize_tool_prompts are incompatible\n");
    fprintf(stderr, "  --minimal disables tools, but --optimize_tool_prompts requires tools\n");
    fprintf(stderr, "  Please use one or the other.\n");
    return 1;
  }

  if (minimal_mode && engineering_mode) {
    printf("⚠️  Warning: Combining --minimal (fast mode) with -engineering (debug mode)\n");
    printf("   Minimal mode: tools disabled, brief system prompt\n");
    printf("   Engineering mode: verbose logging, startup prompt skipped\n");
    printf("   Both will be applied - you'll get fast loading with debug output\n\n");
  }

  if (debug_flag_set && quiet_flag_set) {
    printf("⚠️  Warning: --debug and --quiet are contradictory\n");
    printf("   Last flag wins: using %s mode\n\n", quiet_mode ? "quiet" : "debug");
  }

  // Set initial log level based on quiet mode
  if (!quiet_mode) {
    if (g_debug_enabled) {
      ethervox_log_set_level(ETHERVOX_LOG_LEVEL_DEBUG);
      g_ethervox_debug_enabled = 1;
    } else {
      ethervox_log_set_level(ETHERVOX_LOG_LEVEL_INFO);
      g_ethervox_debug_enabled = 0;
    }
  } else {
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_OFF);
    g_ethervox_debug_enabled = 0;
  }

  // Print banner (skip if settings mode - ncurses needs clean terminal)
  if (!settings_mode) {
    print_banner();
  }

  // Default memory directory - use ~/.ethervox/memory
  if (memory_dir == NULL && interactive && !no_persist) {
    const char* home = getenv("HOME");
    static char ethervox_memory[512];
    if (home) {
      snprintf(ethervox_memory, sizeof(ethervox_memory), "%s/.ethervox/memory", home);
      memory_dir = ethervox_memory;
    } else {
      memory_dir = "./.ethervox/memory";
    }
  }

  // Run component tests if requested
  if (run_tests) {
    run_component_tests(memory_dir ? memory_dir : "/tmp");
    if (!interactive) {
      _exit(0);
    }
  }

  // Initialize platform
  ethervox_platform_t platform = {0};
  ethervox_result_t result = ethervox_platform_register_hal(&platform);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "❌ Platform initialization failed: %s\n", ethervox_error_string(result));
    if (ctx && ctx->message) {
      fprintf(stderr, "   %s\n", ctx->message);
      if (g_debug_enabled) {
        fprintf(stderr, "   at %s:%d in %s()\n", ctx->file, ctx->line, ctx->function);
      }
    }
    return 1;
  }

  // Initialize memory store
  ethervox_memory_store_t memory;
  result = ethervox_memory_init(&memory, NULL, memory_dir);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "❌ Failed to initialize memory store: %s\n", ethervox_error_string(result));
    if (ctx && ctx->message) {
      fprintf(stderr, "   %s\n", ctx->message);
      if (g_debug_enabled && ctx->file) {
        fprintf(stderr, "   at %s:%d\n", ctx->file, ctx->line);
      }
    }
    if (memory_dir) {
      fprintf(stderr, "   Memory directory: %s\n", memory_dir);
      fprintf(stderr, "   Hint: Check directory exists and has write permissions\n");
    }
    return 1;
  }

  // Initialize path configuration
  ethervox_path_config_t path_config;
  result = ethervox_path_config_init(&path_config, &memory);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "❌ Failed to initialize path configuration: %s\n",
            ethervox_error_string(result));
    if (ctx && ctx->message) {
      fprintf(stderr, "   %s\n", ctx->message);
    }
    ethervox_memory_cleanup(&memory);
    return 1;
  }

  // Check for unverified paths and prompt user
  ethervox_user_path_t* unverified = NULL;
  uint32_t unverified_count = 0;
  if (ethervox_is_success(
          ethervox_path_config_get_unverified(&path_config, &unverified, &unverified_count))) {
    if (unverified_count > 0) {
      printf("\n⚠️  Some default paths don't exist on your system:\n");
      for (uint32_t i = 0; i < unverified_count; i++) {
        printf("  • %s: %s\n", unverified[i].label, unverified[i].path);
      }
      printf("Use /setpath to configure these if needed.\n");
      printf("Example: /setpath Notes /path/to/your/notes\n\n");
    }
    free(unverified);
  }

  if (memory_dir) {
    // Use platform-agnostic function to load previous session
    // This handles all the complexity of finding the most recent file,
    // preserving tags, IDs, and adding the "imported" tag
    uint32_t turns_loaded = 0;
    ethervox_result_t load_result = ethervox_memory_load_previous_session(&memory, &turns_loaded);

    if (ethervox_is_error(load_result)) {
      fprintf(stderr, "[WARNING] Memory load previous session failed: Error %d occurred\n",
              load_result);
    }

    if (g_debug_enabled) {
      printf("Memory: Persistent storage at %s\n", memory.storage_filepath);
      printf("Memory: Current entry count before load: %u\n", memory.entry_count);
      printf("Memory: Load result = %s, turns_loaded = %u\n",
             ethervox_is_success(load_result) ? "SUCCESS" : ethervox_error_string(load_result),
             turns_loaded);
      printf("Memory: Current entry count after load: %u\n", memory.entry_count);

      if (ethervox_is_success(load_result) && turns_loaded > 0) {
        printf("Memory: Successfully loaded %u previous memories from last session\n",
               turns_loaded);

        // Show a sample of what was loaded (first 10 entries)
        uint32_t show_count = memory.entry_count < 10 ? memory.entry_count : 10;
        for (uint32_t i = 0; i < show_count; i++) {
          printf("  Entry %u (id=%llu): [", i, (unsigned long long)memory.entries[i].memory_id);
          for (uint32_t t = 0; t < memory.entries[i].tag_count; t++) {
            printf("%s%s", t > 0 ? "," : "", memory.entries[i].tags[t]);
          }
          // Replace newlines with spaces for display
          printf("] \"");
          for (const char* p = memory.entries[i].text; *p && (p - memory.entries[i].text) < 100;
               p++) {
            if (*p == '\n')
              printf("\\n");
            else if (*p == '\r')
              printf("\\r");
            else if (*p == '\t')
              printf("\\t");
            else
              putchar(*p);
          }
          if (strlen(memory.entries[i].text) > 100)
            printf("...");
          printf("\"\n");
        }
        if (memory.entry_count > 10) {
          printf("  ... and %u more entries\n", memory.entry_count - 10);
        }
      } else if (ethervox_is_error(load_result)) {
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        printf("Memory: Failed to load previous session: %s\n",
               ctx && ctx->message ? ctx->message : ethervox_error_string(load_result));
      } else {
        printf("Memory: No previous session to load\n");
      }
    }
  } else if (g_debug_enabled) {
    printf("Memory: In-memory only (no persistence)\n");
  }

  // Load persistent settings
  if (ethervox_is_error(ethervox_settings_load(&g_settings, NULL))) {
    // Failed to load - use defaults
    g_settings = ethervox_settings_get_defaults();
    if (g_debug_enabled) {
      printf("Using default settings (no saved settings file found)\n");
    }
  } else if (g_debug_enabled) {
    printf("[OK] Loaded settings from %s\n", ethervox_settings_get_default_path());
  }

  // If -settings flag was specified, open settings menu now (before model loading)
  if (settings_mode) {
    if (!ethervox_settings_menu_available()) {
      fprintf(stderr, "Error: Settings menu not available on this platform (requires ncurses)\n");
      ethervox_memory_cleanup(&memory);
      return 1;
    }

    // Prepare settings structure using helper function
    ethervox_settings_t menu_settings;
    init_menu_settings(&menu_settings, model_path, memory_dir);

    // Open settings menu (pass model_path for display, but no callback since model not loaded yet)
    // Settings will be applied automatically after menu exits via g_settings reload
    ethervox_result_t menu_result = ethervox_settings_menu_show(&menu_settings, model_path, NULL,
                                                                NULL, tts_reload_callback, NULL);

    // Now ncurses has cleaned up, we can print normally
    if (ethervox_is_error(menu_result) && g_debug_enabled) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("\nSettings menu exited with error: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(menu_result));
    }

    // Reload settings from disk in case they were changed
    if (ethervox_is_error(ethervox_settings_load(&g_settings, NULL))) {
      g_settings = ethervox_settings_get_defaults();
    }

    // Prompt user: continue or exit?
    printf("\n");
    printf("Settings configuration complete.\n\n");
    printf("Start the application with these settings? (yes/no): ");
    fflush(stdout);

    char choice[64];
    if (fgets(choice, sizeof(choice), stdin) != NULL) {
      // Trim newline
      size_t len = strlen(choice);
      if (len > 0 && choice[len - 1] == '\n') {
        choice[len - 1] = '\0';
      }

      // Check for negative responses
      if (strcasecmp(choice, "no") == 0 || strcasecmp(choice, "n") == 0 ||
          strcasecmp(choice, "exit") == 0 || strcasecmp(choice, "quit") == 0 ||
          strcasecmp(choice, "q") == 0) {
        printf("\nExiting. Run again to start with your configured settings.\n");
        ethervox_memory_cleanup(&memory);
        _exit(0);
      }
    }

    printf("\nStarting application...\n\n");
  }

  // Initialize global TTS system at startup
  // This allows the speak tool to work immediately without requiring /convon
  // For -speak mode, delay TTS init until after language detection
  if (strcmp(g_settings.tts.engine, "piper") == 0 && strlen(g_settings.tts.piper_model_path) > 0 &&
      !tts_speak_mode && !tts_speak_direct_mode) {
    pthread_mutex_lock(&g_tts_mutex);

    ethervox_tts_config_t tts_config = ethervox_tts_default_config();
    tts_config.backend = ETHERVOX_TTS_BACKEND_PIPER;
    tts_config.model_path = g_settings.tts.piper_model_path;
    tts_config.speaking_rate = g_settings.tts.speed;
    tts_config.phoneme_variance = g_settings.tts.phoneme_variance;
    tts_config.prosody_variance = g_settings.tts.prosody_variance;
    tts_config.chunk_callback = NULL;
    tts_config.callback_user_data = NULL;

    g_global_tts = ethervox_tts_create(&tts_config);

    pthread_mutex_unlock(&g_tts_mutex);

    if (g_global_tts) {
      if (g_debug_enabled) {
        printf("Global TTS: Initialized with model: %s\n", g_settings.tts.piper_model_path);
      }
    } else {
      fprintf(stderr, "Warning: Failed to initialize global TTS\n");
    }
  } else if (g_debug_enabled && !tts_speak_mode && !tts_speak_direct_mode) {
    printf("Global TTS: Skipped (no Piper model configured)\n");
  }

  // TTS-only modes: synthesize and exit without loading governor
  if (tts_speak_mode || tts_speak_direct_mode) {
    printf("\n🔊 TTS-only mode: %s\n", tts_speak_direct_mode ? "Direct IPA" : "Text");
    printf("   Input: \"%s\"\n", tts_text);

    // Detect language and switch voice if needed (skip for IPA direct mode)
    const char* detected_lang = "en";
    if (!tts_speak_direct_mode) {
      // Use -voice flag if provided, otherwise detect from text
      if (tts_voice_override) {
        detected_lang = tts_voice_override;
        printf("   Language: %s (forced by -voice flag)\n", detected_lang);
      } else {
        // Detect language but don't reload TTS yet (it's not created)
        detected_lang = ethervox_detect_language(tts_text);
        printf("   Language: %s (auto-detected)\n", detected_lang);
      }

      // Load settings and update model path for detected language
      ethervox_persistent_settings_t temp_settings;
      if (ethervox_is_success(ethervox_settings_load(&temp_settings, NULL))) {
        const char* target_voice = ethervox_get_voice_for_language(detected_lang, &temp_settings);
        if (target_voice) {
          const char* home = getenv("HOME");
          if (home) {
            snprintf(temp_settings.tts.piper_model_path, sizeof(temp_settings.tts.piper_model_path),
                     "%s/.ethervox/models/piper/%s.onnx", home, target_voice);
          }
          // Update global settings for TTS creation
          g_settings = temp_settings;
        }
      }
    }

    // Now create TTS with correct voice and streaming callback if needed
    pthread_mutex_lock(&g_tts_mutex);

    ethervox_tts_config_t tts_config = ethervox_tts_default_config();
    tts_config.backend = ETHERVOX_TTS_BACKEND_PIPER;
    tts_config.model_path = g_settings.tts.piper_model_path;
    tts_config.speaking_rate = g_settings.tts.speed;
    tts_config.phoneme_variance = g_settings.tts.phoneme_variance;
    tts_config.prosody_variance = g_settings.tts.prosody_variance;

    // For streaming mode (no -af file), set up callback
    if (!audio_file) {
      g_stream_player = audio_stream_player_create(16000, 1);
      if (g_stream_player) {
        tts_config.chunk_callback = tts_stream_callback;
        tts_config.callback_user_data = g_stream_player;
      }
    } else {
      tts_config.chunk_callback = NULL;
      tts_config.callback_user_data = NULL;
    }

    g_global_tts = ethervox_tts_create(&tts_config);

    pthread_mutex_unlock(&g_tts_mutex);

    if (!g_global_tts) {
      fprintf(stderr, "❌ TTS not initialized. Check that Piper model is configured.\n");
      fprintf(stderr, "   Run with -settings to configure TTS engine.\n");
      ethervox_memory_cleanup(&memory);
      return 1;
    }

    if (audio_file) {
      printf("   Output: %s (batch mode)\n", audio_file);
    } else {
      printf("   Output: streaming playback (use -af <file> to save)\n");
      // Start streaming player before synthesis
      if (g_stream_player) {
        ethervox_result_t result = audio_stream_player_start(g_stream_player);
        if (ethervox_is_error(result)) {
          const ethervox_error_context_t* ctx = ethervox_error_get_context();
          fprintf(stderr, "   ⚠️  Failed to start audio stream: %s\n",
                  ctx && ctx->message ? ctx->message : ethervox_error_string(result));
          fprintf(stderr, "   Falling back to temp file\n");
          audio_stream_player_destroy(g_stream_player);
          g_stream_player = NULL;
        }
      }
    }

    ethervox_tts_audio_t output = {0};
    int result;

    if (tts_speak_direct_mode) {
      // Bypass phonemizer - send IPA directly to Piper
      result = ethervox_tts_synthesize_ipa(g_global_tts, tts_text, &output);
    } else {
      // Normal text-to-speech with phonemizer
      result = ethervox_tts_synthesize_text(g_global_tts, tts_text, &output);
    }

    if (ethervox_is_success(result) && output.samples && output.sample_count > 0) {
      printf("   [OK] Synthesized %zu samples at %d Hz\n", output.sample_count, output.sample_rate);

      // Save to file only if -af was specified
      if (audio_file) {
        if (ethervox_is_success(ethervox_audio_write_wav(audio_file, output.samples,
                                                         (int)output.sample_count,
                                                         output.sample_rate, output.channels))) {
          printf("   💾 Saved to: %s\n", audio_file);
        } else {
          fprintf(stderr, "   ❌ Failed to save audio file\n");
        }
      }

      // Play audio - use streaming if available, otherwise temp file
      printf("   🎵 Playing audio...\n");

      if (g_stream_player) {
        // Streaming player active - chunks already playing, just wait for completion
        if (ethervox_is_success(audio_stream_player_wait(g_stream_player))) {
          printf("   [OK] Playback complete\n\n");
        } else {
          fprintf(stderr, "   ❌ Streaming playback error\n");
        }
      } else {
        // Fallback: save to temp file and play
        char temp_file[512];
        const char* mode_suffix = tts_speak_direct_mode ? "direct" : "speak";
        snprintf(temp_file, sizeof(temp_file), "%s/.ethervox/temp_%s.wav",
                 getenv("HOME") ? getenv("HOME") : ".", mode_suffix);

        if (ethervox_is_success(ethervox_audio_write_wav(temp_file, output.samples,
                                                         (int)output.sample_count,
                                                         output.sample_rate, output.channels))) {
#ifdef __APPLE__
          char cmd[600];
          snprintf(cmd, sizeof(cmd), "afplay %s", temp_file);
          system(cmd);
#elif __linux__
          char cmd[600];
          snprintf(cmd, sizeof(cmd), "aplay %s 2>/dev/null", temp_file);
          system(cmd);
#endif
          printf("   [OK] Playback complete\n\n");
        }
      }

      free(output.samples);
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      fprintf(stderr, "❌ TTS synthesis failed: %s\n",
              ctx && ctx->message ? ctx->message : ethervox_error_string(result));
    }

    // Cleanup and exit
    if (g_stream_player) {
      audio_stream_player_destroy(g_stream_player);
      g_stream_player = NULL;
    }
    if (g_global_tts) {
      ethervox_tts_destroy(g_global_tts);
      g_global_tts = NULL;
    }
    ethervox_memory_cleanup(&memory);
    return ethervox_is_success(result) ? 0 : 1;
  }

  // Test voices mode: demonstrate all configured voices
  if (tts_test_voices_mode) {
    if (!g_global_tts) {
      fprintf(stderr, "❌ TTS not initialized. Check that Piper model is configured.\n");
      fprintf(stderr, "   Run with -settings to configure TTS engine.\n");
      ethervox_memory_cleanup(&memory);
      return 1;
    }

    printf("\n🎭 Voice Demonstration Mode\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    // Load settings to check configured voices
    ethervox_persistent_settings_t test_settings = {0};
    if (ethervox_is_error(ethervox_settings_load(&test_settings, NULL))) {
      fprintf(stderr, "⚠️  Could not load settings, using defaults\n");
    }

    // Start streaming player if available
    bool using_streaming = false;
    if (g_stream_player) {
      if (ethervox_is_success(audio_stream_player_start(g_stream_player))) {
        using_streaming = true;
        printf("🎵 Using streaming audio playback\n\n");
      } else {
        audio_stream_player_destroy(g_stream_player);
        g_stream_player = NULL;
        printf("🎵 Using temp file playback\n\n");
      }
    } else {
      printf("🎵 Using temp file playback\n\n");
    }

    // Test sentences for each language
    typedef struct {
      const char* lang_code;
      const char* lang_name;
      const char* text;
      const char* voice_field;
    } voice_test_t;

    voice_test_t tests[] = {
        {"en", "English", "Hello! This is the English voice. Welcome to EthervoxAI.",
         test_settings.tts.voice_en},
        {"de", "German", "Hallo! Das ist die deutsche Stimme. Willkommen bei EthervoxAI.",
         test_settings.tts.voice_de},
        {"es", "Spanish", "¡Hola! Esta es la voz en español. Bienvenido a EthervoxAI.",
         test_settings.tts.voice_es},
        {"zh", "Chinese", "你好！这是中文语音。欢迎使用EthervoxAI。", test_settings.tts.voice_zh}};

    int total_tests = sizeof(tests) / sizeof(tests[0]);
    int successful_tests = 0;

    for (int i = 0; i < total_tests; i++) {
      printf("---------------------------------------------------------\n");
      printf("Language: %s (%s)\n", tests[i].lang_name, tests[i].lang_code);

      // Check if voice is configured
      if (!tests[i].voice_field || strlen(tests[i].voice_field) == 0) {
        printf("⚠️  No voice configured in settings (skipping)\n\n");
        continue;
      }

      printf("Voice:    %s\n", tests[i].voice_field);
      printf("Text:     \"%s\"\n\n", tests[i].text);

      // Detect language and switch voice
      const char* detected_lang =
          ethervox_detect_and_switch_voice(tests[i].text, NULL, (void**)&g_global_tts);

      if (strcmp(detected_lang, tests[i].lang_code) != 0) {
        printf("⚠️  Expected %s but detected %s\n", tests[i].lang_code, detected_lang);
      }

      // Synthesize
      ethervox_tts_audio_t output = {0};
      ethervox_result_t result = ethervox_tts_synthesize_text(g_global_tts, tests[i].text, &output);

      if (ethervox_is_success(result) && output.samples && output.sample_count > 0) {
        printf("[OK] Synthesized %zu samples at %d Hz\n", output.sample_count, output.sample_rate);

        // Play audio
        printf("🔊 Playing...\n");

        if (using_streaming && g_stream_player) {
          // Wait for streaming playback
          if (ethervox_is_success(audio_stream_player_wait(g_stream_player))) {
            printf("[OK] Playback complete\n\n");
            successful_tests++;
          } else {
            fprintf(stderr, "❌ Streaming playback error\n\n");
          }
        } else {
          // Fallback to temp file
          char temp_file[512];
          snprintf(temp_file, sizeof(temp_file), "%s/.ethervox/temp_voice_test_%s.wav",
                   getenv("HOME") ? getenv("HOME") : ".", tests[i].lang_code);

          if (ethervox_is_success(ethervox_audio_write_wav(temp_file, output.samples,
                                                           (int)output.sample_count,
                                                           output.sample_rate, output.channels))) {
#ifdef __APPLE__
            char cmd[600];
            snprintf(cmd, sizeof(cmd), "afplay %s", temp_file);
            system(cmd);
#elif __linux__
            char cmd[600];
            snprintf(cmd, sizeof(cmd), "aplay %s 2>/dev/null", temp_file);
            system(cmd);
#endif
            printf("[OK] Playback complete\n\n");
            successful_tests++;
          } else {
            fprintf(stderr, "❌ Failed to save/play audio\n\n");
          }
        }

        free(output.samples);
      } else {
        fprintf(stderr, "❌ Synthesis failed (error: %d)\n\n", result);
      }
    }

    // Summary
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Voice Test Summary: %d/%d successful\n\n", successful_tests, total_tests);

    // Cleanup
    if (g_stream_player) {
      audio_stream_player_destroy(g_stream_player);
      g_stream_player = NULL;
    }
    if (g_global_tts) {
      ethervox_tts_destroy(g_global_tts);
      g_global_tts = NULL;
    }
    ethervox_memory_cleanup(&memory);
    return successful_tests == total_tests ? 0 : 1;
  }

  // Initialize Governor
  ethervox_tool_registry_t registry;
  ethervox_voice_session_t voice_state;
  void* voice_session = NULL;

  result = ethervox_tool_registry_init(&registry, 16);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "❌ Failed to initialize tool registry: %s\n", ethervox_error_string(result));
    if (ctx && ctx->message && g_debug_enabled) {
      fprintf(stderr, "   %s at %s:%d\n", ctx->message, ctx->file, ctx->line);
    }
    ethervox_memory_cleanup(&memory);
    return 1;
  }

  // Initialize Governor with config (apply minimal mode if requested)
  ethervox_governor_config_t gov_config = ethervox_governor_default_config();

  // Apply runtime settings from persistent config
  // NOTE: Governor uses LLM settings for model params (gpu_layers, context, threads)
  //       and governor-specific settings for iteration control
  gov_config.confidence_threshold = g_settings.governor.confidence_threshold;
  gov_config.max_iterations = g_settings.governor.max_iterations;
  gov_config.timeout_seconds = g_settings.governor.timeout_seconds;
  gov_config.max_tokens_per_response = g_settings.governor.max_tokens_per_iteration;
  gov_config.gpu_layers = g_settings.llm.gpu_layers;         // Use LLM setting
  gov_config.context_size = g_settings.llm.context_length;   // Use LLM setting
  gov_config.n_threads = g_settings.llm.n_threads;           // Use LLM setting
  gov_config.temperature = g_settings.governor.temperature;  // Governor-specific

  if (g_debug_enabled) {
    printf("Governor settings applied from config:\n");
    printf("  - Max iterations: %u\n", gov_config.max_iterations);
    printf("  - Timeout: %u seconds\n", gov_config.timeout_seconds);
    printf("  - Confidence threshold: %.2f\n", gov_config.confidence_threshold);
    printf("  - Max tokens/iteration: %u\n", gov_config.max_tokens_per_response);
    printf("  - GPU layers: %u\n", gov_config.gpu_layers);
    printf("  - Context size: %u\n", gov_config.context_size);
    printf("  - Threads: %d\n", gov_config.n_threads);
    printf("  - Temperature: %.2f\n", gov_config.temperature);
  }

  if (minimal_mode) {
    gov_config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_MINIMAL;
    printf("⚡ Minimal mode: Using brief system prompt (~50 tokens vs ~1200)\n");
    printf("   Tools disabled for maximum loading speed\n");
  }

  result = ethervox_governor_init(&g_governor, &gov_config, &registry);
  if (ethervox_is_error(result)) {
    const ethervox_error_context_t* ctx = ethervox_error_get_context();
    fprintf(stderr, "❌ Failed to initialize Governor: %s\n", ethervox_error_string(result));
    if (ctx && ctx->message) {
      fprintf(stderr, "   %s\n", ctx->message);
      if (g_debug_enabled && ctx->file) {
        fprintf(stderr, "   at %s:%d in %s()\n", ctx->file, ctx->line, ctx->function);
      }
    }
    fprintf(stderr, "\n⚡ Troubleshooting:\n");
    fprintf(stderr, "   1. Check that you have enough free RAM\n");
    fprintf(stderr, "   2. Try reducing gpu_layers in settings (-settings flag)\n");
    fprintf(stderr, "   3. Try reducing context_size in settings\n");
    fprintf(stderr, "   4. Check model file exists and is not corrupted\n");
    ethervox_tool_registry_cleanup(&registry);
    ethervox_path_config_cleanup(&path_config);
    ethervox_memory_cleanup(&memory);
    return 1;
  }

  if (g_debug_enabled) {
    printf("Governor: Initialized\n");
  }

  // Register compute tools with Governor
  // NOTE: Returns int (count of tools), not ethervox_result_t - intentional
  int compute_count = ethervox_compute_tools_register_all(&registry);
  if (g_debug_enabled && compute_count > 0) {
    printf("Compute Tools: Registered %d tools with Governor\n", compute_count);
  }

  // Register memory tools with Governor
  result = ethervox_memory_tools_register(&registry, &memory);
  if (ethervox_is_success(result) && g_debug_enabled) {
    printf("Memory Tools: Registered with Governor\n");
  } else if (ethervox_is_error(result)) {
    // NOTE: Can be simplified to simple error code if preferred (non-critical tool)
    fprintf(stderr, "[WARNING] Memory tools registration failed: Error %d occurred\n", result);
  }

  // Initialize and register file tools (read-only by default)
  ethervox_file_tools_config_t file_config;

  // Set allowed base paths for file access
  const char* home_dir = getenv("HOME");
  const char* base_paths[4] = {NULL, NULL, NULL, NULL};
  int path_count = 0;

  if (home_dir) {
    // Allow home directory
    base_paths[path_count++] = home_dir;

    // Allow common document paths
    char docs_path[512];
    snprintf(docs_path, sizeof(docs_path), "%s/Documents", home_dir);
    char* docs_copy = strdup(docs_path);
    if (!docs_copy) {
      fprintf(stderr, "[ERROR] Failed to allocate memory for docs path\n");
      goto file_tools_cleanup;
    }
    base_paths[path_count++] = docs_copy;
  }

  // Allow current directory
  char cwd[512];
  if (getcwd(cwd, sizeof(cwd))) {
    char* cwd_copy = strdup(cwd);
    if (!cwd_copy) {
      fprintf(stderr, "[ERROR] Failed to allocate memory for cwd path\n");
      goto file_tools_cleanup;
    }
    base_paths[path_count++] = cwd_copy;
  }

  // Re-use result variable for file tools init
  result = ethervox_file_tools_init(&file_config, base_paths, ETHERVOX_FILE_ACCESS_READ_WRITE);
  if (ethervox_is_success(result)) {
    // Add allowed file extensions (batch check)
    const char* filters[] = {".txt", ".md", ".org", ".c", ".cpp", ".h", ".sh", NULL};
    int filters_added = 0;
    for (int i = 0; filters[i] != NULL; i++) {
      if (ethervox_is_success(ethervox_file_tools_add_filter(&file_config, filters[i]))) {
        filters_added++;
      } else if (g_debug_enabled) {
        fprintf(stderr, "[WARNING] Failed to add file filter %s\n", filters[i]);
      }
    }
    if (g_debug_enabled) {
      printf("File Tools: Added %d/%d file filters\n", filters_added,
             (int)(sizeof(filters) / sizeof(filters[0]) - 1));
    }

    // Register with Governor
    result = ethervox_file_tools_register(&registry, &file_config);
    if (ethervox_is_success(result) && g_debug_enabled) {
      const char* mode_icon =
          (file_config.access_mode == ETHERVOX_FILE_ACCESS_READ_ONLY) ? "🔒" : "🔓";
      const char* mode_str = (file_config.access_mode == ETHERVOX_FILE_ACCESS_READ_ONLY)
                                 ? "read-only (safe mode)"
                                 : "read-write";
      printf("File Tools: Registered with Governor (%s %s: .txt/.md/.org/.c/.cpp/.h/.sh)\n",
             mode_icon, mode_str);
      if (file_config.access_mode == ETHERVOX_FILE_ACCESS_READ_ONLY) {
        printf("            LLM can use file_set_safe_mode tool to enable writes when needed\n");
      }
    } else if (ethervox_is_error(result)) {
      fprintf(stderr, "[WARNING] File tools registration failed: Error %d occurred\n", result);
    }
  } else {
    fprintf(stderr, "[WARNING] File tools initialization failed: Error %d occurred\n", result);
  }

file_tools_cleanup:
  // Free allocated path strings
  for (int i = 1; i < path_count; i++) {  // Skip index 0 (home_dir is not allocated)
    if (base_paths[i] && base_paths[i] != home_dir) {
      free((void*)base_paths[i]);
    }
  }

  // Batch register optional tools
  typedef struct {
    const char* name;
    ethervox_result_t result;
  } tool_reg_result_t;

  tool_reg_result_t tool_registrations[] = {
      {"Path Config", ethervox_path_config_register(&registry, &path_config)},
      {"Unit Conversion", ethervox_unit_conversion_register(&registry)},
#if HAVE_LIBCURL
      {"Weather Forecast", ethervox_weather_tools_register(&registry)},
#endif
      {"Conversation", ethervox_conversation_tools_register(&registry)},
      {"Get Tool Info", ethervox_get_tool_info_register(&registry)},
      {"Startup Prompt", ethervox_startup_prompt_tools_register(&registry)},
      {"System Info", ethervox_system_info_tools_register(&registry)},
      {NULL, ETHERVOX_SUCCESS}};

  int tools_succeeded = 0, tools_failed = 0;
  for (int i = 0; tool_registrations[i].name != NULL; i++) {
    if (ethervox_is_success(tool_registrations[i].result)) {
      tools_succeeded++;
    } else {
      tools_failed++;
      fprintf(stderr, "[WARNING] Tool %s registration failed: Error %d occurred\n",
              tool_registrations[i].name, tool_registrations[i].result);
    }
  }

  if (g_debug_enabled) {
    printf("Tools: %d registered successfully, %d failed\n", tools_succeeded, tools_failed);
  }

  // Set up standalone callbacks for conversation tools
  // This allows the speak tool to work immediately without /convon
  static ethervox_conversation_callbacks_t g_standalone_callbacks = {0};
  g_standalone_callbacks.on_speak = standalone_speak_callback;
  g_standalone_callbacks.on_listen = standalone_listen_callback;
  g_standalone_callbacks.user_data = NULL;

  // Declare the external function (defined in speak.c)
  extern void ethervox_conversation_tools_set_callbacks(void* callbacks);
  ethervox_conversation_tools_set_callbacks(&g_standalone_callbacks);

  if (g_debug_enabled) {
    printf("Conversation Tools: Standalone callbacks registered (speak tool ready)\n");
  }

  // Initialize and register voice tools
  result = ethervox_voice_tools_init(&voice_state, &memory);
  if (ethervox_is_success(result)) {
    result = ethervox_voice_tools_register(&registry, &voice_state);
    if (ethervox_is_success(result)) {
      voice_session = &voice_state;
      g_transcription_session = &voice_state;
      if (g_debug_enabled) {
        printf("Voice Tools: Registered with Governor (Whisper STT with speaker detection)\n");
        printf("             Use /transcribe and /stoptranscribe commands\n");
      }
    } else {
      fprintf(stderr, "[WARNING] Voice tools registration failed: Error %d occurred\n", result);
    }
  } else {
    fprintf(stderr, "[WARNING] Voice tools initialization failed: Error %d occurred\n", result);
  }

  // Auto-load model if requested
  if (auto_load_model && model_path) {
    if (!quiet_mode) {
      printf("\n[INFO] [Governor] Auto-loading model: %s\n", model_path);
    }

    // Redirect stderr to suppress llama.cpp/ggml Metal compilation logs in quiet mode
    int stderr_backup = -1;
    if (quiet_mode) {
      stderr_backup = dup(STDERR_FILENO);
      int dev_null = open_devnull();
      if (dev_null != -1) {
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
      }
    }

    // Resolve model path (make it absolute if needed)
    char resolved_path[1024];
    if (model_path[0] != '/') {
      // Relative path - try multiple locations in order:
      // 1. ~/.ethervox/models/ (standard location)
      // 2. Same directory as executable
      // 3. One level up from executable (if in build/)
      // 4. Current working directory

      bool path_found = false;

      // Try 1: ~/.ethervox/models/
      char ethervox_model_path[512];
      if (ethervox_is_success(ethervox_get_runtime_path(ETHERVOX_MODELS_SUBDIR, ethervox_model_path,
                                                        sizeof(ethervox_model_path)))) {
        snprintf(resolved_path, sizeof(resolved_path), "%s/%s", ethervox_model_path, model_path);
        if (access(resolved_path, R_OK) == 0) {
          path_found = true;
          if (!quiet_mode) {
            printf("[INFO] Found model in standard location: %s\n", resolved_path);
          }
        }
      }

      // Try 2-4: Other locations

      // Try 2-4: Other locations
      char exe_path[512];
      char exe_dir[512];
#ifdef _WIN32
      DWORD len = 0;
#else
      ssize_t len = -1;
#endif

#ifdef __APPLE__
      // macOS uses _NSGetExecutablePath
      uint32_t size = sizeof(exe_path);
      if (_NSGetExecutablePath(exe_path, &size) == 0) {
        len = strlen(exe_path);
      }
#elif defined(_WIN32)
      // Windows uses GetModuleFileName
      len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
      if (len > 0 && len < sizeof(exe_path)) {
        exe_path[len] = '\0';
      } else {
        len = 0;
      }
#else
      len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
#endif

      if (len > 0) {
        exe_path[len] = '\0';
        char* last_slash = strrchr(exe_path, '/');
        if (last_slash) {
          size_t dir_len = last_slash - exe_path;
          strncpy(exe_dir, exe_path, dir_len);
          exe_dir[dir_len] = '\0';

          // Try 2: Same directory as executable
          if (!path_found) {
            snprintf(resolved_path, sizeof(resolved_path), "%s/%s", exe_dir, model_path);
            if (access(resolved_path, R_OK) == 0) {
              path_found = true;
            }
          }

          // Try 3: One level up (project root from build/)
          if (!path_found) {
            char parent_dir[512];
            strncpy(parent_dir, exe_dir, sizeof(parent_dir) - 1);
            parent_dir[sizeof(parent_dir) - 1] = '\0';

            char* second_last = strrchr(parent_dir, '/');
            if (second_last) {
              *second_last = '\0';
              snprintf(resolved_path, sizeof(resolved_path), "%s/%s", parent_dir, model_path);
              if (access(resolved_path, R_OK) == 0) {
                path_found = true;
              }
            }
          }
        }
      }

      // Try 4: Current working directory
      if (!path_found) {
        char cwd[512];
        if (getcwd(cwd, sizeof(cwd))) {
          snprintf(resolved_path, sizeof(resolved_path), "%s/%s", cwd, model_path);
        } else {
          snprintf(resolved_path, sizeof(resolved_path), "%s", model_path);
        }
      }
    } else {
      snprintf(resolved_path, sizeof(resolved_path), "%s", model_path);
    }

    // Check if file exists
    if (access(resolved_path, R_OK) != 0) {
      fprintf(stderr, "[ERROR] Model file not found or not readable: %s\n", resolved_path);
      fprintf(stderr, "[INFO] Run './scripts/download-governor-model.sh' to download it\n");
    } else {
      ethervox_result_t ret = ethervox_governor_load_model(g_governor, resolved_path, NULL, NULL);
      if (ethervox_is_success(ret)) {
        snprintf(g_loaded_model_path, sizeof(g_loaded_model_path), "%s", resolved_path);

        // Initialize manifest system after model load
        ethervox_result_t result = ethervox_governor_init_with_manifest(
            g_governor, g_loaded_model_path, &g_manifest_registry);
        if (ethervox_is_success(result)) {
          // Set manifest for get_tool_info meta-tool
          ethervox_get_tool_info_set_manifest(&g_manifest_registry);
        } else {
          const ethervox_error_context_t* ctx = ethervox_error_get_context();
          if (!quiet_mode) {
            fprintf(stderr, "[WARNING] Model loaded but manifest init failed: %s\n",
                    ctx && ctx->message ? ctx->message : ethervox_error_string(result));
          }
        }

        if (!quiet_mode) {
          printf("[INFO] [OK] Model loaded successfully\n");
        }
      } else {
        const ethervox_error_context_t* ctx = ethervox_error_get_context();
        fprintf(stderr, "[ERROR] [FAIL] Failed to load model: %s\n",
                ctx && ctx->message ? ctx->message : ethervox_error_string(ret));
      }
    }

    // Restore stderr if we redirected it
    if (quiet_mode && stderr_backup != -1) {
      dup2(stderr_backup, STDERR_FILENO);
      close(stderr_backup);
    }

    printf("\n");
  } else if (model_path) {
    printf("\nModel available: %s\n", model_path);
    printf("Use /load %s to load it, or restart with --load <path>\n\n", model_path);
  } else {
    printf("\nNo model specified. Use /load <path> or --load <path> flag.\n");
    printf("Recommended: granite-4.0-h-tiny-Q4_K_M.gguf (will search in ~/.ethervox/models/)\n\n");
  }

  if (!engineering_mode && g_debug_enabled) {
    print_help();
  }

  // Execute startup prompt if model is loaded and not skipped
  if (!skip_startup_prompt && g_loaded_model_path[0] != '\0') {
    char startup_prompt[2048];
    bool has_custom_prompt = false;

    // Determine startup prompt file location
    const char* home_dir = getenv("HOME");
    char default_prompt_file[512];
    if (home_dir) {
      snprintf(default_prompt_file, sizeof(default_prompt_file), "%s/.ethervox/startup_prompt.txt",
               home_dir);
    } else {
      snprintf(default_prompt_file, sizeof(default_prompt_file), "./.ethervox_startup_prompt.txt");
    }

    const char* prompt_file_to_use =
        startup_prompt_file ? startup_prompt_file : default_prompt_file;

    // Try to load custom startup prompt from file
    FILE* fp = fopen(prompt_file_to_use, "r");
    if (fp) {
      size_t len = fread(startup_prompt, 1, sizeof(startup_prompt) - 1, fp);
      startup_prompt[len] = '\0';
      fclose(fp);
      has_custom_prompt = true;
      if (!quiet_mode) {
        printf("\n[INFO] Using custom startup prompt from: %s\n", prompt_file_to_use);
      }
    } else if (startup_prompt_file) {
      // Only warn if a custom file was explicitly specified but not found
      fprintf(stderr, "[WARN] Could not open startup prompt file: %s\n", startup_prompt_file);
    }

    // Use default startup prompt if no custom one loaded
    if (!has_custom_prompt) {
      snprintf(startup_prompt, sizeof(startup_prompt), "%s", DEFAULT_STARTUP_PROMPT);
    }

    if (!quiet_mode) {
      printf("\n[Executing startup prompt...]\n");
    }

    // Redirect stderr to suppress llama.cpp Metal shader compilation logs
    int stderr_backup = -1;
    stderr_backup = dup(STDERR_FILENO);
    int dev_null = open_devnull();
    if (dev_null != -1) {
      dup2(dev_null, STDERR_FILENO);
      close(dev_null);
    }

    // Execute the startup prompt silently IN SECRET MODE (don't save to memory)
    // Save current privacy mode state
    bool original_privacy_mode = ethervox_memory_get_privacy_mode();

    // Enable privacy mode temporarily for startup prompt
    ethervox_memory_set_privacy_mode(true);

    char* response = NULL;
    char* error = NULL;

    ethervox_governor_status_t status =
        ethervox_governor_execute(g_governor, startup_prompt, &response, &error,
                                  NULL,  // No metrics
                                  NULL,  // No progress callback
                                  NULL,  // No token callback
                                  NULL   // No user data
        );

    // Restore original privacy mode state
    ethervox_memory_set_privacy_mode(original_privacy_mode);

    // Restore stderr
    if (stderr_backup != -1) {
      dup2(stderr_backup, STDERR_FILENO);
      close(stderr_backup);
    }

    if (status == ETHERVOX_GOVERNOR_SUCCESS && response) {
      // Display the startup response
      printf("\n\033[36m%s\033[0m\n\n", response);

      // Store the startup exchange in memory
      const char* tags[] = {"startup", "auto_generated"};
      uint64_t user_id, assistant_id;
      if (ethervox_is_success(
              ethervox_memory_store_add(&memory, startup_prompt, tags, 2, 0.7f, true, &user_id))) {
        ethervox_result_t store_result =
            ethervox_memory_store_add(&memory, response, tags, 2, 0.7f, false, &assistant_id);
        if (ethervox_is_error(store_result)) {
          ETHERVOX_LOG_DEBUG("Failed to store assistant response in memory: %s",
                             ethervox_error_string(store_result));
        }
      }

      free(response);
    } else if (error) {
      if (!quiet_mode) {
        fprintf(stderr, "[WARN] Startup prompt failed: %s\n\n", error);
      }
      free(error);
    }
  }

  // Run LLM tests if requested (--testllm mode)
  if (run_llm_tests) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" Running LLM Tool Tests (--testllm mode)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    if (strlen(g_loaded_model_path) == 0) {
      fprintf(stderr, "ERROR: No model loaded. LLM tests require a loaded model.\n");
      fprintf(stderr, "Use --model <path> with --testllm flag.\n");
      return 1;
    }

    // Run tests with verbose output
    run_llm_tool_tests(g_governor, &memory, g_loaded_model_path, true, test_name);

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" LLM Tests Complete\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    // Exit after tests (use _exit to bypass ONNX destructor bug)
    _exit(0);
  }

  // Run tool prompt optimization if requested (--optimize_tool_prompts mode)
  if (run_optimization) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" Tool Prompt Optimization - Enhanced Version (JSON Output)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    if (strlen(g_loaded_model_path) == 0) {
      fprintf(stderr, "ERROR: No model loaded. Optimization requires a loaded model.\n");
      fprintf(stderr, "Use --model <path> with --optimize_tool_prompts flag.\n");
      return 1;
    }

    printf("Model: %s\n", g_loaded_model_path);
    printf("\n");
    printf("This will:\n");
    printf("1. Export current tool registry to binary manifest\n");
    printf("2. Ask the LLM to optimize each tool description (~15 words)\n");
    printf("3. Save optimized prompts to JSON cache\n");
    printf("4. Reduce system prompt from ~15K to ~150 tokens (99%% reduction!)\n");
    printf("\n");

    // Suppress debug/info logs during optimization (keep user-facing output clean)
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_OFF);

    // Step 1: Initialize manifest system
    printf("Step 1: Initializing Tool Manifest System...\n");
    ethervox_result_t init_result =
        ethervox_governor_init_with_manifest(g_governor, g_loaded_model_path, &g_manifest_registry);
    if (ethervox_is_error(init_result)) {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("[FAIL] Failed to initialize manifest system: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(init_result));
      printf("  (Continuing anyway - will use runtime registry)\n");
    } else {
      printf("[OK] Manifest exported and loaded: %u tools\n",
             g_manifest_registry.header.tool_count);
    }

    // Step 2: Run optimizer
    printf("\nStep 2: Running optimization (this may take 30-60 seconds)...\n");
    ethervox_result_t ret =
        ethervox_optimize_tool_prompts(g_governor, g_loaded_model_path, &g_manifest_registry,
                                       true);  // optimize_new_only = true (incremental)

    if (ethervox_is_success(ret)) {
      printf("\n[OK] Optimization complete!\n");
      printf("\nNext steps:\n");
      printf("1. Restart EthervoxAI to use optimized prompts\n");
      printf("2. Your system prompt will now use ~150 tokens instead of ~15K\n");
      printf("3. Tool schemas will be injected on-demand only when called\n");
    } else if (ret == ETHERVOX_ERROR_INTERRUPTED) {
      printf("\n⚠️  Optimization cancelled by user\n");
    } else {
      const ethervox_error_context_t* ctx = ethervox_error_get_context();
      printf("\n[FAIL] Optimization failed: %s\n",
             ctx && ctx->message ? ctx->message : ethervox_error_string(ret));
      printf("  The assistant will still work with fallback level 1 (binary one-liners)\n");
    }

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" Optimization Complete\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");

    // Clean up resources explicitly before exit
    pthread_mutex_lock(&g_tts_mutex);
    if (g_global_tts) {
      ethervox_tts_destroy(g_global_tts);
      g_global_tts = NULL;
    }
    pthread_mutex_unlock(&g_tts_mutex);

    if (g_governor) {
      ethervox_governor_cleanup(g_governor);  // This already frees g_governor
      g_governor = NULL;
    }

    // Use _exit() to bypass C++ static destructors (prevents ONNX Runtime crash)
    // ONNX has global static objects with buggy destructors that fail during normal exit()
    _exit(0);
  }

  // Auto-start voice conversation if -convon flag was specified
  if (auto_start_conversation) {
    printf("[DEBUG] -convon flag detected, attempting auto-start...\n");
    if (!g_governor) {
      fprintf(stderr, "❌ Cannot start conversation: Governor not initialized\n");
      fprintf(stderr, "   g_governor = %p\n", (void*)g_governor);
    } else {
      printf("🎤 Auto-starting voice conversation mode...\n");
      printf("   g_governor = %p, model_path = %s\n", (void*)g_governor, g_loaded_model_path);

      ethervox_conversation_config_t config = ethervox_conversation_get_default_config();

      // Initialize conversation session with governor instance
      g_conversation_session = ethervox_conversation_init(&config, g_governor);
      if (!g_conversation_session) {
        fprintf(stderr, "❌ Failed to initialize conversation session\n");
      } else {
        printf("[OK] Conversation session initialized with Governor\n");

        // Start the conversation thread
        ethervox_result_t result = ethervox_conversation_start(g_conversation_session);
        if (ethervox_is_error(result)) {
          const ethervox_error_context_t* ctx = ethervox_error_get_context();
          fprintf(stderr, "❌ Failed to start conversation thread: %s\n",
                  ctx && ctx->message ? ctx->message : ethervox_error_string(result));
        } else {
          printf("[OK] Voice conversation ENABLED\n");
          printf("  Listening for wake word triggers...\n");
          printf("\n💡 Make sure wake word detection is enabled (/wakeon)\n");
          printf("   Say the wake word to start a conversation!\n\n");
        }
      }
    }
  }

  // Main REPL loop
  bool quit = false;

#if defined(__APPLE__) || defined(__linux__)
  // Set up tab completion for readline
  rl_attempted_completion_function = command_completion;

  // Use readline for command history on macOS/Linux
  while (g_running && !quit) {
    handle_pending_sigint_stop();
    if (!g_running || quit) {
      break;
    }
    char* line = readline("> ");

    if (!line) {
      handle_pending_sigint_stop();
      if (!g_running) {
        break;
      }
      continue;  // EOF (Ctrl+D) or signal
    }

    // Add non-empty lines to history
    if (line[0] != '\0') {
      add_history(line);
    }

    process_command(line, &memory, g_governor, &path_config, &file_config, voice_session, &quit);
    free(line);
    handle_pending_sigint_stop();

    // Check if voice session needs summarization
    if (voice_session && g_loaded_model_path[0] != '\0') {
      ethervox_voice_session_t* session = (ethervox_voice_session_t*)voice_session;
      if (session->needs_summarization && session->last_transcript_file[0] != '\0') {
        session->needs_summarization = false;  // Reset flag

        // Build summarization query with instruction to append to file
        char summary_query[2048];
        snprintf(
            summary_query, sizeof(summary_query),
            "Read the transcript file at '%s' and create a concise summary of the conversation. "
            "After creating the summary, append it to the end of the same transcript file with a "
            "clear "
            "heading '\\n\\n========================================\\nLLM "
            "SUMMARY\\n========================================\\n\\n' "
            "followed by your summary. Use the file_append tool to add the summary.",
            session->last_transcript_file);

        if (g_debug_enabled) {
          fprintf(stderr, "[DEBUG] Sending summarization query: %s\n", summary_query);
        }

        // Suppress all output during summarization unless debug is enabled
        int stdout_backup = -1;
        int stderr_backup = -1;
        if (!g_debug_enabled) {
          stdout_backup = dup(STDOUT_FILENO);
          stderr_backup = dup(STDERR_FILENO);
          int dev_null = open_devnull();
          if (dev_null != -1) {
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
          }
        }

        // Execute summarization
        char* response = NULL;
        char* error = NULL;

        ethervox_governor_status_t status =
            ethervox_governor_execute(g_governor, summary_query, &response, &error,
                                      NULL,  // No metrics
                                      NULL,  // No progress callback
                                      NULL,  // No token callback
                                      NULL   // No user data
            );

        // Restore output streams
        if (!g_debug_enabled) {
          fflush(stdout);
          fflush(stderr);
          if (stdout_backup != -1) {
            dup2(stdout_backup, STDOUT_FILENO);
            close(stdout_backup);
          }
          if (stderr_backup != -1) {
            dup2(stderr_backup, STDERR_FILENO);
            close(stderr_backup);
          }
        }

        if (status == ETHERVOX_GOVERNOR_SUCCESS && response) {
          printf("\n\u2713 Summary completed\n\n");
          if (g_debug_enabled) {
            fprintf(stderr, "[DEBUG] Summary response: %s\n", response);
          }
          free(response);
        } else {
          printf("\n\u26a0\ufe0f  Failed to generate summary\n");
          if (error) {
            printf("  Error: %s\n\n", error);
            free(error);
          }
        }
      }
    }
  }
#else
  // Fallback for platforms without readline
  char line[2048];
  while (g_running && !quit) {
    handle_pending_sigint_stop();
    if (!g_running || quit) {
      break;
    }
    printf("> ");
    fflush(stdout);

    if (!fgets(line, sizeof(line), stdin)) {
      handle_pending_sigint_stop();
      if (!g_running) {
        break;
      }
      continue;
    }

    // Remove newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    process_command(line, &memory, g_governor, &path_config, &file_config, voice_session, &quit);
    handle_pending_sigint_stop();

    // Check if voice session needs summarization
    if (voice_session && g_loaded_model_path[0] != '\0') {
      ethervox_voice_session_t* session = (ethervox_voice_session_t*)voice_session;
      if (session->needs_summarization && session->last_transcript_file[0] != '\0') {
        session->needs_summarization = false;  // Reset flag

        // Build summarization query with instruction to append to file
        char summary_query[2048];
        snprintf(
            summary_query, sizeof(summary_query),
            "Read the transcript file at '%s' and create a concise summary of the conversation. "
            "After creating the summary, append it to the end of the same transcript file with a "
            "clear "
            "heading '\\n\\n========================================\\nLLM "
            "SUMMARY\\n========================================\\n\\n' "
            "followed by your summary. Use the file_append tool to add the summary.",
            session->last_transcript_file);

        if (g_debug_enabled) {
          fprintf(stderr, "[DEBUG] Sending summarization query: %s\n", summary_query);
        }

        // Suppress all output during summarization unless debug is enabled
        int stdout_backup = -1;
        int stderr_backup = -1;
        if (!g_debug_enabled) {
          stdout_backup = dup(STDOUT_FILENO);
          stderr_backup = dup(STDERR_FILENO);
          int dev_null = open_devnull();
          if (dev_null != -1) {
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
          }
        }

        // Execute summarization
        char* response = NULL;
        char* error = NULL;

        ethervox_governor_status_t status =
            ethervox_governor_execute(g_governor, summary_query, &response, &error,
                                      NULL,  // No metrics
                                      NULL,  // No progress callback
                                      NULL,  // No token callback
                                      NULL   // No user data
            );

        // Restore output streams
        if (!g_debug_enabled) {
          fflush(stdout);
          fflush(stderr);
          if (stdout_backup != -1) {
            dup2(stdout_backup, STDOUT_FILENO);
            close(stdout_backup);
          }
          if (stderr_backup != -1) {
            dup2(stderr_backup, STDERR_FILENO);
            close(stderr_backup);
          }
        }

        if (status == ETHERVOX_GOVERNOR_SUCCESS && response) {
          printf("\\n\u2713 Summary completed\\n\\n");
          if (g_debug_enabled) {
            fprintf(stderr, "[DEBUG] Summary response: %s\n", response);
          }
          free(response);
        } else {
          printf("\\n\u26a0\ufe0f  Failed to generate summary\\n");
          if (error) {
            printf("  Error: %s\\n\\n", error);
            free(error);
          }
        }
      }
    }
  }
#endif

  // Cleanup
  printf("\n=== Final Statistics ===\n");
  print_stats(&memory);

  // Generate summary of this session before closing (if we have entries and storage)
  if (memory.entry_count > 2 && memory.storage_filepath[0] != '\0') {
    if (!quiet_mode) {
      printf("\nGenerating session summary...\n");
    }

    char* summary = NULL;
    char** key_points = NULL;
    uint32_t kp_count = 0;

    // Summarize the entire session
    ethervox_result_t ret = ethervox_memory_summarize(&memory, memory.entry_count, NULL, &summary,
                                                      &key_points, &kp_count);

    if (ethervox_is_success(ret) && summary) {
      // Store summary as a special memory entry for the next session
      const char* tags[] = {"session_summary", "auto_generated"};
      uint64_t summary_id;

      if (ethervox_is_error(
              ethervox_memory_store_add(&memory, summary, tags, 2, 0.95f, false, &summary_id))) {
        if (g_debug_enabled) {
          fprintf(stderr, "[WARN] Failed to store summary in memory\n");
        }
      }

      if (!quiet_mode) {
        printf("Session summary saved to memory.\n");
      }

      free(summary);
      if (key_points) {
        for (uint32_t i = 0; i < kp_count; i++) {
          free(key_points[i]);
        }
        free(key_points);
      }
    }
  }

  printf("Cleaning up...\n");

  // Suppress llama.cpp cleanup logs in quiet mode (but not when debug is enabled)
  int stderr_backup = -1;
  if (g_quiet_mode && !g_debug_enabled) {
    stderr_backup = dup(STDERR_FILENO);
    int dev_null = open_devnull();
    if (dev_null != -1) {
      dup2(dev_null, STDERR_FILENO);
      close(dev_null);
    }
  }

  ethervox_governor_cleanup(g_governor);
  ethervox_tool_registry_cleanup(&registry);
  ethervox_path_config_cleanup(&path_config);
  ethervox_memory_cleanup(&memory);

  // Cleanup wake word listening thread
  if (g_wake_thread_running) {
    g_wake_thread_running = false;
    // Thread will exit on its own
  }

  // Cleanup wake word detector
  if (g_wake_runtime) {
    ethervox_wake_cleanup(g_wake_runtime);
    free(g_wake_runtime);
    g_wake_runtime = NULL;
  }

  // Cleanup voice conversation (BEFORE global TTS)
  if (g_conversation_session) {
    ethervox_conversation_cleanup(g_conversation_session);
    g_conversation_session = NULL;
  }

  // Cleanup global TTS (AFTER conversation so session can detach cleanly)
  pthread_mutex_lock(&g_tts_mutex);
  if (g_global_tts) {
    ethervox_tts_destroy(g_global_tts);
    g_global_tts = NULL;
    if (g_debug_enabled) {
      printf("Global TTS: Cleaned up\n");
    }
  }
  pthread_mutex_unlock(&g_tts_mutex);

  // Cleanup wake word detector
  if (g_wake_runtime) {
    ethervox_wake_cleanup(g_wake_runtime);
    free(g_wake_runtime);
    g_wake_runtime = NULL;
  }

  // Restore stderr if we redirected it
  if (g_quiet_mode && !g_debug_enabled && stderr_backup != -1) {
    dup2(stderr_backup, STDERR_FILENO);
    close(stderr_backup);
  }

  printf("Goodbye!\n\n");

  // Use _exit() to bypass C++ static destructors (prevents ONNX Runtime crash)
  _exit(0);
}