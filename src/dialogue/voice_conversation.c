/**
 * @file voice_conversation.c
 * @brief Real-time voice conversation implementation
 *
 * Manages background thread for wake word → Vosk STT → Governor → Piper TTS
 * conversation flow. Separate from transcription pipeline.
 */

#include "ethervox/conversation.h"
#include "ethervox/conversation_tools.h"
#include "ethervox/logging.h"
#include "ethervox/language_detector.h"
#include "ethervox/dialogue.h"
#include "ethervox/error.h"
#include "ethervox/governor.h"
#include "ethervox/stt.h"
#include "ethervox/audio.h"
#include "ethervox/tts.h"
#include "ethervox/aec.h"
#include "ethervox/settings.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

// External reference to global TTS context (initialized at app startup in main.c)
extern ethervox_tts_context_t* g_global_tts;
extern pthread_mutex_t g_tts_mutex;

// Forward declaration for macOS audio state (platform-specific)
#ifdef __APPLE__
typedef struct {
    void* capture_queue;
    void* capture_buffers[3];
    bool is_recording;
    void* playback_queue;
    void* playback_buffers[3];
    bool is_playing;
    int16_t* ring_buffer;
    size_t ring_buffer_size;
    size_t write_pos;
    size_t read_pos;
    pthread_mutex_t lock;
    int16_t* playback_ring_buffer;
    size_t playback_ring_buffer_size;
    size_t playback_write_pos;
    size_t playback_read_pos;
    pthread_mutex_t playback_lock;
    uint32_t sample_rate;
    uint8_t channels;
} macos_audio_state_t;
#endif

/**
 * @brief Internal conversation session structure
 */
struct ethervox_conversation_session {
    ethervox_conversation_config_t config;
    ethervox_governor_t* governor;
    
    // Thread management
    pthread_t thread_id;
    pthread_mutex_t mutex;
    pthread_cond_t trigger_cond;
    bool thread_running;
    bool thread_should_exit;
    
    // State tracking
    ethervox_conversation_state_t state;
    uint64_t conversation_start_time_ms;
    uint64_t last_audio_time_ms;
    
    // Vosk STT runtime
    ethervox_stt_runtime_t stt_runtime;
    bool stt_initialized;
    
    // Audio runtime for microphone capture
    ethervox_audio_runtime_t audio_runtime;
    bool audio_initialized;
    
    // TTS runtime (Piper neural TTS)
    ethervox_tts_context_t* tts_context;
    bool tts_initialized;
    
    // AEC runtime (echo cancellation)
    ethervox_aec_t* aec_context;
    bool aec_initialized;
    
    // Audio capture
    ethervox_audio_buffer_t* audio_buffer;
    bool audio_capture_active;
    
    // Always-listening mode (desktop only)
    bool always_listening;
    char* pending_transcription;  // Buffer for continuous transcription
    
    // Language detection for multilingual TTS
    char last_detected_language[8];  // Last detected language code ("en", "de", "zh", "es")
};

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// ============================================================================
// Conversation Tool Callbacks
// ============================================================================

/**
 * @brief Callback for speak tool - handles TTS synthesis and playback
 */
