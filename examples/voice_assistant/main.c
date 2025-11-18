/**
 * @file main.c
 * @brief Voice Assistant Demo - Wake Word + STT + Dialogue + LLM
 *
 * Demonstrates the complete voice pipeline:
 * 1. Audio Capture (from microphone)
 * 2. Wake Word Detection ("hey ethervox")
 * 3. Speech-to-Text (local Vosk/Whisper)
 * 4. Intent Parsing (existing dialogue_core)
 * 5. LLM Response (local model)
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#if !defined(strcasecmp)
#define strcasecmp _stricmp
#endif
#else
#include <strings.h>
#include <unistd.h>
#endif
static void voice_assistant_sleep_us(unsigned int microseconds) {
#if defined(_WIN32)
  /* Convert microseconds to milliseconds, ensure minimum Sleep interval */
  DWORD millis = (DWORD)((microseconds + 999U) / 1000U);
  if (millis == 0U) {
    millis = 1U;
  }
  Sleep(millis);
#else
  usleep(microseconds);
#endif
}

#include "ethervox/audio.h"
#include "ethervox/dialogue.h"
#include "ethervox/llm.h"
#include "ethervox/model_manager.h"
#include "ethervox/platform.h"
#include "ethervox/stt.h"
#include "ethervox/wake_word.h"

// Pipeline state
typedef enum {
  STATE_LISTENING_FOR_WAKE,
  STATE_RECORDING_SPEECH,
  STATE_PROCESSING_INTENT,
  STATE_GENERATING_RESPONSE
} pipeline_state_t;

typedef struct {
  bool text_mode;

  // Platform
  ethervox_platform_t platform;

  // Audio
  ethervox_audio_runtime_t audio;
  ethervox_audio_config_t audio_config;

  // Wake word
  ethervox_wake_runtime_t wake;
  ethervox_wake_config_t wake_config;

  // STT
  ethervox_stt_runtime_t stt;
  ethervox_stt_config_t stt_config;

  // Dialogue
  ethervox_dialogue_engine_t dialogue;
  ethervox_llm_config_t llm_config;

  // LLM Backend
  ethervox_llm_backend_t* llm_backend;
  bool llm_enabled;
  char* model_path;
  
  // Model Manager
  ethervox_model_manager_t* model_manager;
  bool auto_download_models;

  // State
  pipeline_state_t state;
  char* context_id;
  bool running;
  bool audio_ready;
  bool wake_ready;
  bool stt_ready;
  char language_code[8];
  char stt_language[16];
} voice_pipeline_t;

static voice_pipeline_t g_pipeline = {0};

// Signal handler for clean shutdown
void signal_handler(int signum) {
  (void)signum;
  printf("\nShutting down voice assistant...\n");
  g_pipeline.running = false;
}

static void sanitize_language(const char* source, char* target, size_t target_len) {
  if (!target || target_len == 0) {
    return;
  }

  size_t out_idx = 0;
  for (const char* cursor = source; cursor && *cursor && out_idx < target_len - 1; ++cursor) {
    if (*cursor == '.' || *cursor == '@') {
      break;
    }
    if (*cursor == '_' || *cursor == '-') {
      target[out_idx++] = '-';
      continue;
    }
    if (isalpha((unsigned char)*cursor)) {
      target[out_idx++] = (char)tolower((unsigned char)*cursor);
    }
  }
  target[out_idx] = '\0';

  if (out_idx < 2) {
    /* Use snprintf for bounded copy and guaranteed null-termination */
    snprintf(target, target_len, "%s", "en");
    return;
  }

  target[2] = '\0';
}

static void map_stt_language(const char* base_language, char* target, size_t target_len) {
  if (!base_language || !*base_language) {
    snprintf(target, target_len, "%s", "en-US");
    return;
  }

  if (strncmp(base_language, "es", 2) == 0) {
    snprintf(target, target_len, "%s", "es-ES");
  } else if (strncmp(base_language, "zh", 2) == 0) {
    snprintf(target, target_len, "%s", "zh-CN");
  } else if (strncmp(base_language, "fr", 2) == 0) {
    snprintf(target, target_len, "%s", "fr-FR");
  } else if (strncmp(base_language, "de", 2) == 0) {
    snprintf(target, target_len, "%s", "de-DE");
  } else {
    snprintf(target, target_len, "%s", "en-US");
  }
}

