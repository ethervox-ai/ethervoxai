/**
#include "ethervox/error.h"
 * @file voice_tools.c
 * @brief Voice tools implementation with Whisper STT
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/voice_tools.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>  // For Sleep
#else
#include <unistd.h>
#endif

#include "ethervox/file_tools.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/config.h"
#include "ethervox/platform_utils.h"
#include "ethervox/platform_utils.h"

// Forward declaration - implemented in JNI layer (Android) or returns NULL (other platforms)
extern const char* ethervox_get_android_files_dir(void);

#define LOG_ERROR(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) \
  ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Memory tag for path configurations
#define PATH_CONFIG_TAG "user_path"
#define PATH_CONFIG_PREFIX "USER_PATH:"

/**
 * Attempt to download Whisper model using download script
 */
static int download_whisper_model(const char* model_name, const char* dest_dir) {
  char script_path[1024];
  char command[2048];

  // Find download script
  const char* script_locations[] = {
      "scripts/download-whisper-model.sh", "./scripts/download-whisper-model.sh",
      "../scripts/download-whisper-model.sh", "../../scripts/download-whisper-model.sh", NULL};

  const char* script = NULL;
  for (int i = 0; script_locations[i] != NULL; i++) {
    FILE* test = fopen(script_locations[i], "r");
    if (test) {
      fclose(test);
      script = script_locations[i];
      break;
    }
  }

  if (!script) {
    LOG_ERROR("Download script not found. Please manually download the model.");
    LOG_ERROR("Visit: https://huggingface.co/ggerganov/whisper.cpp/tree/main");
    LOG_ERROR("Download: ggml-%s.bin and place in %s/", model_name, dest_dir);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Create destination directory
  ethervox_result_t mkdir_result = platform_mkdir_recursive(dest_dir);
  if (ethervox_is_error(mkdir_result)) {
    LOG_ERROR("Failed to create directory: %s", dest_dir);
    return mkdir_result;
  }

  // Run download script
  LOG_INFO("Downloading Whisper model '%s' to %s/", model_name, dest_dir);
  LOG_INFO("This may take a few minutes...");

  snprintf(command, sizeof(command), "bash \"%s\" \"%s\" \"%s\"", script, model_name, dest_dir);

  int result = system(command);
  if (result != 0) {
    LOG_ERROR("Model download failed with code %d", result);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  LOG_INFO("Model download completed successfully");
  return ETHERVOX_SUCCESS;
}

// Global session pointer for tool wrappers
static ethervox_voice_session_t* g_voice_session = NULL;

/**
 * Background thread that captures audio and feeds to STT
 *
 * This thread continuously feeds audio to Whisper's internal buffer and lets
 * Whisper's VAD decide when to segment and transcribe at natural speech pauses.
 * We only force processing when:
 *   1. Buffer approaches capacity (~30s at 90% full)
 *   2. User calls /stoptranscribe (handled by stop_listen -> finalize)
 */
static void* audio_capture_thread(void* arg) {
  ethervox_voice_session_t* session = (ethervox_voice_session_t*)arg;

  ethervox_audio_buffer_t audio_buf;
  audio_buf.data = (float*)malloc(16000 * sizeof(float));  // 1 second buffer
  audio_buf.size = 16000;
  audio_buf.channels = 1;
  audio_buf.timestamp_us = 0;
  const uint32_t buffer_capacity = audio_buf.size;

  if (!audio_buf.data) {
    LOG_ERROR("Failed to allocate audio buffer");
    return NULL;
  }

  LOG_INFO("Audio capture thread started - feeding to Whisper VAD");

  while (session->is_recording && !session->stop_requested) {
    // Read audio from platform driver
    // CRITICAL: Reset buffer size before each read to prevent buffer overflow
    audio_buf.size = buffer_capacity;
    ethervox_result_t read_result = ethervox_audio_read(&session->audio_runtime, &audio_buf);

    if (ethervox_is_success(read_result) && audio_buf.size > 0) {
      int samples_read = (int)audio_buf.size;
      
      // Clamp samples_read to buffer capacity to prevent overflow
      if (samples_read > (int)buffer_capacity) {
        LOG_WARN("Audio driver returned more samples (%d) than buffer capacity (%u) - clamping",
                 samples_read, buffer_capacity);
        samples_read = (int)buffer_capacity;
      }
      
      // Set actual size for STT processing
      audio_buf.size = (uint32_t)samples_read;

      // Feed audio to STT - it will accumulate internally
      // Whisper's VAD will decide when to segment and transcribe
      ethervox_stt_result_t result;
      ethervox_result_t stt_ret = ethervox_stt_process(&session->stt_runtime, &audio_buf, &result);

      if (stt_ret == 1) {
        // Normal: audio is accumulating in Whisper's buffer, VAD hasn't triggered yet
        // This is the expected path most of the time
        continue;
      } else if (stt_ret < 0) {
        LOG_WARN("STT processing error: %d", stt_ret);
        continue;
      }

      // stt_ret == 0: Whisper's VAD detected a natural speech boundary and transcribed
      if (ethervox_is_success(stt_ret) && result.text && strlen(result.text) > 0) {
        LOG_INFO("📥 Whisper VAD segment complete: %zu chars", strlen(result.text));

        // Track max speaker ID for later naming
        // Text contains [Speaker N] markers - extract highest N
        const char* speaker_marker = strstr(result.text, "[Speaker ");
        while (speaker_marker) {
          int speaker_id = -1;
          if (sscanf(speaker_marker, "[Speaker %d]", &speaker_id) == 1) {
            if (speaker_id > session->max_speaker_id) {
              session->max_speaker_id = speaker_id;
            }
          }
          speaker_marker = strstr(speaker_marker + 1, "[Speaker ");
        }

        // Format segment with timestamp and language info
        char formatted_segment[8192];
        
        // Get current date/time
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char datetime_str[64];
        strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        // Add date/time timestamp and language to the transcript
        snprintf(formatted_segment, sizeof(formatted_segment), 
                 "[%s] (%s) %s",
                 datetime_str, result.language, result.text);

        // Append to transcript buffer (in-memory)
        size_t needed = session->transcript_len + strlen(formatted_segment) + 2;
        if (needed > session->transcript_capacity) {
          session->transcript_capacity = needed * 2;
          session->full_transcript =
              (char*)realloc(session->full_transcript, session->transcript_capacity);
        }

        if (session->transcript_len > 0) {
          strcat(session->full_transcript, "\n");
          session->transcript_len++;
        }
        strcat(session->full_transcript, formatted_segment);
        session->transcript_len += strlen(formatted_segment);
        session->segment_count++;

        LOG_INFO("Segment %u: %s", session->segment_count, formatted_segment);

        // LIVE UPDATE: Append to file immediately for LLM monitoring
        if (session->last_transcript_file[0] != '\0') {
          FILE* f = fopen(session->last_transcript_file, "a");
          if (f) {
            fprintf(f, "%s\n", formatted_segment);
            fflush(f);  // Ensure data is written immediately
            fclose(f);
            LOG_DEBUG("Updated live transcript file: %s", session->last_transcript_file);
          } else {
            LOG_WARN("Failed to open transcript file for writing: %s",
                     session->last_transcript_file);
          }
        }

        ethervox_stt_result_free(&result);
      }
      // Note: buffer size is reset at the top of the loop before next read
    } else if (ethervox_is_success(read_result) && audio_buf.size == 0) {
      // No audio available yet, sleep briefly
#ifdef _WIN32
      Sleep(50);  // 50ms
#else
      usleep(50000);  // 50ms
#endif
    } else {
      LOG_ERROR("Audio read error: result=%d", read_result);
      break;
    }
  }

  free(audio_buf.data);
  LOG_INFO("Audio capture thread stopped");
  return NULL;
}

/**
 * Initialize voice tools with Whisper backend
 */
ethervox_result_t ethervox_voice_tools_init(ethervox_voice_session_t* session, void* memory) {
  if (!session) {
    LOG_ERROR("Session is NULL");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  memset(session, 0, sizeof(ethervox_voice_session_t));

  session->memory_store = memory;
  session->transcript_capacity = 1024 * 100;  // 100KB initial
  session->full_transcript = (char*)calloc(session->transcript_capacity, 1);

  if (!session->full_transcript) {
    LOG_ERROR("Failed to allocate transcript buffer");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Initialize path config to use standard directories
  ethervox_path_config_t path_config;
  if (ethervox_is_error(ethervox_path_config_init(&path_config, memory))) {
    LOG_WARN("Failed to initialize path config, using fallback paths");
  }

  // Build comprehensive path search list
  // CRITICAL: Search for multilingual models to ensure auto language detection works
  // Preferred order: tiny.bin (small, fast), base.bin (balanced), then .en variants
  const int MAX_MODEL_PATHS = 64;
  char possible_paths[MAX_MODEL_PATHS][ETHERVOX_FILE_MAX_PATH];
  int path_count = 0;
  bool path_overflow_logged = false;

  // Model search priority: tiny.bin > base.bin > tiny.en.bin > base.en.bin
  const char* model_candidates[] = {"tiny.bin", "ggml-tiny.bin", "base.bin"};
  const size_t model_candidate_count = sizeof(model_candidates) / sizeof(model_candidates[0]);
  const char* preferred_model = model_candidates[0];  // Start with tiny.bin
  bool found_multilingual = false;

  char custom_path[ETHERVOX_FILE_MAX_PATH];
  bool has_custom_path = ethervox_is_success(ethervox_path_config_get(&path_config, "WhisperModels", custom_path,
                                                   sizeof(custom_path)));

  ethervox_user_path_t* user_paths = NULL;
  uint32_t user_path_count = 0;
  bool has_user_paths = ethervox_is_success(ethervox_path_config_list(&path_config, &user_paths, &user_path_count));

  // Platform-specific default paths
  const char* android_files_dir = ethervox_get_android_files_dir();
  char default_model_dir[ETHERVOX_FILE_MAX_PATH];
  char local_app_data[512];
  
  if (android_files_dir) {
    // Android: Use app-specific files directory
    snprintf(default_model_dir, sizeof(default_model_dir), "%s/models/whisper", android_files_dir);
    LOG_INFO("[Android] Using app files directory for models: %s", default_model_dir);
  } else if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS) {
    // Windows/macOS/Linux: Use platform-specific local app data
    snprintf(default_model_dir, sizeof(default_model_dir), "%s%cmodels%cwhisper", local_app_data,
#ifdef _WIN32
             '\\', '\\'
#else
             '/', '/'
#endif
    );
    LOG_DEBUG("Using platform app data for models: %s", default_model_dir);
  } else {
    // Fallback: relative path
    snprintf(default_model_dir, sizeof(default_model_dir), "models%cwhisper",
#ifdef _WIN32
             '\\'
#else
             '/'
#endif
    );
    LOG_WARN("No platform-specific path available, using relative path");
  }

  // Build search paths for preferred multilingual model
  if (has_custom_path) {
    if (path_count < MAX_MODEL_PATHS) {
      snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH, "%s/%s", custom_path,
               preferred_model);
      LOG_DEBUG("Added custom model path candidate: %s/%s", custom_path, preferred_model);
    }
  }

  if (has_user_paths && user_paths) {
    for (uint32_t i = 0; i < user_path_count; i++) {
      if (!user_paths[i].verified)
        continue;
      if (path_count < MAX_MODEL_PATHS) {
        snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH,
                 "%s/ethervox/models/whisper/%s", user_paths[i].path, preferred_model);
      }
      if (path_count < MAX_MODEL_PATHS) {
        snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH,
                 "%s/.ethervox/models/whisper/%s", user_paths[i].path, preferred_model);
      }
    }
  }

  // Standard paths
  const char* standard_paths[] = {".ethervox/models/whisper/%s",
                                  "./.ethervox/models/whisper/%s",
                                  "../.ethervox/models/whisper/%s",
                                  "models/whisper/%s",
                                  "./models/whisper/%s",
                                  "../models/whisper/%s",
                                  "../../models/whisper/%s"};
  for (size_t i = 0; i < sizeof(standard_paths) / sizeof(standard_paths[0]); i++) {
    if (path_count < MAX_MODEL_PATHS) {
      snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH, standard_paths[i],
               preferred_model);
    }
  }

  // Add platform-specific app data paths
  if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS) {
    if (path_count < MAX_MODEL_PATHS) {
      snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH,
               "%s%cmodels%cwhisper%c%s", local_app_data,
#ifdef _WIN32
               '\\', '\\', '\\',
#else
               '/', '/', '/',
#endif
               preferred_model);
    }
  }

  // Try to find multilingual model - search all candidates in priority order
  const char* model_path = NULL;
  for (size_t m = 0; m < model_candidate_count && !found_multilingual; m++) {
    preferred_model = model_candidates[m];
    
    // Rebuild paths for this candidate
    path_count = 0;
    
    // PLATFORM-SPECIFIC: Add Android or default paths as highest priority
    if (android_files_dir) {
      // Android: Use app-specific files directory FIRST
      if (path_count < MAX_MODEL_PATHS) {
        snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH,
                 "%s/models/whisper/%s", android_files_dir, preferred_model);
        LOG_DEBUG("[Android] Added path: %s", possible_paths[path_count - 1]);
      }
    }
    
    if (has_custom_path) {
      if (path_count < MAX_MODEL_PATHS) {
        snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH, "%s/%s", custom_path,
                 preferred_model);
      }
    }
    
    if (has_user_paths && user_paths) {
      for (uint32_t i = 0; i < user_path_count; i++) {
        if (!user_paths[i].verified)
          continue;
        if (path_count < MAX_MODEL_PATHS) {
          snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH,
                   "%s/ethervox/models/whisper/%s", user_paths[i].path, preferred_model);
        }
        if (path_count < MAX_MODEL_PATHS) {
          snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH,
                   "%s/.ethervox/models/whisper/%s", user_paths[i].path, preferred_model);
        }
      }
    }
    
    for (size_t i = 0; i < sizeof(standard_paths) / sizeof(standard_paths[0]); i++) {
      if (path_count < MAX_MODEL_PATHS) {
        snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH, standard_paths[i],
                 preferred_model);
      }
    }
    
    // Add platform-specific app data paths for this candidate
    if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS) {
      if (path_count < MAX_MODEL_PATHS) {
        snprintf(possible_paths[path_count++], ETHERVOX_FILE_MAX_PATH,
                 "%s%cmodels%cwhisper%c%s", local_app_data,
#ifdef _WIN32
                 '\\', '\\', '\\',
#else
                 '/', '/', '/',
#endif
                 preferred_model);
      }
    }
    
    // Try to find this candidate
    for (int i = 0; i < path_count; i++) {
      FILE* test = fopen(possible_paths[i], "rb");
      if (test) {
        fclose(test);
        model_path = strdup(possible_paths[i]);
        session->model_path = (char*)model_path;
        found_multilingual = true;
        LOG_INFO("Found multilingual Whisper model at: %s", model_path);
        break;
      }
    }
  }

  // Fallback: if multilingual not found, try English-only variants
  if (!found_multilingual) {
    LOG_WARN("Multilingual models (tiny.bin, base.bin) not found, falling back to English-only variants");
    const char* fallback_models[] = {"tiny.en.bin", "ggml-tiny.en.bin", "base.en.bin"};
    const size_t fallback_model_count = sizeof(fallback_models) / sizeof(fallback_models[0]);
    
    for (size_t m = 0; m < fallback_model_count && !model_path; m++) {
      const char* fallback_model = fallback_models[m];
      int fallback_count = 0;

      // Rebuild paths with fallback model
      if (has_custom_path && fallback_count < MAX_MODEL_PATHS) {
        snprintf(possible_paths[fallback_count++], ETHERVOX_FILE_MAX_PATH, "%s/%s", custom_path,
                 fallback_model);
      }

      if (has_user_paths && user_paths) {
        for (uint32_t i = 0; i < user_path_count && fallback_count < MAX_MODEL_PATHS; i++) {
          if (!user_paths[i].verified)
            continue;
          snprintf(possible_paths[fallback_count++], ETHERVOX_FILE_MAX_PATH,
                   "%s/ethervox/models/whisper/%s", user_paths[i].path, fallback_model);
          snprintf(possible_paths[fallback_count++], ETHERVOX_FILE_MAX_PATH,
                   "%s/.ethervox/models/whisper/%s", user_paths[i].path, fallback_model);
        }
      }

      for (size_t i = 0;
           i < sizeof(standard_paths) / sizeof(standard_paths[0]) && fallback_count < MAX_MODEL_PATHS;
           i++) {
        snprintf(possible_paths[fallback_count++], ETHERVOX_FILE_MAX_PATH, standard_paths[i],
                 fallback_model);
      }

      // Add platform-specific app data paths for fallback
      if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS) {
        if (fallback_count < MAX_MODEL_PATHS) {
          snprintf(possible_paths[fallback_count++], ETHERVOX_FILE_MAX_PATH,
                   "%s%cmodels%cwhisper%c%s", local_app_data,
#ifdef _WIN32
                   '\\', '\\', '\\',
#else
                   '/', '/', '/',
#endif
                   fallback_model);
        }
      }

      for (int i = 0; i < fallback_count; i++) {
        FILE* test = fopen(possible_paths[i], "rb");
        if (test) {
          fclose(test);
          model_path = strdup(possible_paths[i]);
          session->model_path = (char*)model_path;
          LOG_INFO("Found English-only Whisper model at: %s", model_path);
          break;
        }
      }
      
      path_count = fallback_count;
      if (model_path) break;
    }
  }

  if (has_user_paths && user_paths) {
    free(user_paths);
  }

  if (!model_path) {
    LOG_WARN("Whisper model not found. Searched %d locations:", path_count);
    for (int i = 0; i < path_count && i < 5; i++) {
      LOG_WARN("  - %s", possible_paths[i]);
    }
    if (path_count > 5) {
      LOG_WARN("  ... and %d more locations", path_count - 5);
    }

    // Offer to auto-download
    LOG_INFO("========================================");
    LOG_INFO("Whisper Model Auto-Download Available");
    LOG_INFO("========================================");
    LOG_INFO("Would you like to download the Whisper base (multilingual) model (~141 MB)?");
    LOG_INFO("This is a one-time download and will be saved to: %s", default_model_dir);
    LOG_INFO("");
    LOG_INFO("Options:");
    LOG_INFO("  1. Auto-download now (recommended for first-time setup)");
    LOG_INFO("  2. Manual download instructions");
    LOG_INFO("  3. Configure custom model path with path_set tool");
    LOG_INFO("");

    // Attempt auto-download to standard location
    const char* download_dir = default_model_dir;
    LOG_INFO("Attempting auto-download to %s/...", download_dir);

    if (download_whisper_model("base", download_dir) == 0) {
      // Try to find the downloaded model
      const char* downloaded_candidates[] = {"base.bin", "base.en.bin"};
      const size_t downloaded_candidate_count =
          sizeof(downloaded_candidates) / sizeof(downloaded_candidates[0]);
      char downloaded_path[ETHERVOX_FILE_MAX_PATH];
      bool downloaded_model_found = false;

      for (size_t i = 0; i < downloaded_candidate_count && !downloaded_model_found; ++i) {
        snprintf(downloaded_path, sizeof(downloaded_path), "%s/%s", download_dir,
                 downloaded_candidates[i]);
        FILE* test = fopen(downloaded_path, "rb");
        if (test) {
          fclose(test);
          downloaded_model_found = true;
        }
      }

      if (downloaded_model_found) {
        model_path = strdup(downloaded_path);
        session->model_path = (char*)model_path;
        LOG_INFO("Successfully downloaded and loaded model from: %s", model_path);
      } else {
        LOG_ERROR("Download completed but model file not found in %s", download_dir);
        ethervox_path_config_cleanup(&path_config);
        free(session->full_transcript);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
      }
    } else {
      LOG_ERROR("Auto-download failed. Manual download required:");
      LOG_ERROR("");
      LOG_ERROR("Option 1 - Use download script:");
      LOG_ERROR("  ./scripts/download-whisper-model.sh base");
      LOG_ERROR("");
      LOG_ERROR("Option 2 - Manual download:");
      LOG_ERROR("  1. Visit: https://huggingface.co/ggerganov/whisper.cpp/tree/main");
      LOG_ERROR("  2. Download: ggml-base.bin (~141 MB, multilingual)");
      LOG_ERROR("  3. Place in: ~/.ethervox/models/whisper/base.bin");
      LOG_ERROR("");
      LOG_ERROR("Option 3 - Configure custom path:");
      LOG_ERROR("  Use path_set(\"WhisperModels\", \"/your/path/to/.ethervox/models/whisper\")");

      ethervox_path_config_cleanup(&path_config);
      free(session->full_transcript);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
  }

  // Determine language mode based on model filename
  const char* model_filename = strrchr(model_path, '/');
  model_filename = model_filename ? model_filename + 1 : model_path;
  bool english_only_model = false;
  if (model_filename) {
    size_t name_len = strlen(model_filename);
    english_only_model = (strstr(model_filename, ".en.") != NULL) ||
                         (strstr(model_filename, "_en.") != NULL) ||
                         (strstr(model_filename, "-en.") != NULL) ||
                         (name_len >= 7 && strcmp(model_filename + name_len - 7, ".en.bin") == 0);
  }

  const char* language_code = english_only_model ? "en" : "auto";
  if (!english_only_model) {
    LOG_INFO("Detected multilingual Whisper model (%s) - enabling auto language detection",
             model_filename ? model_filename : model_path);
  } else {
    LOG_INFO("Detected English-only Whisper model (%s) - forcing language=en",
             model_filename ? model_filename : model_path);
  }

  // Configure STT with Whisper backend
  ethervox_stt_config_t stt_config = {.backend = ETHERVOX_STT_BACKEND_WHISPER,  // Use Whisper!
                                      .model_path = model_path,
                                      .language = language_code,
                                      .sample_rate = 16000,
                                      .enable_partial_results = true,
                                      .enable_punctuation = true,
                                      .vad_threshold = 0.5f};

  // Initialize STT with Whisper
  if (ethervox_stt_init(&session->stt_runtime, &stt_config) != 0) {
    LOG_ERROR("Failed to initialize Whisper STT");
    free(session->model_path);  // Clean up allocated path
    session->model_path = NULL;
    ethervox_path_config_cleanup(&path_config);
    free(session->full_transcript);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Initialize audio (will use platform-specific implementation)
  ethervox_audio_config_t audio_config = ethervox_audio_get_default_config();
  if (ethervox_audio_init(&session->audio_runtime, &audio_config) != 0) {
    LOG_WARN("Audio init failed - will use test data");
    // Continue anyway - we can test with simulated audio
  }

  // Cleanup path config (model_path is now owned by stt_config)
  ethervox_path_config_cleanup(&path_config);

  // Initialize speaker tracking
  session->max_speaker_id = -1;
  session->speaker_names = NULL;
  session->speaker_names_capacity = 0;
  session->needs_summarization = false;

  session->is_initialized = true;
  LOG_INFO("Voice tools initialized with Whisper STT backend");

  return ETHERVOX_SUCCESS;
}

/**
 * Start listening session
 */
ethervox_result_t ethervox_voice_tools_start_listen(ethervox_voice_session_t* session) {
  if (!session || !session->is_initialized) {
    LOG_ERROR("Session not initialized");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (session->is_recording) {
    LOG_WARN("Already recording");
    return ETHERVOX_SUCCESS;
  }

  // Reset transcript
  session->full_transcript[0] = '\0';
  session->transcript_len = 0;
  session->segment_count = 0;
  session->session_start_time = time(NULL);
  session->stop_requested = false;
  
  // Reset speaker tracking for new session
  session->max_speaker_id = -1;
  if (session->speaker_names) {
    for (int i = 0; i < session->speaker_names_capacity; i++) {
      free(session->speaker_names[i]);
    }
    free(session->speaker_names);
    session->speaker_names = NULL;
    session->speaker_names_capacity = 0;
  }

  // Start STT
  if (ethervox_stt_start(&session->stt_runtime) != 0) {
    LOG_ERROR("Failed to start STT");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  LOG_INFO("Voice recording started - listening for speech...");
  LOG_DEBUG("Language: %s, VAD threshold: %.2f", session->stt_runtime.config.language,
            session->stt_runtime.config.vad_threshold);

  // Start audio capture
  if (!session->audio_runtime.is_initialized) {
    LOG_ERROR("Audio runtime not initialized");
    ethervox_stt_stop(&session->stt_runtime);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (ethervox_audio_start_capture(&session->audio_runtime) != 0) {
    LOG_ERROR("Failed to start audio capture");
    ethervox_stt_stop(&session->stt_runtime);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  session->is_recording = true;

  // Create transcript file immediately for live LLM monitoring
  // Use platform-specific path (Android app files or platform app data)
  const char* android_files_dir = ethervox_get_android_files_dir();
  char transcript_dir[512];
  char local_app_data[512];
  
  if (android_files_dir) {
    // Android: Use app-specific files directory
    snprintf(transcript_dir, sizeof(transcript_dir), "%s/transcripts", android_files_dir);
    LOG_INFO("[Android] Saving transcripts to: %s", transcript_dir);
  } else if (platform_get_local_app_data_dir(local_app_data, sizeof(local_app_data)) == ETHERVOX_SUCCESS) {
    // Windows/macOS/Linux: Use platform-specific local app data
    snprintf(transcript_dir, sizeof(transcript_dir), "%s%ctranscripts", local_app_data,
#ifdef _WIN32
             '\\'
#else
             '/'
#endif
    );
    LOG_INFO("Saving transcripts to: %s", transcript_dir);
  } else {
    // Fallback: current directory
    snprintf(transcript_dir, sizeof(transcript_dir), "transcripts");
    LOG_WARN("No platform-specific path, using ./transcripts");
  }
  
  // Create directory (platform-safe)
#ifdef _WIN32
  int mkdir_result = _mkdir(transcript_dir);
#else
  int mkdir_result = mkdir(transcript_dir, 0755);
#endif
  if (mkdir_result != 0 && errno != EEXIST) {
    LOG_ERROR("Failed to create transcript directory %s: errno=%d", transcript_dir, errno);
  } else {
    LOG_INFO("Transcript directory ready: %s", transcript_dir);
  }

  time_t now = time(NULL);
  struct tm* tm_info = localtime(&now);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

  snprintf(session->last_transcript_file, sizeof(session->last_transcript_file),
           "%s/transcript_%s.txt", transcript_dir, timestamp);

  // Create file with header
  LOG_INFO("Attempting to create transcript file: %s", session->last_transcript_file);
  FILE* f = fopen(session->last_transcript_file, "w");
  if (f) {
    fprintf(f, "Voice Transcript - Recording Started: %s\n", timestamp);
    fprintf(f, "========================================\n\n");
    fflush(f);  // Force write to disk
    fclose(f);
    LOG_INFO("[OK] Created live transcript file: %s", session->last_transcript_file);
  } else {
    LOG_ERROR("[FAIL] Failed to create transcript file: %s (errno=%d)", session->last_transcript_file, errno);
    session->last_transcript_file[0] = '\0';  // Clear the path since file creation failed
  }

  // Start background capture thread
  pthread_t* thread = (pthread_t*)malloc(sizeof(pthread_t));
  if (thread && pthread_create(thread, NULL, audio_capture_thread, session) == 0) {
    session->capture_thread = thread;
    LOG_INFO("Voice recording session started with background thread");
  } else {
    LOG_ERROR("Failed to create capture thread");
    free(thread);
    ethervox_audio_stop_capture(&session->audio_runtime);
    ethervox_stt_stop(&session->stt_runtime);
    session->is_recording = false;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  return ETHERVOX_SUCCESS;
}

/**
 * Stop listening and get transcript
 */
ethervox_result_t ethervox_voice_tools_stop_listen(ethervox_voice_session_t* session,
                                     const char** transcript_out) {
  if (!session || !session->is_initialized) {
    LOG_ERROR("Session not initialized");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  bool was_recording = session->is_recording;
  bool has_thread = (session->capture_thread != NULL);

  if (!was_recording && !has_thread) {
    LOG_WARN("Not currently recording");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Signal thread to stop (even if flag already dropped)
  session->stop_requested = true;
  session->is_recording = false;

  // Wait for capture thread to finish if it is still running
  if (has_thread) {
    pthread_t* thread = (pthread_t*)session->capture_thread;
    pthread_join(*thread, NULL);
    free(thread);
    session->capture_thread = NULL;
    LOG_INFO("Capture thread joined - all audio fed to STT");
  }

  // Stop audio capture
  if (session->audio_runtime.is_initialized) {
    ethervox_audio_stop_capture(&session->audio_runtime);
  }

  // CRITICAL: Finalize STT to process any remaining buffered audio
  // This forces Whisper to transcribe whatever is left in the buffer,
  // even if VAD hasn't triggered yet (handles the /stoptranscribe case)
  LOG_INFO("Finalizing Whisper transcription for remaining audio...");
  ethervox_stt_result_t final_result;
  if (ethervox_stt_finalize(&session->stt_runtime, &final_result) == 0) {
    if (final_result.text && strlen(final_result.text) > 0) {
      LOG_INFO("Finalize produced %zu chars", strlen(final_result.text));
      // Append final text to transcript
      size_t needed = session->transcript_len + strlen(final_result.text) + 2;
      if (needed > session->transcript_capacity) {
        session->transcript_capacity = needed * 2;
        session->full_transcript =
            (char*)realloc(session->full_transcript, session->transcript_capacity);
      }
      if (session->transcript_len > 0) {
        strcat(session->full_transcript, " ");
      }
      strcat(session->full_transcript, final_result.text);
      session->transcript_len = strlen(session->full_transcript);
    }
    ethervox_stt_result_free(&final_result);
  }

  ethervox_stt_stop(&session->stt_runtime);
  session->is_recording = false;

  // Append completion status to transcript file (content already written live during recording)
  if (session->transcript_len > 0 && session->last_transcript_file[0] != '\0') {
    FILE* f = fopen(session->last_transcript_file, "a");
    if (f) {
      time_t now = time(NULL);
      struct tm* tm_info = localtime(&now);
      char timestamp[64];
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

      fprintf(f, "\n\n========================================\n");
      fprintf(f, "Recording completed: %s\n", timestamp);
      fprintf(f, "Duration: %llu seconds\n",
              (unsigned long long)(time(NULL) - session->session_start_time));
      fprintf(f, "Total segments: %u\n", session->segment_count);
      fprintf(f, "Total characters: %zu\n", session->transcript_len);
      fflush(f);
      fclose(f);

      LOG_INFO("Saved transcript to: %s (%zu chars, %u segments)", session->last_transcript_file,
               session->transcript_len, session->segment_count);
      
      // Prompt user to assign speaker names if speakers were detected
      if (session->max_speaker_id >= 0) {
        LOG_INFO("Detected %d speaker(s) in conversation", session->max_speaker_id + 1);
        int naming_result = ethervox_voice_tools_assign_speaker_names(session);
        if (naming_result == 0) {
          LOG_INFO("Speaker names assigned and transcript updated");
          
          // Trigger LLM summarization
          printf("Summarizing...\n");
          fflush(stdout);
          
          // Set flag to trigger summarization in main loop
          session->needs_summarization = true;
        } else if (naming_result == 1) {
          LOG_INFO("User chose to keep anonymous speaker labels");
        }
      }

      // Also store in memory system
      if (session->memory_store) {
        ethervox_memory_store_t* mem = (ethervox_memory_store_t*)session->memory_store;
        const char* tags[] = {"voice", "transcript", "whisper"};
        uint64_t memory_id = 0;

        // Store with file path reference
        char memory_content[2048];
        snprintf(memory_content, sizeof(memory_content),
                 "Voice transcript saved to: %s\n\nTranscript:\n%s", session->last_transcript_file,
                 session->full_transcript);

        ethervox_memory_store_add(mem, memory_content, tags, 3, 0.9f, false, &memory_id);
        LOG_INFO("Stored in memory (ID: %llu) with file reference", (unsigned long long)memory_id);
      }
    } else {
      LOG_ERROR("Failed to open transcript file for final update: %s",
                session->last_transcript_file);
    }
  }

  // Ensure we always return a valid string (never NULL)
  if (session->full_transcript && session->full_transcript[0] != '\0') {
    *transcript_out = session->full_transcript;
  } else {
    // Return empty string if no transcript (silence or error)
    static const char* empty_transcript = "";
    *transcript_out = empty_transcript;
    LOG_WARN("No transcript generated (silence or error) - returning empty string");
  }

  LOG_INFO("Voice recording session stopped - %zu chars transcribed", session->transcript_len);

  return ETHERVOX_SUCCESS;
}

/**
 * Check if recording
 */
bool ethervox_voice_tools_is_recording(const ethervox_voice_session_t* session) {
  return session && session->is_recording;
}

/**
 * Cleanup
 */
void ethervox_voice_tools_cleanup(ethervox_voice_session_t* session) {
  if (!session)
    return;

  if (session->is_recording) {
    const char* transcript;
    ethervox_voice_tools_stop_listen(session, &transcript);
  }

  ethervox_stt_cleanup(&session->stt_runtime);

  if (session->audio_runtime.is_initialized) {
    ethervox_audio_cleanup(&session->audio_runtime);
  }

  if (session->full_transcript) {
    free(session->full_transcript);
    session->full_transcript = NULL;
  }
  
  // Cleanup speaker names
  if (session->speaker_names) {
    for (int i = 0; i < session->speaker_names_capacity; i++) {
      free(session->speaker_names[i]);
    }
    free(session->speaker_names);
    session->speaker_names = NULL;
    session->speaker_names_capacity = 0;
  }

  if (session->model_path) {
    free(session->model_path);
    session->model_path = NULL;
  }

  session->is_initialized = false;

  LOG_INFO("Voice tools cleaned up");
}

/**
 * Prompt user to assign names to speakers and update transcript file
 */
ethervox_result_t ethervox_voice_tools_assign_speaker_names(ethervox_voice_session_t* session) {
  if (!session || session->max_speaker_id < 0) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;  // No speakers detected
  }
  
  int num_speakers = session->max_speaker_id + 1;
  
  // First, extract example quotes from each speaker
  typedef struct {
    char* quotes[ETHERVOX_SPEAKER_EXAMPLE_QUOTES];
    int quote_count;
  } speaker_examples_t;
  
  speaker_examples_t* examples = (speaker_examples_t*)calloc(num_speakers, sizeof(speaker_examples_t));
  if (!examples) {
    LOG_ERROR("Failed to allocate speaker examples array");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Read transcript file to extract examples
  if (session->last_transcript_file[0] != '\0') {
    FILE* f = fopen(session->last_transcript_file, "r");
    if (f) {
      char line[1024];
      while (fgets(line, sizeof(line), f)) {
        // Look for speaker markers: [Speaker N] anywhere in the line
        // Format: [2025-12-04 08:45:30] (en) [Speaker 0] text here
        const char* speaker_marker = strstr(line, "[Speaker ");
        if (speaker_marker) {
          int speaker_id = -1;
          if (sscanf(speaker_marker, "[Speaker %d]", &speaker_id) == 1) {
            if (speaker_id >= 0 && speaker_id < num_speakers) {
              // Extract text after speaker marker
              const char* text_start = strchr(speaker_marker, ']');
              if (text_start) {
                text_start++;  // Skip ]
                while (*text_start == ' ') text_start++;  // Skip spaces
                
                // Store this quote if we have room and it's not empty
                if (examples[speaker_id].quote_count < ETHERVOX_SPEAKER_EXAMPLE_QUOTES && 
                    strlen(text_start) > 1) {  // More than just newline
                  // Trim newline
                  size_t len = strlen(text_start);
                  if (len > 0 && text_start[len - 1] == '\n') {
                    char* quote = (char*)malloc(len);
                    if (quote) {
                      strncpy(quote, text_start, len - 1);
                      quote[len - 1] = '\0';
                      examples[speaker_id].quotes[examples[speaker_id].quote_count++] = quote;
                    }
                  } else {
                    examples[speaker_id].quotes[examples[speaker_id].quote_count++] = strdup(text_start);
                  }
                }
              }
            }
          }
        }
      }
      fclose(f);
    }
  }
  
  // Prompt user with examples
  printf("\n");
  printf("========================================\n");
  printf("Speaker Identification\n");
  printf("========================================\n");
  printf("Detected %d speaker(s) in the conversation.\n\n", num_speakers);
  
  // Show examples for each speaker
  for (int i = 0; i < num_speakers; i++) {
    printf("Speaker %d examples:\n", i);
    if (examples[i].quote_count == 0) {
      printf("  (no examples found)\n");
    } else {
      for (int j = 0; j < examples[i].quote_count; j++) {
        printf("  %d. \"%s\"\n", j + 1, examples[i].quotes[j]);
      }
    }
    printf("\n");
  }
  
  // Loop until valid input (y/Y/n/N or empty for default No)
  bool valid_input = false;
  bool assign_names = false;
  
  while (!valid_input) {
    printf("Would you like to assign names to the speakers? [y/N]: ");
    fflush(stdout);
    
    char response[10];
    if (fgets(response, sizeof(response), stdin) == NULL) {
      // Cleanup examples
      for (int i = 0; i < num_speakers; i++) {
        for (int j = 0; j < examples[i].quote_count; j++) {
          free(examples[i].quotes[j]);
        }
      }
      free(examples);
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Trim newline and whitespace
    size_t len = strlen(response);
    while (len > 0 && (response[len - 1] == '\n' || response[len - 1] == ' ' || 
                       response[len - 1] == '\t')) {
      response[--len] = '\0';
    }
    
    // Skip leading whitespace
    char* input = response;
    while (*input == ' ' || *input == '\t') {
      input++;
    }
    
    // Validate input: empty (default No), y, Y, n, or N
    if (strlen(input) == 0 || input[0] == 'n' || input[0] == 'N') {
      valid_input = true;
      assign_names = false;
    } else if (input[0] == 'y' || input[0] == 'Y') {
      valid_input = true;
      assign_names = true;
    } else {
      printf("Invalid input. Please enter 'y' for yes or 'n' for no (or press Enter for no).\n\n");
    }
  }
  
  // Check if user declined
  if (!assign_names) {
    printf("Keeping anonymous speaker labels (Speaker 0, Speaker 1, etc.)\n");
    // Cleanup examples
    for (int i = 0; i < num_speakers; i++) {
      for (int j = 0; j < examples[i].quote_count; j++) {
        free(examples[i].quotes[j]);
      }
    }
    free(examples);
    return ETHERVOX_SUCCESS;  // User declined
  }
  
  // Cleanup examples now that we're done showing them
  for (int i = 0; i < num_speakers; i++) {
    for (int j = 0; j < examples[i].quote_count; j++) {
      free(examples[i].quotes[j]);
    }
  }
  free(examples);
  
  // Allocate speaker names array
  session->speaker_names_capacity = num_speakers;
  session->speaker_names = (char**)calloc(num_speakers, sizeof(char*));
  if (!session->speaker_names) {
    LOG_ERROR("Failed to allocate speaker names array");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Prompt for each speaker's name
  printf("\n");
  for (int i = 0; i < num_speakers; i++) {
    printf("Enter name for Speaker %d: ", i);
    fflush(stdout);
    
    char name_buffer[128];
    if (fgets(name_buffer, sizeof(name_buffer), stdin) == NULL) {
      // Failed to read - use anonymous
      session->speaker_names[i] = strdup("Anonymous");
      continue;
    }
    
    // Trim newline and whitespace
    size_t name_len = strlen(name_buffer);
    while (name_len > 0 && (name_buffer[name_len - 1] == '\n' || name_buffer[name_len - 1] == ' ' || 
                       name_buffer[name_len - 1] == '\t')) {
      name_buffer[--name_len] = '\0';
    }
    
    // Skip leading whitespace
    char* name_start = name_buffer;
    while (*name_start == ' ' || *name_start == '\t') {
      name_start++;
    }
    
    // Use provided name or "Anonymous" if empty
    if (strlen(name_start) > 0) {
      session->speaker_names[i] = strdup(name_start);
    } else {
      session->speaker_names[i] = strdup("Anonymous");
    }
  }
  
  printf("\n");
  printf("Speaker assignments:\n");
  for (int i = 0; i < num_speakers; i++) {
    printf("  Speaker %d → %s\n", i, session->speaker_names[i]);
  }
  printf("\n");
  
  // Update transcript file with named speakers
  if (session->last_transcript_file[0] == '\0') {
    LOG_WARN("No transcript file to update");
    return ETHERVOX_SUCCESS;  // Names saved but no file to update
  }
  
  // Read current transcript file
  FILE* f = fopen(session->last_transcript_file, "r");
  if (!f) {
    LOG_ERROR("Failed to open transcript file for reading: %s", session->last_transcript_file);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Read entire file into memory
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  char* file_content = (char*)malloc(file_size + 1);
  if (!file_content) {
    fclose(f);
    LOG_ERROR("Failed to allocate memory for file content");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  size_t bytes_read = fread(file_content, 1, file_size, f);
  file_content[bytes_read] = '\0';
  fclose(f);
  
  // Create updated content by replacing [Speaker N] with names
  char* updated_content = (char*)malloc(file_size * 2 + 1024);  // Extra space for longer names
  if (!updated_content) {
    free(file_content);
    LOG_ERROR("Failed to allocate memory for updated content");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  // Process content and replace speaker markers
  const char* read_ptr = file_content;
  char* write_ptr = updated_content;
  
  while (*read_ptr) {
    // Look for [Speaker N] pattern
    if (strncmp(read_ptr, "[Speaker ", 9) == 0) {
      int speaker_id = -1;
      int chars_read = 0;
      if (sscanf(read_ptr, "[Speaker %d]%n", &speaker_id, &chars_read) == 1) {
        // Found a speaker marker - replace with name
        if (speaker_id >= 0 && speaker_id < num_speakers && session->speaker_names[speaker_id]) {
          int name_len = sprintf(write_ptr, "[%s]", session->speaker_names[speaker_id]);
          write_ptr += name_len;
          read_ptr += chars_read;
          continue;
        }
      }
    }
    
    // Not a speaker marker or failed to parse - copy character as-is
    *write_ptr++ = *read_ptr++;
  }
  *write_ptr = '\0';
  
  // Write updated content back to file
  f = fopen(session->last_transcript_file, "w");
  if (!f) {
    free(file_content);
    free(updated_content);
    LOG_ERROR("Failed to open transcript file for writing: %s", session->last_transcript_file);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  fprintf(f, "%s", updated_content);
  fclose(f);
  
  // Rename file with speaker names if provided
  char new_filename[1024];
  bool should_rename = false;
  
  // Build new filename with first two speaker names (or first if only one)
  if (num_speakers >= 2 && session->speaker_names[0] && session->speaker_names[1]) {
    // Extract directory and timestamp from original filename
    const char* last_slash = strrchr(session->last_transcript_file, '/');
    size_t dir_len = last_slash ? (last_slash - session->last_transcript_file + 1) : 0;
    
    // Get timestamp from original filename (transcript_YYYYMMDD_HHMMSS.txt)
    const char* timestamp_start = strstr(session->last_transcript_file, "transcript_");
    char timestamp[32] = "";
    if (timestamp_start) {
      timestamp_start += 11; // Skip "transcript_"
      const char* timestamp_end = strchr(timestamp_start, '.');
      if (timestamp_end) {
        size_t ts_len = timestamp_end - timestamp_start;
        if (ts_len < sizeof(timestamp)) {
          strncpy(timestamp, timestamp_start, ts_len);
          timestamp[ts_len] = '\0';
        }
      }
    }
    
    if (dir_len > 0 && strlen(timestamp) > 0) {
      snprintf(new_filename, sizeof(new_filename), "%.*s%s_and_%s_%s.txt",
               (int)dir_len, session->last_transcript_file,
               session->speaker_names[0], session->speaker_names[1], timestamp);
      should_rename = true;
    }
  } else if (num_speakers == 1 && session->speaker_names[0]) {
    const char* last_slash = strrchr(session->last_transcript_file, '/');
    size_t dir_len = last_slash ? (last_slash - session->last_transcript_file + 1) : 0;
    
    const char* timestamp_start = strstr(session->last_transcript_file, "transcript_");
    char timestamp[32] = "";
    if (timestamp_start) {
      timestamp_start += 11;
      const char* timestamp_end = strchr(timestamp_start, '.');
      if (timestamp_end) {
        size_t ts_len = timestamp_end - timestamp_start;
        if (ts_len < sizeof(timestamp)) {
          strncpy(timestamp, timestamp_start, ts_len);
          timestamp[ts_len] = '\0';
        }
      }
    }
    
    if (dir_len > 0 && strlen(timestamp) > 0) {
      snprintf(new_filename, sizeof(new_filename), "%.*s%s_%s.txt",
               (int)dir_len, session->last_transcript_file,
               session->speaker_names[0], timestamp);
      should_rename = true;
    }
  }
  
  // Attempt to rename the file
  if (should_rename) {
    if (rename(session->last_transcript_file, new_filename) == 0) {
      LOG_INFO("Renamed transcript file to: %s", new_filename);
      printf("\n📝 Renamed transcript file:\n");
      printf("  Old: %s\n", session->last_transcript_file);
      printf("  New: %s\n\n", new_filename);
      
      // Update session with new filename
      strncpy(session->last_transcript_file, new_filename, sizeof(session->last_transcript_file) - 1);
      session->last_transcript_file[sizeof(session->last_transcript_file) - 1] = '\0';
    } else {
      LOG_WARN("Failed to rename transcript file");
    }
  }
  
  // Display the updated transcript content
  printf("\n========================================\n");
  printf("📄 Updated Transcript\n");
  printf("========================================\n\n");
  printf("%s\n", updated_content);
  printf("========================================\n\n");
  
  free(file_content);
  free(updated_content);
  
  LOG_INFO("[OK] Updated transcript file with speaker names: %s", session->last_transcript_file);
  printf("[OK] Transcript updated with speaker names\n");
  printf("  File: %s\n\n", session->last_transcript_file);
  
  return ETHERVOX_SUCCESS;
}

/**
 * Tool wrapper: listen_and_summarize
 *
 * This tool allows the LLM to start/stop voice recording with Whisper transcription.
 * The LLM must call with action="start" then action="stop" to get the transcript.
 */
static int tool_listen_and_summarize_wrapper(const char* args_json, char** result, char** error) {
  ethervox_voice_session_t* session = g_voice_session;

  if (!session || !session->is_initialized) {
    *error = strdup("Voice tools not initialized");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Parse action parameter
  char action[32] = {0};
  const char* action_start = strstr(args_json, "\"action\":");
  if (action_start) {
    action_start = strchr(action_start, ':') + 1;
    while (*action_start == ' ' || *action_start == '"')
      action_start++;
    const char* action_end = strchr(action_start, '"');
    if (action_end) {
      size_t len = action_end - action_start;
      if (len < sizeof(action)) {
        strncpy(action, action_start, len);
        action[len] = '\0';
      }
    }
  }

  if (strcmp(action, "start") == 0) {
    // Start recording
    if (ethervox_voice_tools_start_listen(session) != 0) {
      *error = strdup("Failed to start listening");
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    *result = strdup(
        "{\"status\":\"recording\",\"message\":\"Voice recording started. Use /end command or call "
        "with action='stop' to finish.\"}");
    return ETHERVOX_SUCCESS;

  } else if (strcmp(action, "stop") == 0) {
    // Stop recording and get transcript
    const char* transcript;
    if (ethervox_voice_tools_stop_listen(session, &transcript) != 0) {
      *error = strdup("Failed to stop listening or not recording");
      return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Build JSON response with transcript
    size_t res_len = strlen(transcript) + 512;
    char* res = malloc(res_len);
    snprintf(res, res_len,
             "{\"status\":\"completed\",\"transcript\":\"%s\",\"length\":%zu,\"segments\":%u}",
             transcript, session->transcript_len, session->segment_count);

    *result = res;

    // Store transcript in memory
    if (session->memory_store && strlen(transcript) > 0) {
      const char* tags[] = {"voice", "transcript", "whisper"};
      uint64_t memory_id = 0;

      // Cast to memory store and add
      ethervox_memory_store_t* mem = (ethervox_memory_store_t*)session->memory_store;
      ethervox_memory_store_add(mem, transcript, tags, 3, 0.8f, false, &memory_id);

      LOG_INFO("Stored voice transcript in memory (ID: %llu)", (unsigned long long)memory_id);
    }

    return ETHERVOX_SUCCESS;

  } else if (strcmp(action, "status") == 0) {
    // Check recording status
    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg),
             "{\"is_recording\":%s,\"transcript_length\":%zu,\"segments\":%u}",
             session->is_recording ? "true" : "false", session->transcript_len,
             session->segment_count);
    *result = strdup(status_msg);
    return ETHERVOX_SUCCESS;

  } else {
    *error = strdup("Invalid action. Use 'start', 'stop', or 'status'");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
}

/**
 * Tool definition for listen_and_summarize
 */
static ethervox_tool_t listen_tool = {
    .name = "listen_and_summarize",
    .description =
        "Start or stop voice recording with Whisper STT transcription and speaker detection. "
        "Call with {\"action\":\"start\"} to begin recording (user will use /stoptranscribe "
        "command to end), "
        "or {\"action\":\"stop\"} to get the final transcript with speaker labels. "
        "Transcript will be automatically stored in memory.",
    .parameters_json_schema =
        "{\"type\":\"object\","
        "\"properties\":{"
        "\"action\":{\"type\":\"string\",\"enum\":[\"start\",\"stop\",\"status\"],"
        "\"description\":\"Action to perform: start recording, stop and get transcript, or check "
        "status\"}"
        "},"
        "\"required\":[\"action\"]}",
    .execute = tool_listen_and_summarize_wrapper,
    .is_deterministic = false,
    .requires_confirmation = false,
    .is_stateful = true,
    .estimated_latency_ms = 100.0f};

/**
 * Register base voice tools with Governor
 * 
 * Base tools: listen_and_summarize (always available)
 */
ethervox_result_t ethervox_voice_tools_register(void* registry, ethervox_voice_session_t* session) {
  if (!registry || !session) {
    LOG_ERROR("Invalid parameters for voice tools registration");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  g_voice_session = session;

  ethervox_tool_registry_t* reg = (ethervox_tool_registry_t*)registry;

  if (ethervox_tool_registry_add(reg, &listen_tool) != 0) {
    LOG_ERROR("Failed to register listen_and_summarize tool");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  LOG_INFO("Registered listen_and_summarize tool with Governor (Whisper STT)");

  return ETHERVOX_SUCCESS;
}

/**
 * Register advanced voice training tools with Governor
 * 
 * Training tools: pronunciation training, voice training mode
 * These are optional/advanced features separate from base voice tools
 * 
 * @param registry Governor tool registry
 * @param session Voice session state  
 * @param phonemizer_ctx Phonemizer context for training
 * @param tts_ctx TTS context for synthesis
 * @param stt_ctx STT context for transcription
 * @return 0 on success, -1 on error
 */
ethervox_result_t ethervox_voice_tools_register_training(
    void* registry, 
    ethervox_voice_session_t* session,
    void* phonemizer_ctx,
    void* tts_ctx,
    void* stt_ctx
) {
  if (!registry || !session) {
    LOG_ERROR("Invalid parameters for training tools registration");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Training tools are registered in conversation_tools plugin
  // This function exists for future training-specific tools
  
  LOG_INFO("Training tools registration placeholder (tools in conversation_tools)");
  
  return ETHERVOX_SUCCESS;
}