static int conversation_on_speak(const char* text, const char* language,
                                  bool wait_for_response, bool allow_interrupt, 
                                  void* user_data) {
    ethervox_conversation_session_t* session = (ethervox_conversation_session_t*)user_data;
    if (!session || !text) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOG_INFO("[Speak Tool] Synthesizing: %s (language=%s, wait=%d, interrupt=%d)",
                      text, language ? language : "auto", wait_for_response, allow_interrupt);
    
    pthread_mutex_lock(&session->mutex);
    session->state = ETHERVOX_CONV_STATE_SPEAKING;
    pthread_mutex_unlock(&session->mutex);
    
    // Use explicit language if provided, otherwise auto-detect from text
    const char* target_language = language;
    if (!target_language) {
        // Auto-detect from assistant's text when language not specified
        target_language = ethervox_detect_and_switch_voice(
            text,
            NULL,  // Force detection from text, don't inherit user's STT language
            (void**)&session->tts_context
        );
    } else {
        // Explicit language specified - switch voice directly
        target_language = ethervox_switch_to_language(target_language, (void**)&session->tts_context);
    }
    
    // Print to console
    printf("\n========================================================\n");
    printf("🤖 Assistant [%s]: %s\n", target_language, text);
    printf("========================================================\n\n");
    
    // Synthesize and play audio with Piper TTS
    if (session->tts_initialized && session->tts_context) {
        ethervox_tts_audio_t tts_output = {0};
        ethervox_result_t result = ethervox_tts_synthesize_text(session->tts_context, text, &tts_output);
        
        if (ethervox_is_success(result) && tts_output.samples && tts_output.sample_count > 0) {
            ETHERVOX_LOG_INFO("TTS synthesized %zu samples at %dHz", 
                            tts_output.sample_count, tts_output.sample_rate);
            
            // Set AEC reference buffer (must be called before playback)
            if (session->aec_initialized && session->aec_context) {
                ethervox_aec_set_reference(session->aec_context, 
                                          tts_output.samples, 
                                          tts_output.sample_count);
                ETHERVOX_LOG_DEBUG("AEC reference buffer updated with TTS output");
            }
            
            // Play the synthesized audio through speakers
            if (session->audio_initialized && session->audio_runtime.driver.write_audio) {
                // Convert float samples to int16 for CoreAudio
                size_t byte_count = tts_output.sample_count * sizeof(int16_t);
                int16_t* pcm_buffer = (int16_t*)malloc(byte_count);
                if (pcm_buffer) {
                    for (size_t i = 0; i < tts_output.sample_count; i++) {
                        float sample = tts_output.samples[i];
                        // Clamp and convert to int16
                        if (sample > 1.0f) sample = 1.0f;
                        if (sample < -1.0f) sample = -1.0f;
                        pcm_buffer[i] = (int16_t)(sample * 32767.0f);
                    }
                    
                    ethervox_audio_buffer_t playback_buffer = {
                        .data = (float*)pcm_buffer,  // Cast to float* to match struct type
                        .size = byte_count,  // Size in BYTES for the audio driver
                        .channels = tts_output.channels
                    };
                    
                    int play_result = session->audio_runtime.driver.write_audio(
                        &session->audio_runtime, &playback_buffer);
                    
                    if (play_result == 0) {
                        ETHERVOX_LOG_INFO("Audio playback queued (%zu samples)", tts_output.sample_count);
                        
                        // ALWAYS wait for playback to finish before resuming listening
                        // This prevents microphone from capturing TTS echo/feedback
#ifdef __APPLE__
                        // macOS-specific playback synchronization
                        macos_audio_state_t* state = (macos_audio_state_t*)session->audio_runtime.platform_data;
                        if (state) {
                            ETHERVOX_LOG_DEBUG("Waiting for audio playback to complete (allow_interrupt=%d)...", allow_interrupt);
                            
                            // Give the playback thread time to start consuming samples
                            usleep(20000); // 20ms initial delay
                            
                            int poll_count = 0;
                            while (1) {
                                pthread_mutex_lock(&state->playback_lock);
                                bool is_empty = (state->playback_write_pos == state->playback_read_pos);
                                size_t write_pos = state->playback_write_pos;
                                size_t read_pos = state->playback_read_pos;
                                pthread_mutex_unlock(&state->playback_lock);
                                
                                if (poll_count % 50 == 0 && !is_empty) { // Log every 500ms while playing
                                    ETHERVOX_LOG_DEBUG("Playback buffer: write=%zu read=%zu", write_pos, read_pos);
                                }
                                
                                if (is_empty) {
                                    break;
                                }
                                
                                // Check for interruption if allowed (user speaking detected)
                                if (allow_interrupt && poll_count % 10 == 0) {
                                    pthread_mutex_lock(&session->mutex);
                                    bool should_stop = session->thread_should_exit;
                                    pthread_mutex_unlock(&session->mutex);
                                    
                                    if (should_stop) {
                                        ETHERVOX_LOG_INFO("Audio playback interrupted by user");
                                        // Clear the playback buffer to stop audio immediately
                                        pthread_mutex_lock(&state->playback_lock);
                                        state->playback_write_pos = state->playback_read_pos;
                                        pthread_mutex_unlock(&state->playback_lock);
                                        break;
                                    }
                                }
                                
                                usleep(10000); // 10ms polling interval
                                poll_count++;
                            }
                            ETHERVOX_LOG_DEBUG("Audio playback completed");
                        }
#endif
                    } else {
                        ETHERVOX_LOG_WARN("Audio playback failed: %d", play_result);
                    }
                    
                    free(pcm_buffer);
                } else {
                    ETHERVOX_LOG_ERROR("Failed to allocate PCM buffer");
                }
            } else {
                ETHERVOX_LOG_WARN("Audio playback not available");
            }
            
            ethervox_tts_audio_free(&tts_output);
        } else {
            ETHERVOX_LOG_WARN("TTS synthesis failed (code=%d), using text-only mode", result);
        }
    } else {
        ETHERVOX_LOG_DEBUG("TTS not initialized, text-only mode");
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * @brief Callback for listen tool - captures microphone input
 */
static int conversation_on_listen(char** user_input, int timeout_ms,
                                   const char* prompt_hint, void* user_data) {
    ethervox_conversation_session_t* session = (ethervox_conversation_session_t*)user_data;
    if (!session || !user_input) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOG_INFO("[Listen Tool] Capturing audio (timeout=%dms, hint=%s)",
                      timeout_ms, prompt_hint ? prompt_hint : "none");
    
    pthread_mutex_lock(&session->mutex);
    session->state = ETHERVOX_CONV_STATE_LISTENING;
    pthread_mutex_unlock(&session->mutex);
    
    if (prompt_hint) {
        printf("💬 %s\n", prompt_hint);
    }
    
    *user_input = NULL;
    
    // TODO: Implement actual STT capture with Vosk
    // For now, return placeholder
    if (!session->stt_initialized) {
        ETHERVOX_LOG_WARN("STT not initialized, cannot capture audio");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Failure - no STT available
    }
    
    // Start audio capture with timeout
    uint64_t start_time = get_time_ms();
    ethervox_audio_buffer_t audio_chunk = {0};
    
    // Use the existing STT system to capture and transcribe speech
    // This is the same flow used in the main conversation loop
    printf("🎤 Listening");
    fflush(stdout);
    
    // Accumulate audio until speech detected or timeout
    ethervox_audio_buffer_t accumulated_audio = {0};
    bool speech_detected = false;
    int silence_frames = 0;
    const int silence_threshold = 10;  // frames of silence before considering speech ended
    
    while ((get_time_ms() - start_time) < (uint64_t)timeout_ms) {
        ethervox_result_t audio_result = ethervox_audio_read(&session->audio_runtime, &audio_chunk);
        if (ethervox_is_error(audio_result) || audio_chunk.size == 0) {
            break;
        }
        
        // Check for voice activity using RMS energy
        float energy = ethervox_audio_calculate_rms_energy(
            audio_chunk.data, audio_chunk.size / sizeof(int16_t)
        );
        
        // Use a reasonable default threshold (TODO: get from settings)
        const float energy_threshold = 0.02f;
        if (energy > energy_threshold) {
            speech_detected = true;
            silence_frames = 0;
            printf(".");
            fflush(stdout);
            
            // Process audio chunk through STT
            ethervox_stt_result_t stt_result;
            ethervox_result_t stt_ret = ethervox_stt_process(&session->stt_runtime, &audio_chunk, &stt_result);
            if (ethervox_is_success(stt_ret) && stt_result.is_final && stt_result.text && strlen(stt_result.text) > 0) {
                // Got final transcription
                *user_input = strdup(stt_result.text);
                ETHERVOX_LOG_INFO("Transcribed from listen tool: %s", *user_input);
                
                // Capture detected language from Whisper STT for multilingual TTS
                if (stt_result.language && strlen(stt_result.language) > 0) {
                    strncpy(session->last_detected_language, stt_result.language, 
                           sizeof(session->last_detected_language) - 1);
                    session->last_detected_language[sizeof(session->last_detected_language) - 1] = '\0';
                    ETHERVOX_LOG_INFO("[Language Detection] STT detected language: %s", 
                                     session->last_detected_language);
                }
                
                printf(" [OK]\n");
                ethervox_audio_buffer_free(&audio_chunk);
                return ETHERVOX_SUCCESS;
            }
        } else if (speech_detected) {
            silence_frames++;
            if (silence_frames >= silence_threshold) {
                printf(" [OK]\n");
                // Speech ended - finalize transcription
                ethervox_stt_result_t final_result;
                if (ethervox_stt_finalize(&session->stt_runtime, &final_result) == 0) {
                    if (final_result.text && strlen(final_result.text) > 0) {
                        *user_input = strdup(final_result.text);
                        ETHERVOX_LOG_INFO("Finalized transcription: %s", *user_input);
                        
                        // Capture detected language
                        if (final_result.language && strlen(final_result.language) > 0) {
                            strncpy(session->last_detected_language, final_result.language,
                                   sizeof(session->last_detected_language) - 1);
                            session->last_detected_language[sizeof(session->last_detected_language) - 1] = '\0';
                            ETHERVOX_LOG_INFO("[Language Detection] Finalized STT language: %s",
                                             session->last_detected_language);
                        }
                    }
                }
                ethervox_audio_buffer_free(&audio_chunk);
                return ETHERVOX_SUCCESS;
            }
        }
        
        ethervox_audio_buffer_free(&audio_chunk);
    }
    
    printf(" ⏱️\n");
    
    // Timeout reached - try to get partial transcription
    if (speech_detected) {
        ethervox_stt_result_t final_result;
        if (ethervox_stt_finalize(&session->stt_runtime, &final_result) == 0) {
            if (final_result.text && strlen(final_result.text) > 0) {
                *user_input = strdup(final_result.text);
                ETHERVOX_LOG_INFO("Partial transcription on timeout: %s", *user_input);
            }
        }
    }
    
    ETHERVOX_LOG_INFO("Listen timeout reached after %dms", timeout_ms);
    return ETHERVOX_SUCCESS;  // Return 0 for success even on timeout
}

/**
 * @brief Callback for interrupt detection
 */
static int conversation_on_interrupt(void* user_data) {
    ethervox_conversation_session_t* session = (ethervox_conversation_session_t*)user_data;
    if (!session) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check if thread should exit or conversation should stop
    pthread_mutex_lock(&session->mutex);
    bool should_interrupt = session->thread_should_exit;
    pthread_mutex_unlock(&session->mutex);
    
    if (should_interrupt) {
        ETHERVOX_LOG_INFO("[Interrupt] Conversation interrupted");
        return ETHERVOX_SUCCESS;  // Interrupt detected
    }
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;  // No interrupt
}

/**
 * @brief Conversation processing thread
 */
static void* conversation_thread(void* arg) {
    ethervox_conversation_session_t* session = (ethervox_conversation_session_t*)arg;
    
    printf("\n========================================================\n");
    printf("🎙️  CONVERSATION THREAD STARTED\n");
    printf("========================================================\n");
    
    pthread_mutex_lock(&session->mutex);
    
    // Check if always-listening mode is enabled
    bool always_listening = session->always_listening;
    
    if (always_listening) {
        printf("🔊 Always-listening mode: ENABLED (continuous STT, no wake word needed)\n");
    } else {
        printf("👂 Wake word mode: ENABLED (waiting for wake word trigger)\n");
    }
    
    printf("Governor: %s\n", session->governor ? "[OK] Connected" : "❌ Not connected");
    printf("========================================================\n\n");
    
    pthread_mutex_unlock(&session->mutex);
    
    // Initialize audio and STT once for the session
    if (!session->audio_initialized) {
        printf("🎤 Initializing microphone...\n");
        ethervox_audio_config_t audio_config = {0};
        audio_config.sample_rate = 16000;
        audio_config.channels = 1;
        audio_config.bits_per_sample = 16;
        audio_config.buffer_size = 4096;
        
        if (ethervox_audio_register_platform_driver(&session->audio_runtime) == 0 &&
            session->audio_runtime.driver.init(&session->audio_runtime, &audio_config) == 0) {
            session->audio_initialized = true;
            printf("[OK] Microphone ready\n");
        } else {
            printf("❌ Failed to initialize microphone\n");
            return NULL;
        }
    }
    
    if (!session->stt_initialized) {
        printf("🗣️  Initializing speech recognition (Whisper)...\n");
        ethervox_stt_config_t stt_config = ethervox_stt_get_default_config();
        stt_config.sample_rate = 16000;
        stt_config.enable_partial_results = true;
        
        // Use Whisper streaming (already compiled in)
        stt_config.backend = ETHERVOX_STT_BACKEND_WHISPER;
        
        // Set Whisper model path
        const char* home = getenv("HOME");
        static char whisper_model_path[512];
        if (home) {
            snprintf(whisper_model_path, sizeof(whisper_model_path), 
                     "%s/.ethervox/models/whisper/base.bin", home);
            stt_config.model_path = whisper_model_path;
        }
        
        if (ethervox_stt_init(&session->stt_runtime, &stt_config) == 0) {
            session->stt_initialized = true;
            printf("[OK] Speech recognition ready\n");
        } else {
            printf("❌ Failed to initialize speech recognition\n");
            return NULL;
        }
    }
    
    printf("\n========================================================\n");
    if (always_listening) {
        printf("🎙️  CONTINUOUS LISTENING MODE ACTIVE\n");
        printf("   Speak anytime - no wake word needed\n");
    } else {
        printf("👂 WAKE WORD MODE ACTIVE\n");
        printf("   Say 'hey ethervox' to start\n");
    }
    printf("========================================================\n\n");
    
    while (!session->thread_should_exit) {
        pthread_mutex_lock(&session->mutex);
        
        if (always_listening) {
            // Always-listening mode: immediate listening state
            session->state = ETHERVOX_CONV_STATE_LISTENING;
        } else {
            // Wake word mode: wait for trigger
            session->state = ETHERVOX_CONV_STATE_IDLE;
            
            while (!session->thread_should_exit && session->state == ETHERVOX_CONV_STATE_IDLE) {
                pthread_cond_wait(&session->trigger_cond, &session->mutex);
            }
            
            if (session->thread_should_exit) {
                pthread_mutex_unlock(&session->mutex);
                break;
            }
            
            session->state = ETHERVOX_CONV_STATE_LISTENING;
            printf("\n🎤 Wake word detected, listening...\n");
        }
        
        pthread_mutex_unlock(&session->mutex);
        
        // Simple audio capture loop - continuously listen and transcribe
        printf("🎤 Listening");
        fflush(stdout);
        
        char recognized_text[1024] = {0};
        bool speech_detected = false;
        
        // Start STT and audio capture
        if (ethervox_stt_start(&session->stt_runtime) != 0) {
            ETHERVOX_LOG_ERROR("Failed to start STT");
            pthread_mutex_lock(&session->mutex);
            session->state = ETHERVOX_CONV_STATE_IDLE;
            pthread_mutex_unlock(&session->mutex);
            usleep(100000);
            continue;
        }
        
        if (session->audio_runtime.driver.start_capture(&session->audio_runtime) != 0) {
            ETHERVOX_LOG_ERROR("Failed to start audio capture");
            ethervox_stt_stop(&session->stt_runtime);
            pthread_mutex_lock(&session->mutex);
            session->state = ETHERVOX_CONV_STATE_IDLE;
            pthread_mutex_unlock(&session->mutex);
            usleep(100000);
            continue;
        }
        
        // Streaming audio capture: continuously feed to Whisper
        uint64_t listen_start = get_time_ms();
        
        while (!speech_detected && !session->thread_should_exit) {
            ethervox_audio_buffer_t audio_chunk;
            audio_chunk.size = 1600;  // 100ms at 16kHz
            audio_chunk.channels = 1;
            audio_chunk.data = (float*)calloc(audio_chunk.size, sizeof(float));
            
            if (!audio_chunk.data) {
                break;
            }
            
            ethervox_result_t read_result = session->audio_runtime.driver.read_audio(&session->audio_runtime, &audio_chunk);
            if (ethervox_is_error(read_result) || audio_chunk.size == 0) {
                free(audio_chunk.data);
                usleep(10000);  // Wait and try again
                continue;
            }
            
            int samples_read = (int)audio_chunk.size;
            
            // Apply AEC to remove speaker output from microphone input
            // AEC requires 10ms frames (160 samples at 16kHz), so process in chunks
            if (session->aec_initialized && session->aec_context) {
                const size_t aec_frame_size = 160;  // 10ms at 16kHz
                size_t offset = 0;
                
                while (offset < samples_read) {
                    size_t frame_samples = (offset + aec_frame_size <= samples_read) ? 
                                          aec_frame_size : (samples_read - offset);
                    
                    // Only process full frames (AEC needs exact frame size)
                    if (frame_samples == aec_frame_size) {
                        ethervox_result_t aec_result = ethervox_aec_process(session->aec_context, 
                                                              audio_chunk.data + offset, 
                                                              frame_samples);
                        if (ethervox_is_error(aec_result)) {
                            ETHERVOX_LOG_WARN("AEC processing failed at offset %zu: %d", offset, aec_result);
                            break;  // Stop processing on error
                        }
                    }
                    // else: partial frame at end, skip AEC (will be processed with next chunk)
                    
                    offset += frame_samples;
                }
            }
            
            // Feed all audio to Whisper - let it decide on VAD and boundaries
            ethervox_stt_result_t stt_result = {0};
            ethervox_result_t stt_ret = ethervox_stt_process(&session->stt_runtime, &audio_chunk, &stt_result);
            
            // Check for results (Whisper returns is_final when it detects sentence boundary)
            if (ethervox_is_success(stt_ret) && stt_result.text && strlen(stt_result.text) > 3) {
                // Show partial results
                if (stt_result.is_partial) {
                    printf("\r🎤 %s", stt_result.text);
                    fflush(stdout);
                }
                
                // Trust Whisper's is_final flag - it knows speech boundaries
                if (stt_result.is_final) {
                    // Filter Whisper hallucinations
                    if (strstr(stt_result.text, "Transcribed by") == NULL &&
                        strstr(stt_result.text, "R.A.R.E.") == NULL &&
                        strstr(stt_result.text, "Thank you") != stt_result.text) {
                        
                        strncpy(recognized_text, stt_result.text, sizeof(recognized_text) - 1);
                        speech_detected = true;
                        printf("\r[OK] Final: %s\n", stt_result.text);
                        
                        // Capture detected language from Whisper
                        if (stt_result.language && strlen(stt_result.language) > 0) {
                            strncpy(session->last_detected_language, stt_result.language,
                                   sizeof(session->last_detected_language) - 1);
                            session->last_detected_language[sizeof(session->last_detected_language) - 1] = '\0';
                            ETHERVOX_LOG_INFO("[Language Detection] Whisper detected: %s",
                                             session->last_detected_language);
                        }
                    }
                }
                
                // Free STT result memory
                ethervox_stt_result_free(&stt_result);
            }
            
            free(audio_chunk.data);
            
            // Timeout check (30 seconds max)
            if (get_time_ms() - listen_start > 30000) {
                printf("\r⏱️  Timeout\n");
                break;
            }
            
            usleep(10000);  // 10ms sleep
        }
        
        ethervox_stt_stop(&session->stt_runtime);
        session->audio_runtime.driver.stop_capture(&session->audio_runtime);
        
        pthread_mutex_lock(&session->mutex);
        
        if (!speech_detected || strlen(recognized_text) == 0) {
            // No speech detected in always-listening mode - keep looping
            if (always_listening) {
                session->state = ETHERVOX_CONV_STATE_LISTENING;
                pthread_mutex_unlock(&session->mutex);
                usleep(100000);  // 100ms sleep to avoid tight loop
                continue;
            }
            // In wake word mode, return to idle
            session->state = ETHERVOX_CONV_STATE_IDLE;
            pthread_mutex_unlock(&session->mutex);
            continue;
        }
        
        // Speech detected - process with Governor
        session->state = ETHERVOX_CONV_STATE_PROCESSING;
        pthread_mutex_unlock(&session->mutex);
        
        printf("\n👤 User: %s\n", recognized_text);
        ETHERVOX_LOG_INFO("Processing user input with Governor: %s", recognized_text);
        
        // Send to Governor with execution context for tool-based conversational AI
        if (session->governor) {
            char* llm_response = NULL;
            char* error_msg = NULL;
            
            // Set up conversation callbacks for speak/listen tools
            ethervox_conversation_callbacks_t callbacks = {
                .on_speak = conversation_on_speak,
                .on_listen = conversation_on_listen,
                .on_interrupt = conversation_on_interrupt,
                .user_data = session
            };
            
            // Create execution context with VOICE source
            ethervox_execution_context_t exec_context = {
                .source = ETHERVOX_INPUT_SOURCE_VOICE,
                .source_description = "voice conversation",
                .tts_available = session->audio_initialized,
                .microphone_available = session->stt_initialized,
                .current_turn = ETHERVOX_TURN_USER,
                .callbacks = &callbacks
            };
            
            // Execute Governor - this will call speak/listen tools via callbacks
            ethervox_governor_status_t status = ethervox_governor_execute_with_context(
                session->governor,
                recognized_text,
                &exec_context,
                &llm_response,
                &error_msg,
                NULL,  // metrics (optional)
                NULL,  // progress callback (optional)
                NULL,  // token callback (optional)
                NULL   // user_data (optional)
            );
            
            // NOTE: In the new tool-based architecture, the LLM should use the 'speak' tool
            // to generate responses. If we get a direct text response here, it means:
            // 1. The LLM didn't use the speak tool (needs stronger prompting), OR
            // 2. The response is informational/acknowledgment
            
            if (status == ETHERVOX_GOVERNOR_SUCCESS) {
                if (llm_response && strlen(llm_response) > 0) {
                    // LLM returned text without using speak tool - print as fallback
                    ETHERVOX_LOG_INFO("Governor returned text response (speak tool not used): %s", llm_response);
                    printf("🤖 Assistant (text): %s\n", llm_response);
                    free(llm_response);
                } else {
                    // Success with no text response means LLM used tools correctly
                    ETHERVOX_LOG_INFO("Governor executed successfully (tools used)");
                }
            } else if (status == ETHERVOX_GOVERNOR_INTERRUPTED) {
                ETHERVOX_LOG_INFO("Conversation interrupted by user");
                if (llm_response) free(llm_response);
                if (error_msg) free(error_msg);
                break;  // Exit conversation thread
            } else {
                ETHERVOX_LOG_WARN("Governor execution failed: %s", 
                                error_msg ? error_msg : "unknown error");
                printf("❌ Error processing request: %s\n", 
                       error_msg ? error_msg : "unknown error");
                if (error_msg) free(error_msg);
            }
        } else {
            ETHERVOX_LOG_WARN("No Governor instance available");
            printf("❌ Governor not initialized\n");
        }
        
        pthread_mutex_lock(&session->mutex);
        
        // Return to appropriate state based on mode
        if (always_listening) {
            // In always-listening mode, immediately go back to listening
            session->state = ETHERVOX_CONV_STATE_LISTENING;
            pthread_mutex_unlock(&session->mutex);
            ETHERVOX_LOG_DEBUG("Continuing in always-listening mode...");
        } else {
            // In wake word mode, return to idle and wait for next trigger
            session->state = ETHERVOX_CONV_STATE_IDLE;
            pthread_mutex_unlock(&session->mutex);
        }
    }
    
    pthread_mutex_lock(&session->mutex);
    session->state = ETHERVOX_CONV_STATE_UNINITIALIZED;
    session->thread_running = false;
    pthread_mutex_unlock(&session->mutex);
    
    return NULL;
}

ethervox_conversation_config_t ethervox_conversation_get_default_config(void) {
    ethervox_conversation_config_t config = {0};
    
    // Vosk configuration
    config.vosk.model_path = NULL; // Will auto-detect in ~/.ethervox/models/vosk/
    config.vosk.sample_rate = 16000;
    config.vosk.max_alternatives = 1;
    config.vosk.partial_results = true;
    
    // Piper configuration
    config.piper.model_path = NULL; // Will auto-detect in ~/.ethervox/models/piper/
    config.piper.config_path = NULL;
    config.piper.speed = 1.0f;
    config.piper.sample_rate = 22050;
    
    // Conversation timeouts
    config.listen_timeout_ms = 5000;        // 5 seconds of silence to stop listening
    config.conversation_timeout_ms = 30000;  // 30 seconds max conversation
    config.audio_buffer_size = 16000 * 10;   // 10 seconds at 16kHz
    
    // Audio feedback
    config.enable_beep_on_wake = true;
    config.enable_beep_on_listen_end = true;
    
    // Always-listening mode (enabled on desktop platforms with sufficient resources)
#if defined(ETHERVOX_PLATFORM_MACOS) || defined(ETHERVOX_PLATFORM_LINUX) || defined(ETHERVOX_PLATFORM_WINDOWS)
    config.always_listening = true;  // Desktop: continuous STT, no wake word needed
#else
    config.always_listening = false; // Embedded: use wake word to conserve resources
#endif
    
    return config;
}

ethervox_conversation_session_t* ethervox_conversation_init(
    const ethervox_conversation_config_t* config,
    ethervox_governor_t* governor_runtime
) {
    if (!config) {
        ETHERVOX_LOG_ERROR("conversation_init: NULL config");
        return NULL;
    }
    
    ethervox_conversation_session_t* session = 
        (ethervox_conversation_session_t*)calloc(1, sizeof(ethervox_conversation_session_t));
    if (!session) {
        ETHERVOX_LOG_ERROR("conversation_init: allocation failed");
        return NULL;
    }
    
    // Copy configuration
    memcpy(&session->config, config, sizeof(ethervox_conversation_config_t));
    session->governor = governor_runtime;
    
    // Initialize threading primitives
    pthread_mutex_init(&session->mutex, NULL);
    pthread_cond_init(&session->trigger_cond, NULL);
    session->thread_running = false;
    session->thread_should_exit = false;
    session->state = ETHERVOX_CONV_STATE_UNINITIALIZED;
    
    // Initialize STT and audio
    session->stt_initialized = false;
    memset(&session->stt_runtime, 0, sizeof(ethervox_stt_runtime_t));
    session->audio_initialized = false;
    memset(&session->audio_runtime, 0, sizeof(ethervox_audio_runtime_t));
    session->audio_buffer = NULL;
    session->audio_capture_active = false;
    
    // Always-listening mode
    session->always_listening = config->always_listening;
    session->pending_transcription = NULL;
    
    // Language detection
    session->last_detected_language[0] = '\0';  // Initialize to empty (use fallback detection)
    
    // Initialize TTS (Piper backend)
    session->tts_initialized = false;
    session->tts_context = NULL;
    
    // Load settings for TTS and AEC configuration
    ethervox_persistent_settings_t settings;
    bool settings_loaded = ethervox_is_success(ethervox_settings_load(&settings, NULL));
    
    // Check if global TTS is already initialized (from app startup)
    // If so, reuse it instead of creating a new instance
    pthread_mutex_lock(&g_tts_mutex);
    if (g_global_tts) {
        session->tts_context = g_global_tts;
        session->tts_initialized = true;
        pthread_mutex_unlock(&g_tts_mutex);
        ETHERVOX_LOG_INFO("Reusing global TTS instance for voice conversation");
    } else {
        pthread_mutex_unlock(&g_tts_mutex);
        
        // No global TTS, initialize one for this session
        if (settings_loaded) {
            // Check if Piper is enabled and model exists
            if (strcmp(settings.tts.engine, "piper") == 0 && 
                strlen(settings.tts.piper_model_path) > 0) {
                
                ethervox_tts_config_t tts_config = ethervox_tts_default_config();
                tts_config.backend = ETHERVOX_TTS_BACKEND_PIPER;
                tts_config.model_path = settings.tts.piper_model_path;
                tts_config.speaking_rate = settings.tts.speed;
                tts_config.phoneme_variance = settings.tts.phoneme_variance;
                tts_config.prosody_variance = settings.tts.prosody_variance;
                tts_config.sample_rate = 16000;  // Target sample rate
                tts_config.channels = 1;         // Mono
                
                session->tts_context = ethervox_tts_create(&tts_config);
                if (session->tts_context && ethervox_tts_is_ready(session->tts_context)) {
                    session->tts_initialized = true;
                    ETHERVOX_LOG_INFO("Piper TTS initialized: %s", settings.tts.piper_model_path);
                } else {
                    ETHERVOX_LOG_WARN("Failed to initialize Piper TTS");
                    if (session->tts_context) {
                        ethervox_tts_destroy(session->tts_context);
                        session->tts_context = NULL;
                    }
                }
            } else {
                ETHERVOX_LOG_INFO("TTS disabled or not Piper (engine=%s)", settings.tts.engine);
            }
        } else {
            ETHERVOX_LOG_WARN("Failed to load settings, TTS disabled");
        }
    }
    
    // Initialize AEC if enabled
    session->aec_initialized = false;
    session->aec_context = NULL;
    
    if (settings_loaded && settings.aec.enabled && strcmp(settings.aec.backend, "speex") == 0) {
        ethervox_aec_config_t aec_config = {
            .sample_rate = 16000,
            .frame_size = 160,  // 10ms frames at 16kHz
            .filter_length = settings.aec.filter_length_ms,
            .suppression_level = settings.aec.suppression_level
        };
        
        session->aec_context = ethervox_aec_create(&aec_config);
        if (session->aec_context) {
            session->aec_initialized = true;
            ETHERVOX_LOG_INFO("AEC initialized (backend=%s, filter=%dms, suppression=%.2f)",
                            settings.aec.backend, settings.aec.filter_length_ms, 
                            settings.aec.suppression_level);
        } else {
            ETHERVOX_LOG_WARN("Failed to initialize AEC");
        }
    } else if (settings_loaded) {
        ETHERVOX_LOG_INFO("AEC disabled (enabled=%d, backend=%s)", 
                        settings.aec.enabled, settings.aec.backend);
    }
    
    ETHERVOX_LOG_INFO("Conversation session initialized (always_listening=%d, TTS=%d, AEC=%d)",
                      session->always_listening, session->tts_initialized, session->aec_initialized);
    
    return session;
}

ethervox_result_t ethervox_conversation_start(ethervox_conversation_session_t* session) {
    if (!session) {
        ETHERVOX_LOG_ERROR("conversation_start: NULL session");
        return -EINVAL;
    }
    
    pthread_mutex_lock(&session->mutex);
    
    if (session->thread_running) {
        pthread_mutex_unlock(&session->mutex);
        ETHERVOX_LOG_WARN("conversation_start: already running");
        return ETHERVOX_SUCCESS; // Not an error
    }
    
    session->thread_should_exit = false;
    
    int rc = pthread_create(&session->thread_id, NULL, conversation_thread, session);
    if (rc != 0) {
        pthread_mutex_unlock(&session->mutex);
        ETHERVOX_LOG_ERROR("conversation_start: pthread_create failed: %d", rc);
        return -rc;
    }
    
    session->thread_running = true;
    
    pthread_mutex_unlock(&session->mutex);
    
    ETHERVOX_LOG_INFO("Conversation thread started");
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_conversation_stop(ethervox_conversation_session_t* session) {
    if (!session) {
        ETHERVOX_LOG_ERROR("conversation_stop: NULL session");
        return -EINVAL;
    }
    
    pthread_mutex_lock(&session->mutex);
    
    if (!session->thread_running) {
        pthread_mutex_unlock(&session->mutex);
        return ETHERVOX_SUCCESS; // Already stopped
    }
    
    session->thread_should_exit = true;
    pthread_cond_signal(&session->trigger_cond); // Wake up thread
    
    pthread_mutex_unlock(&session->mutex);
    
    // Wait for thread to exit
    pthread_join(session->thread_id, NULL);
    
    ETHERVOX_LOG_INFO("Conversation thread stopped");
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_conversation_trigger(ethervox_conversation_session_t* session) {
    if (!session) {
        ETHERVOX_LOG_ERROR("conversation_trigger: NULL session");
        return -EINVAL;
    }
    
    pthread_mutex_lock(&session->mutex);
    
    if (!session->thread_running) {
        pthread_mutex_unlock(&session->mutex);
        ETHERVOX_LOG_WARN("conversation_trigger: thread not running");
        return -EINVAL;
    }
    
    // Only trigger if idle
    if (session->state == ETHERVOX_CONV_STATE_IDLE) {
        session->state = ETHERVOX_CONV_STATE_LISTENING;
        pthread_cond_signal(&session->trigger_cond);
        ETHERVOX_LOG_DEBUG("Conversation triggered by wake word");
    } else {
        ETHERVOX_LOG_DEBUG("Conversation trigger ignored (already active)");
    }
    
    pthread_mutex_unlock(&session->mutex);
    
    return ETHERVOX_SUCCESS;
}

ethervox_conversation_state_t ethervox_conversation_get_state(
    const ethervox_conversation_session_t* session
) {
    if (!session) {
        return ETHERVOX_CONV_STATE_UNINITIALIZED;
    }
    
    // Read state atomically
    ethervox_conversation_state_t state;
    pthread_mutex_lock((pthread_mutex_t*)&session->mutex);
    state = session->state;
    pthread_mutex_unlock((pthread_mutex_t*)&session->mutex);
    
    return state;
}

bool ethervox_conversation_is_active(
    const ethervox_conversation_session_t* session
) {
    ethervox_conversation_state_t state = ethervox_conversation_get_state(session);
    return (state == ETHERVOX_CONV_STATE_LISTENING ||
            state == ETHERVOX_CONV_STATE_PROCESSING ||
            state == ETHERVOX_CONV_STATE_SPEAKING);
}

void ethervox_conversation_cleanup(ethervox_conversation_session_t* session) {
    if (!session) {
        return;
    }
    
    // Stop thread if running
    if (session->thread_running) {
        ethervox_conversation_stop(session);
    }
    
    // Cleanup STT runtime
    if (session->stt_initialized) {
        ethervox_stt_cleanup(&session->stt_runtime);
        session->stt_initialized = false;
    }
    
    // Cleanup TTS context (but NOT if it's the global instance)
    if (session->tts_initialized && session->tts_context) {
        pthread_mutex_lock(&g_tts_mutex);
        bool is_global = (session->tts_context == g_global_tts);
        pthread_mutex_unlock(&g_tts_mutex);
        
        if (!is_global) {
            // Session-specific TTS, safe to destroy
            ethervox_tts_destroy(session->tts_context);
            ETHERVOX_LOG_DEBUG("Session TTS context destroyed");
        } else {
            // Global TTS, just detach from session
            ETHERVOX_LOG_DEBUG("Detached from global TTS (not destroyed)");
        }
        
        session->tts_context = NULL;
        session->tts_initialized = false;
    }
    
    // Cleanup AEC context
    if (session->aec_initialized && session->aec_context) {
        ethervox_aec_destroy(session->aec_context);
        session->aec_context = NULL;
        session->aec_initialized = false;
        ETHERVOX_LOG_DEBUG("AEC context destroyed");
    }
    
    // Free audio buffer if still allocated
    if (session->audio_buffer && session->audio_buffer->data) {
        free(session->audio_buffer->data);
        session->audio_buffer = NULL;
    }
    
    // Destroy threading primitives
    pthread_mutex_destroy(&session->mutex);
    pthread_cond_destroy(&session->trigger_cond);
    
    free(session);
    
    ETHERVOX_LOG_INFO("Conversation session cleaned up");
}

/**
 * Get phonemizer context from conversation session
 */
void* ethervox_conversation_get_phonemizer(ethervox_conversation_session_t* session) {
    if (!session) {
        ETHERVOX_LOG_WARN("get_phonemizer: session is NULL");
        return NULL;
    }
    if (!session->tts_context) {
        ETHERVOX_LOG_WARN("get_phonemizer: tts_context is NULL");
        return NULL;
    }
    
    ETHERVOX_LOG_INFO("get_phonemizer: calling ethervox_tts_get_phonemizer");
    // Get phonemizer from TTS context
    void* result = ethervox_tts_get_phonemizer(session->tts_context);
    if (result) {
        ETHERVOX_LOG_INFO("get_phonemizer: success, got phonemizer %p", result);
    } else {
        ETHERVOX_LOG_WARN("get_phonemizer: ethervox_tts_get_phonemizer returned NULL");
    }
    return result;
}

/**
 * Get TTS context from conversation session
 */
void* ethervox_conversation_get_tts(ethervox_conversation_session_t* session) {
    if (!session) {
        return NULL;
    }
    return session->tts_context;
}

/**
 * Get STT context from conversation session
 */
void* ethervox_conversation_get_stt(ethervox_conversation_session_t* session) {
    if (!session || !session->stt_initialized) {
        return NULL;
    }
    return &session->stt_runtime;
}