static void pipeline_resolve_language(voice_pipeline_t* pipeline, const char* override_language) {
  const char* resolved = NULL;

  if (override_language && *override_language) {
    resolved = override_language;
  } else {
    const char* env_lang = getenv("ETHERVOX_LANG");
    if (env_lang && *env_lang) {
      resolved = env_lang;
    }
  }

  if (!resolved || !*resolved) {
    resolved = ethervox_dialogue_detect_system_language();
  }

  sanitize_language(resolved, pipeline->language_code, sizeof(pipeline->language_code));
  if (pipeline->language_code[0] == '\0') {
    snprintf(pipeline->language_code, sizeof(pipeline->language_code), "%s", "en");
  }

  map_stt_language(pipeline->language_code, pipeline->stt_language, sizeof(pipeline->stt_language));
}

// Initialize pipeline
int pipeline_init(voice_pipeline_t* pipeline, const char* language_override, bool enable_audio, const char* model_path) {
  printf("=== EthervoxAI Voice Assistant ===\n\n");

  // Initialize platform
  if (ethervox_platform_init(&pipeline->platform) != 0) {
    fprintf(stderr, "Failed to initialize platform\n");
    return -1;
  }
  printf("Platform: %s\n\n", ethervox_platform_get_name());

  pipeline->text_mode = !enable_audio;

  pipeline_resolve_language(pipeline, language_override);
  printf("Language preference: %s (STT: %s)\n\n", pipeline->language_code, pipeline->stt_language);

  if (enable_audio) {
    bool audio_pipeline_ready = true;

    pipeline->audio_config = (ethervox_audio_config_t){
        .sample_rate = 16000, .channels = 1, .bits_per_sample = 16, .buffer_size = 1024};

    if (ethervox_audio_init(&pipeline->audio, &pipeline->audio_config) != 0) {
      fprintf(stderr, "Failed to initialize audio\n");
      audio_pipeline_ready = false;
    } else {
      pipeline->audio_ready = true;
      printf("✓ Audio initialized (16kHz, mono)\n");

      pipeline->wake_config = ethervox_wake_get_default_config();
      pipeline->wake_config.wake_word = "hey ethervox";
      pipeline->wake_config.sensitivity = 0.7f;

      if (ethervox_wake_init(&pipeline->wake, &pipeline->wake_config) != 0) {
        fprintf(stderr, "Failed to initialize wake word detection\n");
        audio_pipeline_ready = false;
      } else {
        pipeline->wake_ready = true;
        printf("✓ Wake word: '%s' (sensitivity: %.1f)\n", pipeline->wake_config.wake_word,
               pipeline->wake_config.sensitivity);

        pipeline->stt_config = ethervox_stt_get_default_config();
        pipeline->stt_config.language = pipeline->stt_language;

        if (ethervox_stt_init(&pipeline->stt, &pipeline->stt_config) != 0) {
          fprintf(stderr, "Failed to initialize STT\n");
          audio_pipeline_ready = false;
        } else {
          pipeline->stt_ready = true;
          printf("✓ STT initialized (%s)\n", pipeline->stt_config.language);
          printf(
              "Tip: speak '%s' clearly near the microphone. Use --text if audio isn't "
              "available.\n\n",
              pipeline->wake_config.wake_word);
        }
      }
    }

    if (!audio_pipeline_ready) {
      if (pipeline->stt_ready) {
        ethervox_stt_cleanup(&pipeline->stt);
        pipeline->stt_ready = false;
      }
      if (pipeline->wake_ready) {
        ethervox_wake_cleanup(&pipeline->wake);
        pipeline->wake_ready = false;
      }
      if (pipeline->audio_ready) {
        ethervox_audio_cleanup(&pipeline->audio);
        pipeline->audio_ready = false;
      }

      pipeline->text_mode = true;
      printf(
          "⚠️  Audio capture unavailable; switching to text interaction mode. Set "
          "ETHERVOX_ALSA_DEVICE or launch with --text to skip audio.\n\n");
    }
  } else {
    printf("(Audio disabled — running in text interaction mode)\n");
  }

  // Initialize LLM backend (optional)
  pipeline->llm_enabled = false;
  pipeline->llm_backend = NULL;
  pipeline->model_path = NULL;
  pipeline->model_manager = NULL;
  pipeline->auto_download_models = true;

  if (model_path && *model_path) {
    printf("Initializing LLM backend...\n");
    
    // Create LLM backend
    pipeline->llm_backend = ethervox_llm_create_llama_backend();
    if (!pipeline->llm_backend) {
      fprintf(stderr, "⚠️  Failed to create LLM backend (continuing without local LLM)\n");
    } else {
      // Configure LLM
      pipeline->llm_config = ethervox_dialogue_get_default_llm_config();
      pipeline->llm_config.context_length = 2048;
      pipeline->llm_config.max_tokens = 256;
      pipeline->llm_config.temperature = 0.7f;
      pipeline->llm_config.top_p = 0.9f;
      pipeline->llm_config.use_gpu = false;  // Can be made configurable
      pipeline->llm_config.language_code = pipeline->language_code;

      // Initialize backend
      if (ethervox_llm_backend_init(pipeline->llm_backend, &pipeline->llm_config) != 0) {
        fprintf(stderr, "⚠️  Failed to initialize LLM backend (continuing without local LLM)\n");
        ethervox_llm_backend_free(pipeline->llm_backend);
        pipeline->llm_backend = NULL;
      } else {
        // Check if model_path is a preset name or actual path
        const char* actual_model_path = model_path;
        const ethervox_model_info_t* selected_model = NULL;
        
        // Check if user specified a preset model name
        if (strcasecmp(model_path, "tinyllama") == 0) {
          selected_model = &ETHERVOX_MODEL_TINYLLAMA_1B_Q4;
        } else if (strcasecmp(model_path, "phi2") == 0) {
          selected_model = &ETHERVOX_MODEL_PHI2_Q4;
        } else if (strcasecmp(model_path, "mistral") == 0) {
          selected_model = &ETHERVOX_MODEL_MISTRAL_7B_Q4;
        } else if (strcasecmp(model_path, "llama2") == 0) {
          selected_model = &ETHERVOX_MODEL_LLAMA2_7B_Q4;
        }
        
        // If preset model selected, use model manager
        if (selected_model && pipeline->auto_download_models) {
          printf("Using preset model: %s\n", selected_model->name);
          
          // Create model manager if not exists
          if (!pipeline->model_manager) {
            ethervox_model_manager_config_t mm_config = ethervox_model_manager_get_default_config();
            mm_config.auto_download = true;
            pipeline->model_manager = ethervox_model_manager_create(&mm_config);
          }
          
          if (pipeline->model_manager) {
            // Check if model is available, download if needed
            printf("Checking model availability...\n");
            if (!ethervox_model_manager_is_available(pipeline->model_manager, selected_model)) {
              printf("Model not found locally, downloading...\n");
              printf("Size: %.2f MB\n", (float)selected_model->size_bytes / 1024.0f / 1024.0f);
              printf("This may take several minutes depending on your connection.\n");
              
              int dl_result = ethervox_model_manager_ensure_available(pipeline->model_manager, selected_model);
              if (dl_result != 0) {
                fprintf(stderr, "⚠️  Failed to download model\n");
                fprintf(stderr, "Please download manually from:\n%s\n", selected_model->url);
                ethervox_llm_backend_cleanup(pipeline->llm_backend);
                ethervox_llm_backend_free(pipeline->llm_backend);
                pipeline->llm_backend = NULL;
                goto skip_llm_load;
              }
              printf("✓ Model downloaded successfully\n");
            } else {
              printf("✓ Model already available locally\n");
            }
            
            // Get model path from manager
            actual_model_path = ethervox_model_manager_get_path(pipeline->model_manager, selected_model);
          }
        }
        
        // Load model
        printf("Loading model: %s\n", actual_model_path);
        if (ethervox_llm_backend_load_model(pipeline->llm_backend, actual_model_path) != 0) {
          fprintf(stderr, "⚠️  Failed to load model (continuing without local LLM)\n");
          ethervox_llm_backend_cleanup(pipeline->llm_backend);
          ethervox_llm_backend_free(pipeline->llm_backend);
          pipeline->llm_backend = NULL;
        } else {
          pipeline->llm_enabled = true;
          pipeline->model_path = strdup(actual_model_path);
          printf("✓ LLM backend initialized with model: %s\n", actual_model_path);
          
          // Display capabilities
          ethervox_llm_capabilities_t caps;
          if (pipeline->llm_backend->get_capabilities(pipeline->llm_backend, &caps) == 0) {
            printf("  • Model format: %s\n", caps.model_format);
            printf("  • Max context: %u tokens\n", caps.max_context_length);
            printf("  • GPU support: %s\n", caps.supports_gpu ? "yes" : "no");
          }
        }
skip_llm_load:
        (void)0;  // Label target
      }
    }
  } else {
    printf("(No LLM model specified - using simple response mode)\n");
  }

  // Initialize dialogue engine
  pipeline->llm_config = ethervox_dialogue_get_default_llm_config();
  pipeline->llm_config.language_code = pipeline->language_code;
  
  if (ethervox_dialogue_init(&pipeline->dialogue, &pipeline->llm_config) != 0) {
    fprintf(stderr, "Failed to initialize dialogue engine\n");
    return -1;
  }
  ethervox_dialogue_set_language(&pipeline->dialogue, pipeline->language_code);
  printf("✓ Dialogue engine initialized\n");

  // Create dialogue context
  ethervox_dialogue_context_request_t context_request = {
      .user_id = "demo_user",
      .language_code = pipeline->language_code};
  if (ethervox_dialogue_create_context(&pipeline->dialogue, &context_request,
                                       &pipeline->context_id) != 0) {
    fprintf(stderr, "Failed to create dialogue context\n");
    return -1;
  }
  printf("✓ Dialogue context: %s\n\n", pipeline->context_id);

  pipeline->state = STATE_LISTENING_FOR_WAKE;
  pipeline->running = true;

  return 0;
}

// Cleanup pipeline
void pipeline_cleanup(voice_pipeline_t* pipeline) {
  // Cleanup LLM backend
  if (pipeline->llm_backend) {
    ethervox_llm_backend_unload_model(pipeline->llm_backend);
    ethervox_llm_backend_cleanup(pipeline->llm_backend);
    ethervox_llm_backend_free(pipeline->llm_backend);
    pipeline->llm_backend = NULL;
  }

  if (pipeline->model_path) {
    free(pipeline->model_path);
    pipeline->model_path = NULL;
  }
  
  if (pipeline->model_manager) {
    ethervox_model_manager_destroy(pipeline->model_manager);
    pipeline->model_manager = NULL;
  }
  

  ethervox_dialogue_cleanup(&pipeline->dialogue);

  if (pipeline->stt_ready) {
    ethervox_stt_cleanup(&pipeline->stt);
  }

  if (pipeline->wake_ready) {
    ethervox_wake_cleanup(&pipeline->wake);
  }

  if (pipeline->audio_ready) {
    ethervox_audio_cleanup(&pipeline->audio);
  }

  ethervox_platform_cleanup(&pipeline->platform);

  if (pipeline->context_id) {
    free(pipeline->context_id);
  }

  printf("Pipeline cleaned up\n");
}

static void pipeline_run_text(voice_pipeline_t* pipeline);

// Main processing loop
static void pipeline_run_voice(voice_pipeline_t* pipeline) {
  printf("🎤 Say '%s' to begin. Press Ctrl+C to exit.\n\n", pipeline->wake_config.wake_word);

  if (ethervox_audio_start_capture(&pipeline->audio) != 0) {
    fprintf(stderr, "Failed to start audio capture\n");
    if (pipeline->stt_ready) {
      ethervox_stt_cleanup(&pipeline->stt);
      pipeline->stt_ready = false;
    }
    if (pipeline->wake_ready) {
      ethervox_wake_cleanup(&pipeline->wake);
      pipeline->wake_ready = false;
    }
    if (pipeline->audio_ready) {
      ethervox_audio_cleanup(&pipeline->audio);
      pipeline->audio_ready = false;
    }

    printf(
        "⚠️  Switching to text interaction mode. Configure ETHERVOX_ALSA_DEVICE to choose an "
        "input device.\n\n");
    pipeline->text_mode = true;
    pipeline_run_text(pipeline);
    return;
  }

  bool conversation_active = false;
  bool stt_session_active = false;

  while (pipeline->running) {
    ethervox_audio_buffer_t audio_buffer = {0};
    if (ethervox_audio_read(&pipeline->audio, &audio_buffer) != 0) {
      voice_assistant_sleep_us(10000);
      continue;
    }

    if (!conversation_active) {
      ethervox_wake_result_t wake_result = {0};
      if (ethervox_wake_process(&pipeline->wake, &audio_buffer, &wake_result) == 0 &&
          wake_result.detected) {
        printf("\n🔔 Wake word detected! Listening for speech...\n");
        ethervox_wake_reset(&pipeline->wake);
        if (!stt_session_active) {
          ethervox_stt_start(&pipeline->stt);
          stt_session_active = true;
        }
        conversation_active = true;
        printf("🗣️  Speak now (say 'stop' to end).\n");
      }
    } else {
      if (!stt_session_active) {
        ethervox_stt_start(&pipeline->stt);
        stt_session_active = true;
      }

      ethervox_stt_result_t stt_result = {0};
      int stt_status = ethervox_stt_process(&pipeline->stt, &audio_buffer, &stt_result);

      if (stt_status == 0 && (stt_result.is_final || stt_result.is_partial)) {
        const char* transcript = stt_result.text ? stt_result.text : "";
        printf("\n📝 Heard: \"%s\"\n", transcript);

        bool should_stop = (strcasecmp(transcript, "stop") == 0);

        char response_text[512];
        
        // Generate response using LLM if available
        if (pipeline->llm_enabled && pipeline->llm_backend) {
          printf("🤖 Generating LLM response...\n");
          
          ethervox_llm_response_t llm_response = {0};
          if (ethervox_llm_backend_generate(pipeline->llm_backend, transcript, 
                                           pipeline->language_code, &llm_response) == 0) {
            snprintf(response_text, sizeof(response_text), "%s", llm_response.text);
            printf("💬 LLM response (%u tokens, %ums): %s\n", 
                   llm_response.token_count, llm_response.processing_time_ms, response_text);
            ethervox_llm_response_free(&llm_response);
          } else {
            printf("⚠️  LLM generation failed, using simple response\n");
            snprintf(response_text, sizeof(response_text), "I heard you say: %s", transcript);
          }
        } else {
          snprintf(response_text, sizeof(response_text), "I heard you say: %s", transcript);
        }

        ethervox_tts_request_t tts_request = {.text = response_text,
                                              .language_code = pipeline->language_code,
                                              .speech_rate = 1.0f,
                                              .pitch = 0.0f,
                                              .voice_id = "default"};

        ethervox_audio_buffer_t tts_output = {0};
        if (ethervox_tts_synthesize(&pipeline->audio, &tts_request, &tts_output) == 0) {
          printf("🔊 TTS ready (%u samples)\n", tts_output.size);
          ethervox_audio_buffer_free(&tts_output);
        } else {
          printf("⚠️  TTS synthesis failed\n");
        }

        ethervox_stt_result_free(&stt_result);

        if (should_stop) {
          printf("🛑 Stop command received. Exiting loop.\n");
          pipeline->running = false;
          ethervox_audio_buffer_free(&audio_buffer);
          break;
        }

        if (stt_session_active) {
          ethervox_stt_stop(&pipeline->stt);
          stt_session_active = false;
        }

        ethervox_stt_start(&pipeline->stt);
        stt_session_active = true;
        printf("🗣️  Ready for your next phrase.\n");
      }
    }

    ethervox_audio_buffer_free(&audio_buffer);
  }

  if (stt_session_active) {
    ethervox_stt_stop(&pipeline->stt);
  }

  ethervox_audio_stop_capture(&pipeline->audio);
}

static void trim_newline(char* str) {
  if (!str) {
    return;
  }

  size_t len = strlen(str);
  while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
    str[--len] = '\0';
  }
}

static bool is_exit_phrase(const char* text) {
  if (!text) {
    return true;
  }

  return strcasecmp(text, "exit") == 0 || strcasecmp(text, "quit") == 0 ||
         strcasecmp(text, "stop") == 0;
}

static void pipeline_run_text(voice_pipeline_t* pipeline) {
  printf("💬 Text interaction mode enabled. Type 'exit' to quit.\n");

  char input[512];

  while (pipeline->running) {
    printf("You> ");
    fflush(stdout);

    if (!fgets(input, sizeof(input), stdin)) {
      printf("\nInput stream closed — exiting.\n");
      break;
    }

    trim_newline(input);

    if (input[0] == '\0') {
      continue;
    }

    if (is_exit_phrase(input)) {
      printf("🛑 Stop command received.\n");
      break;
    }

    ethervox_intent_t intent = {0};
  ethervox_dialogue_intent_request_t intent_request = {
    .text = input,
    .language_code = pipeline->language_code};
  if (ethervox_dialogue_parse_intent(&pipeline->dialogue, &intent_request, &intent) != 0) {
      printf("⚠️  Couldn't parse intent. Try again.\n");
      continue;
    }

    // Generate response using LLM if available
    if (pipeline->llm_enabled && pipeline->llm_backend) {
      ethervox_llm_response_t llm_response = {0};
      if (ethervox_llm_backend_generate(pipeline->llm_backend, input, 
                                       pipeline->language_code, &llm_response) == 0) {
        printf("Assistant> %s\n", llm_response.text);
        printf("  [%u tokens, %ums, confidence: %.2f]\n", 
               llm_response.token_count, llm_response.processing_time_ms, llm_response.confidence);
        ethervox_llm_response_free(&llm_response);
      } else {
        printf("⚠️  LLM generation failed\n");
      }
    } else {
      // Fallback to dialogue engine
      ethervox_llm_response_t response = {0};
      if (ethervox_dialogue_process_llm(&pipeline->dialogue, &intent, pipeline->context_id,
                                        &response) != 0) {
        printf("⚠️  Dialogue engine couldn't produce a response.\n");
        ethervox_intent_free(&intent);
        continue;
      }

      const char* assistant_text = response.text ? response.text : "(no response)";
      printf("Assistant> %s\n", assistant_text);
      ethervox_llm_response_free(&response);
    }
    
    ethervox_intent_free(&intent);
  }
}

void pipeline_run(voice_pipeline_t* pipeline) {
  if (pipeline->text_mode) {
    pipeline_run_text(pipeline);
    return;
  }

  pipeline_run_voice(pipeline);
}

int main(int argc, char** argv) {
  const char* cli_language = NULL;
  const char* model_path = NULL;
  bool text_mode = false;
  
  int i = 1;
  while (i < argc) {
    if (strncmp(argv[i], "--lang=", 7) == 0) {
      cli_language = argv[i] + 7;
      i++;
    } else if ((strcmp(argv[i], "--lang") == 0 || strcmp(argv[i], "-l") == 0) && i + 1 < argc) {
      cli_language = argv[i + 1];
      i += 2;
    } else if (strncmp(argv[i], "--model=", 8) == 0) {
      model_path = argv[i] + 8;
      i++;
    } else if ((strcmp(argv[i], "--model") == 0 || strcmp(argv[i], "-m") == 0) && i + 1 < argc) {
      model_path = argv[i + 1];
      i += 2;
    } else if (strcmp(argv[i], "--text") == 0 || strcmp(argv[i], "--cli") == 0) {
      text_mode = true;
      i++;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("Usage: %s [options]\n\n", argv[0]);
      printf("Options:\n");
      printf("  --lang=LANG, -l LANG    Set language (e.g., en, es, zh)\n");
      printf("  --model=NAME, -m NAME   LLM model (preset or path)\n");
      printf("                          Presets: tinyllama, phi2, mistral, llama2\n");
      printf("                          Or provide path to local GGUF file\n");
      printf("  --text, --cli           Use text mode instead of voice\n");
    } else {
      i++;
      printf("  --help, -h              Show this help message\n\n");
      printf("Examples:\n");
      printf("  %s --text\n", argv[0]);
      printf("  %s --model=tinyllama\n", argv[0]);
      printf("  %s --model=/path/to/model.gguf\n", argv[0]);
      printf("  %s --text --model=phi2 --lang=es\n", argv[0]);
      printf("\nPreset models are auto-downloaded to ~/.cache/ethervox/models/\n");
      return 0;
    }
  }

  // Set up signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Initialize pipeline
  if (pipeline_init(&g_pipeline, cli_language, !text_mode, model_path) != 0) {
    fprintf(stderr, "Failed to initialize pipeline\n");
    return 1;
  }

  // Run main loop
  pipeline_run(&g_pipeline);

  // Cleanup
  pipeline_cleanup(&g_pipeline);

  printf("\nGoodbye!\n");
  return 0;
}