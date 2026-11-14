/**
 * @file voice_conversation.c
 * @brief Real-time voice conversation implementation
 *
 * Manages background thread for wake word → Granite Speech STT → Governor →
 * TTS (platform-native via ethervox_tts_host_t; mobile uses async
 * conversation_on_speak's on_speak_request callback + notify_speaking_done)
 * conversation flow. Separate from transcription pipeline.
 *
 * This is the single, platform-agnostic Mode 1/2/4 orchestrator (STT ->
 * Governor -> TTS -> barge-in). src/platform/ethervox_android_core.c calls
 * into it through a thin JNI shim (ethervox_conversation_init/start/stop/
 * interrupt/notify_speaking_done) rather than re-implementing the state
 * machine - this avoids maintaining the barge-in/interrupt logic twice.
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
#include "ethervox/tts_host.h"
#include "ethervox/aec.h"
#include "ethervox/settings.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

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

    // Explicit barge-in flag - set by ethervox_conversation_interrupt(), checked
    // by conversation_on_speak (desktop Piper playback wait loop) and
    // conversation_on_interrupt (Governor listen/speak tool interrupt check).
    // Cleared at the start of each new listening turn.
    bool interrupt_requested;

    // Mobile TTS completion signal - platform-native TTS (Android
    // TextToSpeech / iOS AVSpeechSynthesizer) reports playback completion
    // asynchronously via its own listener, unlike desktop Piper which blocks
    // this thread directly on the audio ring buffer. conversation_on_speak's
    // mobile branch (config.on_speak_request set) waits on this condition
    // variable instead, and ethervox_conversation_notify_speaking_done() (the
    // JNI/bridge-callable counterpart of onDone()/onError()) signals it.
    pthread_cond_t speaking_done_cond;
    bool speaking_done;
    
    // STT runtime (Granite Speech BASE backend)
    ethervox_stt_runtime_t stt_runtime;
    bool stt_initialized;
    
    // Audio runtime for microphone capture
    ethervox_audio_runtime_t audio_runtime;
    bool audio_initialized;
    
    // AEC runtime (echo cancellation)
    ethervox_aec_t* aec_context;
    bool aec_initialized;
    
    // Audio capture
    ethervox_audio_buffer_t* audio_buffer;
    bool audio_capture_active;

    // Barge-in monitor (VAD-based interrupt during THINKING/SPEAKING) - see
    // conversation.h's barge_in_* config fields and plan.md Open Question 1.
    // Runs as a separate thread reading the continuously-open mic (see
    // conversation_thread()'s capture-lifecycle note) concurrently with
    // Governor generation / TTS playback, since both block the main
    // conversation thread and can't poll audio themselves.
    pthread_t barge_in_thread;
    bool barge_in_thread_active;     // A monitor thread currently exists for this turn
    bool barge_in_stop_requested;    // Ask the monitor to exit cleanly (turn ended, no barge-in)

    // Pre-roll ring buffer: continuously retains the last
    // config.barge_in_preroll_ms of raw audio while the monitor runs, so the
    // syllables that triggered a barge-in aren't lost - handed to the next
    // capture_utterance_with_vad() call as a synthetic head start.
    float* preroll_ring;
    size_t preroll_ring_capacity;    // Samples
    size_t preroll_ring_write_pos;
    size_t preroll_ring_filled;      // Samples written so far, capped at capacity

    // Snapshot of the ring above, taken when a barge-in commits; consumed
    // and freed at the start of the next capture_utterance_with_vad() call.
    float* pending_preroll_audio;
    size_t pending_preroll_samples;
    
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

/**
 * @brief Transition session state and notify the optional on_state_change
 * callback. Caller must already hold session->mutex (matches every existing
 * call site, which set session->state directly under the lock before this
 * helper existed).
 */
static void conversation_set_state(ethervox_conversation_session_t* session,
                                    ethervox_conversation_state_t new_state) {
    session->state = new_state;
    if (session->config.on_state_change) {
        session->config.on_state_change(new_state, session->config.callback_user_data);
    }
}

/**
 * @brief Report an unrecoverable error via the optional on_error callback
 * (in addition to the existing LOG_ERROR calls at each call site).
 */
static void conversation_notify_error(ethervox_conversation_session_t* session,
                                      const char* message) {
    if (session->config.on_error) {
        session->config.on_error(message, session->config.callback_user_data);
    }
}

/**
 * @brief Capture one utterance using RMS-energy silence detection, then
 * finalize() it through Granite Speech.
 *
 * ARCHITECTURE CHANGE (Granite Speech integration): this consolidates two
 * previously-duplicated (and, in the main conversation_thread loop's copy,
 * actually broken) implementations. Granite Speech's ethervox_stt_process()
 * always returns is_final=false (see stt/granite_speech_backend.c) - it only
 * accumulates audio; the transcript is only produced by
 * ethervox_stt_finalize(), which decodes once. Whisper's process() used to
 * emit is_final=true at sentence boundaries, and conversation_thread()'s
 * capture loop still trusted that pattern - dead code today, since Granite
 * Speech never sets that flag, so the loop only ever produced a result via
 * the 30s hard timeout. conversation_on_listen()'s own copy of this logic
 * (used for the Governor's `listen` tool) was already correct. Both call
 * sites now share this one, correct, energy-VAD + finalize() implementation.
 *
 * @param session Conversation session (STT/audio runtimes must already be
 *                started - see ethervox_stt_start()/audio_runtime.driver.start_capture())
 * @param timeout_ms Maximum time to wait for speech before giving up
 * @param text_out Receives a strdup'd transcript on success (caller frees),
 *                 or NULL if no speech was detected before timeout
 * @return ETHERVOX_SUCCESS always (matches conversation_on_listen's original
 *         "timeout is not an error" contract) - check *text_out for NULL to
 *         distinguish "nothing said" from "something transcribed"
 */
static ethervox_result_t capture_utterance_with_vad(ethervox_conversation_session_t* session,
                                                     int timeout_ms, char** text_out) {
    *text_out = NULL;

    // Unified voice model architecture (see
    // docs/UNIFIED_VOICE_MODEL_ARCHITECTURE.md): when the Governor itself
    // was loaded with audio support (ethervox_governor_load_model_with_audio,
    // JNI/caller-side decision), skip the separate STT-runtime path entirely
    // and decode audio directly on the Governor's own llama_context (ASR
    // scratch sequence 2) via ethervox_governor_transcribe_audio(). The
    // RMS-energy VAD logic below is identical either way - only the
    // "what do I do with the accumulated audio" tail differs.
    bool use_governor_audio = ethervox_governor_has_audio_support(session->governor);

    // Raw audio accumulator for the Governor-audio path only (mirrors
    // ethervox_stt_runtime_t's own accumulator sizing in stt_core.c: 30s at
    // 16kHz comfortably covers one Mode 1 conversational turn).
    float* gov_audio_accum = NULL;
    size_t gov_audio_accum_capacity = 0;
    size_t gov_audio_accum_len = 0;
    if (use_governor_audio) {
        gov_audio_accum_capacity = (size_t)session->config.stt.sample_rate * 30;
        gov_audio_accum = (float*)malloc(gov_audio_accum_capacity * sizeof(float));
        if (!gov_audio_accum) {
            ETHERVOX_LOG_ERROR("Failed to allocate Governor audio accumulator");
            return ETHERVOX_ERROR_OUT_OF_MEMORY;
        }
    }

    uint64_t start_time = get_time_ms();
    bool speech_detected = false;
    int silence_frames = 0;
    const int silence_threshold = 10;  // frames of silence before considering speech ended
    const float energy_threshold = 0.02f;  // TODO: make configurable (see plan.md Open Question 1)

    // If a barge-in monitor just interrupted THINKING/SPEAKING, it left the
    // audio that triggered it here (see run_barge_in_monitor()) - feed it
    // into the STT accumulator before the real-time read loop below, so the
    // user's interrupting speech isn't clipped at the start just because
    // this new listening turn started a beat after they began talking.
    pthread_mutex_lock(&session->mutex);
    float* preroll_audio = session->pending_preroll_audio;
    size_t preroll_samples = session->pending_preroll_samples;
    session->pending_preroll_audio = NULL;
    session->pending_preroll_samples = 0;
    pthread_mutex_unlock(&session->mutex);

    if (preroll_audio && preroll_samples > 0) {
        ETHERVOX_LOG_INFO("[Barge-in] Seeding %zu pre-roll samples into new listening turn",
                          preroll_samples);
        if (use_governor_audio) {
            size_t to_copy = preroll_samples < (gov_audio_accum_capacity - gov_audio_accum_len)
                                 ? preroll_samples : (gov_audio_accum_capacity - gov_audio_accum_len);
            memcpy(gov_audio_accum + gov_audio_accum_len, preroll_audio, to_copy * sizeof(float));
            gov_audio_accum_len += to_copy;
        } else {
            ethervox_audio_buffer_t preroll_buffer = {
                .data = preroll_audio,
                .size = (uint32_t)preroll_samples,
                .channels = 1
            };
            ethervox_stt_result_t discard_result = {0};
            ethervox_stt_process(&session->stt_runtime, &preroll_buffer, &discard_result);
            ethervox_stt_result_free(&discard_result);
        }
        speech_detected = true;  // We already know the user was talking
    }
    free(preroll_audio);

    while ((get_time_ms() - start_time) < (uint64_t)timeout_ms) {
        // Explicit barge-in / stop check - bail immediately rather than
        // waiting out the rest of the timeout.
        pthread_mutex_lock(&session->mutex);
        bool should_stop = session->thread_should_exit || session->interrupt_requested;
        pthread_mutex_unlock(&session->mutex);
        if (should_stop) {
            break;
        }

        ethervox_audio_buffer_t audio_chunk = {0};
        ethervox_result_t audio_result = ethervox_audio_read(&session->audio_runtime, &audio_chunk);
        if (ethervox_is_error(audio_result) || audio_chunk.size == 0) {
            continue;
        }

        // Apply AEC to remove speaker output from microphone input, if
        // available (desktop-only Speex backend - see the file header's
        // barge-in note; a no-op when aec_initialized is false, e.g. on
        // Android/iOS). AEC requires exact 10ms frames (160 samples at
        // 16kHz); partial trailing frames are skipped and picked up whole
        // on the next read. audio_chunk.size is a SAMPLE count, not a byte
        // count (see ethervox_audio_buffer_t in audio.h).
        if (session->aec_initialized && session->aec_context) {
            const size_t aec_frame_size = 160;
            size_t offset = 0;
            while (offset + aec_frame_size <= audio_chunk.size) {
                ethervox_result_t aec_result = ethervox_aec_process(
                    session->aec_context, audio_chunk.data + offset, aec_frame_size);
                if (ethervox_is_error(aec_result)) {
                    ETHERVOX_LOG_WARN("AEC processing failed at offset %zu: %d", offset, aec_result);
                    break;
                }
                offset += aec_frame_size;
            }
        }

        float energy = ethervox_audio_calculate_rms_energy(audio_chunk.data, audio_chunk.size);

        if (energy > energy_threshold) {
            speech_detected = true;
            silence_frames = 0;

            if (use_governor_audio) {
                size_t space_left = gov_audio_accum_capacity - gov_audio_accum_len;
                size_t to_copy = audio_chunk.size < space_left ? audio_chunk.size : space_left;
                if (to_copy > 0) {
                    memcpy(gov_audio_accum + gov_audio_accum_len, audio_chunk.data,
                           to_copy * sizeof(float));
                    gov_audio_accum_len += to_copy;
                }
                if (to_copy < audio_chunk.size) {
                    ETHERVOX_LOG_WARN("Governor audio accumulator full (%zu/%zu samples), "
                                      "dropping %u samples", gov_audio_accum_len,
                                      gov_audio_accum_capacity, audio_chunk.size - (uint32_t)to_copy);
                }
            } else {
                ethervox_stt_result_t stt_result = {0};
                ethervox_result_t stt_ret = ethervox_stt_process(&session->stt_runtime, &audio_chunk, &stt_result);
                // Granite Speech's process() never sets is_final (see doc comment
                // above) - this branch is effectively unreachable for it today,
                // kept only in case a future backend does support incremental
                // finalization mid-utterance.
                if (ethervox_is_success(stt_ret) && stt_result.is_final && stt_result.text &&
                    strlen(stt_result.text) > 0) {
                    *text_out = strdup(stt_result.text);
                    if (stt_result.language && strlen(stt_result.language) > 0) {
                        strncpy(session->last_detected_language, stt_result.language,
                                sizeof(session->last_detected_language) - 1);
                        session->last_detected_language[sizeof(session->last_detected_language) - 1] = '\0';
                    }
                    ethervox_stt_result_free(&stt_result);
                    ethervox_audio_buffer_free(&audio_chunk);
                    free(gov_audio_accum);
                    return ETHERVOX_SUCCESS;
                }
                ethervox_stt_result_free(&stt_result);
            }
        } else if (speech_detected) {
            silence_frames++;
            if (silence_frames >= silence_threshold) {
                if (use_governor_audio) {
                    if (gov_audio_accum_len > 0) {
                        char* transcript = NULL;
                        if (ethervox_is_success(ethervox_governor_transcribe_audio(
                                session->governor, gov_audio_accum, (uint32_t)gov_audio_accum_len,
                                &transcript)) && transcript) {
                            *text_out = transcript;
                        } else {
                            free(transcript);
                        }
                    }
                } else {
                    ethervox_stt_result_t final_result = {0};
                    if (ethervox_stt_finalize(&session->stt_runtime, &final_result) == 0) {
                        if (final_result.text && strlen(final_result.text) > 0) {
                            *text_out = strdup(final_result.text);
                            if (final_result.language && strlen(final_result.language) > 0) {
                                strncpy(session->last_detected_language, final_result.language,
                                        sizeof(session->last_detected_language) - 1);
                                session->last_detected_language[sizeof(session->last_detected_language) - 1] = '\0';
                            }
                        }
                        ethervox_stt_result_free(&final_result);
                    }
                }
                ethervox_audio_buffer_free(&audio_chunk);
                free(gov_audio_accum);
                return ETHERVOX_SUCCESS;
            }
        }

        ethervox_audio_buffer_free(&audio_chunk);
    }

    // Timeout reached - finalize whatever was accumulated as a best effort.
    if (speech_detected) {
        if (use_governor_audio) {
            if (gov_audio_accum_len > 0) {
                char* transcript = NULL;
                if (ethervox_is_success(ethervox_governor_transcribe_audio(
                        session->governor, gov_audio_accum, (uint32_t)gov_audio_accum_len,
                        &transcript)) && transcript) {
                    *text_out = transcript;
                } else {
                    free(transcript);
                }
            }
        } else {
            ethervox_stt_result_t final_result = {0};
            if (ethervox_stt_finalize(&session->stt_runtime, &final_result) == 0) {
                if (final_result.text && strlen(final_result.text) > 0) {
                    *text_out = strdup(final_result.text);
                }
                ethervox_stt_result_free(&final_result);
            }
        }
    }

    free(gov_audio_accum);
    return ETHERVOX_SUCCESS;
}

// ============================================================================
// Conversation Tool Callbacks
// ============================================================================

/**
 * @brief Callback for speak tool - handles TTS synthesis and playback
 *
 * Dispatches to one of three paths, decided once per call:
 *   1. tts_enabled == false (Mode 4, voice-to-text): no audio at all, just
 *      report the text via on_response_text for display/captions.
 *   2. config.on_speak_request set (mobile/platform-native TTS): hand the
 *      text off to the platform's TTS engine and block this thread until
 *      ethervox_conversation_notify_speaking_done() is called (mirrors the
 *      desktop Piper wait-for-playback-empty loop below, just signaled by
 *      the platform's async TTS completion instead of polling a ring buffer).
 *   3. Neither of the above (desktop, unchanged): synthesize and play
 *      through the existing Piper + AEC + CoreAudio/ALSA/etc. path.
 * This keeps the Governor and barge-in state machine platform-agnostic -
 * see conversation.h's on_speak_request doc comment for the callback shape.
 */
static int conversation_on_speak(const char* text, const char* language,
                                  bool wait_for_response, bool allow_interrupt,
                                  int speaker_id, void* user_data) {
    ethervox_conversation_session_t* session = (ethervox_conversation_session_t*)user_data;
    if (!session || !text) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOG_INFO("[Speak Tool] Synthesizing: %s (language=%s, wait=%d, interrupt=%d, speaker_id=%d)",
                      text, language ? language : "auto", wait_for_response, allow_interrupt, speaker_id);
    
    pthread_mutex_lock(&session->mutex);
    conversation_set_state(session, ETHERVOX_CONV_STATE_SPEAKING);
    pthread_mutex_unlock(&session->mutex);

    // Response text is reported for display regardless of whether it ends up
    // synthesized to audio (Mode 4 shows it but never speaks it).
    if (session->config.on_response_text) {
        session->config.on_response_text(text, language, session->config.callback_user_data);
    }

    if (!session->config.tts_enabled) {
        ETHERVOX_LOG_INFO("[Speak Tool] tts_enabled=false (Mode 4/voice-to-text), text-only");
        return ETHERVOX_SUCCESS;
    }

    // Mobile/platform-native TTS path - fire the callback, then block until
    // the platform reports completion (or we're interrupted/timed out).
    if (session->config.on_speak_request) {
        pthread_mutex_lock(&session->mutex);
        session->speaking_done = false;
        pthread_mutex_unlock(&session->mutex);

        session->config.on_speak_request(text, language, speaker_id, allow_interrupt,
                                          session->config.callback_user_data);

        pthread_mutex_lock(&session->mutex);
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += 30;  // Safety net - never hang forever if the platform
                                 // TTS listener callback is dropped/never fires
        while (!session->speaking_done && !session->interrupt_requested &&
               !session->thread_should_exit) {
            int wait_ret = pthread_cond_timedwait(&session->speaking_done_cond, &session->mutex,
                                                   &deadline);
            if (wait_ret == ETIMEDOUT) {
                ETHERVOX_LOG_WARN("[Speak Tool] Timed out waiting for platform TTS completion");
                break;
            }
        }
        pthread_mutex_unlock(&session->mutex);

        return ETHERVOX_SUCCESS;
    }
    
    // Host TTS path (all platforms via ethervox_tts_host_t)
    const char* target_language = language ? language : "en";
    
    // Print to console
    printf("\n========================================================\n");
    printf("🤖 Assistant [%s]: %s\n", target_language, text);
    printf("========================================================\n\n");
    
    // Speak via the registered TTS host
    ethervox_tts_style_t style = {
        .voice_id = NULL,  // Use system default
        .rate = 1.0f,
        .pitch = 1.0f,
        .emotion = NULL,
        .intensity = 1.0f
    };
    
    ethervox_result_t result = ethervox_tts_host_speak(text, &style, NULL /* word_cb */);
    
    if (ethervox_is_success(result)) {
        ETHERVOX_LOG_INFO("TTS host speak initiated");
        
        // Wait for speech to complete, checking for interrupts if allowed
        int poll_count = 0;
        while (ethervox_tts_host_is_speaking()) {
            // Check for interruption if allowed
            if (allow_interrupt && poll_count % 10 == 0) {
                pthread_mutex_lock(&session->mutex);
                bool should_stop = session->thread_should_exit || session->interrupt_requested;
                pthread_mutex_unlock(&session->mutex);
                
                if (should_stop) {
                    ETHERVOX_LOG_INFO("TTS interrupted by user");
                    ethervox_tts_host_stop();
                    break;
                }
            }
            
            usleep(10000);  // 10ms polling interval
            poll_count++;
        }
        ETHERVOX_LOG_DEBUG("TTS playback completed");
    } else if (result == ETHERVOX_ERROR_NO_TTS_HOST) {
        ETHERVOX_LOG_WARN("No TTS host registered, text-only mode");
    } else {
        ETHERVOX_LOG_WARN("TTS host speak failed (code=%d), using text-only mode", result);
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
    conversation_set_state(session, ETHERVOX_CONV_STATE_LISTENING);
    pthread_mutex_unlock(&session->mutex);
    
    if (prompt_hint) {
        printf("💬 %s\n", prompt_hint);
    }
    
    *user_input = NULL;
    
    // Audio is fed to Granite Speech below; finalize() produces the transcript.
    if (!session->stt_initialized) {
        ETHERVOX_LOG_WARN("STT not initialized, cannot capture audio");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Failure - no STT available
    }
    
    printf("🎤 Listening");
    fflush(stdout);

    char* text = NULL;
    capture_utterance_with_vad(session, timeout_ms, &text);

    if (text) {
        *user_input = text;  // Ownership transfers to the caller (strdup'd)
        ETHERVOX_LOG_INFO("Transcribed from listen tool: %s", *user_input);
        printf(" [OK]\n");
        if (session->config.on_user_transcript) {
            session->config.on_user_transcript(
                *user_input, session->last_detected_language[0] ? session->last_detected_language : NULL,
                session->config.callback_user_data);
        }
    } else {
        printf(" ⏱️\n");
        ETHERVOX_LOG_INFO("Listen timeout reached after %dms", timeout_ms);
    }
    
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
    
    // Check if thread should exit, an explicit barge-in was requested
    // (ethervox_conversation_interrupt()), or conversation should stop
    pthread_mutex_lock(&session->mutex);
    bool should_interrupt = session->thread_should_exit || session->interrupt_requested;
    pthread_mutex_unlock(&session->mutex);
    
    if (should_interrupt) {
        ETHERVOX_LOG_INFO("[Interrupt] Conversation interrupted");
        return ETHERVOX_SUCCESS;  // Interrupt detected
    }
    
    return ETHERVOX_ERROR_INVALID_ARGUMENT;  // No interrupt
}

// ============================================================================
// Barge-in detection primitives (pure, allocation-free) - declared in
// conversation.h specifically so they're unit-testable independent of real
// audio hardware/threading. See tests/unit/test_voice_conversation.c.
// ============================================================================

void ethervox_barge_in_detector_init(ethervox_barge_in_detector_t* detector,
                                      float energy_threshold, int min_speech_ms,
                                      int grace_period_ms) {
    if (!detector) return;
    detector->energy_threshold = energy_threshold;
    detector->min_speech_ms = min_speech_ms;
    detector->grace_period_ms = grace_period_ms;
    detector->consecutive_speech_ms = 0;
    detector->speaking_started_at_ms = 0;
}

bool ethervox_barge_in_detector_process(ethervox_barge_in_detector_t* detector,
                                         float energy, int chunk_ms,
                                         bool is_speaking, uint64_t now_ms) {
    if (!detector) return false;

    if (is_speaking) {
        if (detector->speaking_started_at_ms == 0) {
            detector->speaking_started_at_ms = now_ms;
        }
        if ((now_ms - detector->speaking_started_at_ms) < (uint64_t)detector->grace_period_ms) {
            return false;  // Still within the TTS-onset/AEC-convergence grace window
        }
    }

    if (energy > detector->energy_threshold) {
        detector->consecutive_speech_ms += chunk_ms > 0 ? chunk_ms : 1;
    } else {
        detector->consecutive_speech_ms = 0;
    }

    return detector->consecutive_speech_ms >= detector->min_speech_ms;
}

void ethervox_preroll_ring_write(float* ring, size_t capacity, size_t* write_pos,
                                  size_t* filled, const float* samples, size_t count) {
    if (!ring || capacity == 0 || !write_pos || !filled || !samples) return;

    for (size_t i = 0; i < count; i++) {
        ring[*write_pos] = samples[i];
        *write_pos = (*write_pos + 1) % capacity;
        if (*filled < capacity) {
            (*filled)++;
        }
    }
}

size_t ethervox_preroll_ring_snapshot(const float* ring, size_t capacity, size_t write_pos,
                                       size_t filled, float* out, size_t out_capacity) {
    if (!ring || capacity == 0 || !out || out_capacity == 0) return 0;

    size_t samples = filled < out_capacity ? filled : out_capacity;
    if (samples == 0) return 0;

    // Chronological (oldest-first) read-out: the oldest retained sample is
    // `samples` slots behind the current write position.
    size_t start = (write_pos + capacity - samples) % capacity;
    for (size_t i = 0; i < samples; i++) {
        out[i] = ring[(start + i) % capacity];
    }
    return samples;
}

// ============================================================================
// Barge-in monitor (VAD-based interrupt during THINKING/SPEAKING)
// ============================================================================

/**
 * @brief Background thread: polls the continuously-open mic for sustained
 * speech energy while the Governor is generating (THINKING) or platform TTS
 * is playing (SPEAKING) - both block conversation_thread() directly via a
 * synchronous call, so this is the only way to notice new user speech
 * during those states. See conversation.h's barge_in_* config fields and
 * plan.md Open Question 1 for the full design rationale.
 *
 * Requires config.barge_in_enabled and that the caller has NOT stopped
 * audio capture between LISTENING and this turn's PROCESSING/SPEAKING (see
 * conversation_thread()'s capture-lifecycle note) - this thread does not
 * start or stop the mic itself, only reads from it.
 *
 * The actual trigger decision (hysteresis + grace period) is delegated to
 * ethervox_barge_in_detector_process() and the pre-roll ring bookkeeping to
 * ethervox_preroll_ring_write()/ethervox_preroll_ring_snapshot() - both pure
 * functions unit-tested directly in test_voice_conversation.c. This thread
 * is just the (untestable-without-hardware) glue: mutex-guarded state
 * reads, blocking audio I/O, and calling ethervox_conversation_interrupt()
 * on a committed trigger.
 *
 * On a committed trigger, snapshots the pre-roll ring buffer into
 * session->pending_preroll_audio and calls the existing
 * ethervox_conversation_interrupt() - the exact same path as the manual
 * "tap to interrupt" UI action - so barge-in is additive to, not a
 * reimplementation of, that mechanism.
 */
static void* run_barge_in_monitor(void* arg) {
    ethervox_conversation_session_t* session = (ethervox_conversation_session_t*)arg;

    const uint32_t sample_rate =
        session->audio_runtime.config.sample_rate > 0 ? session->audio_runtime.config.sample_rate : 16000;

    ethervox_barge_in_detector_t detector;
    ethervox_barge_in_detector_init(&detector, session->config.barge_in_energy_threshold,
                                     session->config.barge_in_min_speech_ms,
                                     session->config.barge_in_grace_period_ms);

    for (;;) {
        pthread_mutex_lock(&session->mutex);
        bool stop = session->barge_in_stop_requested || session->thread_should_exit ||
                    session->interrupt_requested;
        ethervox_conversation_state_t current_state = session->state;
        pthread_mutex_unlock(&session->mutex);
        if (stop) {
            break;
        }

        ethervox_audio_buffer_t chunk = {0};
        chunk.data = (float*)malloc(1024 * sizeof(float));
        chunk.size = chunk.data ? 1024 : 0;
        chunk.channels = 1;
        if (!chunk.data || ethervox_is_error(ethervox_audio_read(&session->audio_runtime, &chunk)) ||
            chunk.size == 0) {
            free(chunk.data);
            usleep(20000);  // ~20ms - avoid a tight spin when nothing is buffered yet
            continue;
        }

        // Always maintain the pre-roll ring, even during the grace period,
        // so a barge-in right at the edge of the grace window still has its
        // leading syllables available.
        if (session->preroll_ring && session->preroll_ring_capacity > 0) {
            pthread_mutex_lock(&session->mutex);
            ethervox_preroll_ring_write(session->preroll_ring, session->preroll_ring_capacity,
                                        &session->preroll_ring_write_pos,
                                        &session->preroll_ring_filled, chunk.data, chunk.size);
            pthread_mutex_unlock(&session->mutex);
        }

        bool is_speaking = (current_state == ETHERVOX_CONV_STATE_SPEAKING);
        float energy = ethervox_audio_calculate_rms_energy(chunk.data, chunk.size);
        int chunk_ms = (int)((uint64_t)chunk.size * 1000ULL / sample_rate);
        free(chunk.data);

        bool triggered =
            ethervox_barge_in_detector_process(&detector, energy, chunk_ms, is_speaking, get_time_ms());

        if (triggered) {
            ETHERVOX_LOG_INFO("[Barge-in] Sustained speech detected (%dms) during %s - interrupting",
                              detector.consecutive_speech_ms, is_speaking ? "SPEAKING" : "THINKING");

            pthread_mutex_lock(&session->mutex);
            size_t capacity = session->preroll_ring_filled;
            free(session->pending_preroll_audio);
            session->pending_preroll_audio = capacity > 0 ? (float*)malloc(capacity * sizeof(float)) : NULL;
            if (session->pending_preroll_audio) {
                session->pending_preroll_samples = ethervox_preroll_ring_snapshot(
                    session->preroll_ring, session->preroll_ring_capacity, session->preroll_ring_write_pos,
                    session->preroll_ring_filled, session->pending_preroll_audio, capacity);
            } else {
                session->pending_preroll_samples = 0;
            }
            pthread_mutex_unlock(&session->mutex);

            ethervox_conversation_interrupt(session);
            break;
        }
    }

    return NULL;
}


/**
 * @brief Start the barge-in monitor thread for this turn (THINKING and/or
 * SPEAKING). No-op if barge-in is disabled or a monitor is already running.
 * Must be paired with stop_barge_in_monitor() before the next
 * capture_utterance_with_vad() call - see that function's caller in
 * conversation_thread().
 */
static void start_barge_in_monitor(ethervox_conversation_session_t* session) {
    if (!session->config.barge_in_enabled || session->barge_in_thread_active) {
        return;
    }

    pthread_mutex_lock(&session->mutex);
    session->barge_in_stop_requested = false;
    session->preroll_ring_write_pos = 0;
    session->preroll_ring_filled = 0;
    pthread_mutex_unlock(&session->mutex);

    if (pthread_create(&session->barge_in_thread, NULL, run_barge_in_monitor, session) == 0) {
        session->barge_in_thread_active = true;
    } else {
        ETHERVOX_LOG_WARN("[Barge-in] Failed to start monitor thread - falling back to "
                           "tap-to-interrupt only for this turn");
    }
}

/**
 * @brief Stop and join the barge-in monitor thread if one is running. Safe
 * to call even if barge-in is disabled or the monitor already self-
 * terminated (e.g. because it just triggered a barge-in).
 */
static void stop_barge_in_monitor(ethervox_conversation_session_t* session) {
    if (!session->barge_in_thread_active) {
        return;
    }

    pthread_mutex_lock(&session->mutex);
    session->barge_in_stop_requested = true;
    pthread_mutex_unlock(&session->mutex);

    pthread_join(session->barge_in_thread, NULL);
    session->barge_in_thread_active = false;
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
        // Reused as the barge-in capability signal for the Android AAudio
        // driver: true switches its input stream to
        // AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION for platform AEC/NS/AGC
        // (see platform_android.c); false (default) keeps
        // AAUDIO_INPUT_PRESET_VOICE_RECOGNITION, today's unchanged behavior.
        // Desktop's real Speex AEC (session->aec_context, set up separately
        // below) is unaffected either way.
        audio_config.enable_echo_cancellation = session->config.barge_in_enabled;
        
        if (ethervox_audio_register_platform_driver(&session->audio_runtime) == 0 &&
            session->audio_runtime.driver.init(&session->audio_runtime, &audio_config) == 0) {
            session->audio_initialized = true;
            printf("[OK] Microphone ready\n");
        } else {
            printf("❌ Failed to initialize microphone\n");
            pthread_mutex_lock(&session->mutex);
            conversation_set_state(session, ETHERVOX_CONV_STATE_ERROR);
            pthread_mutex_unlock(&session->mutex);
            return NULL;
        }
    }
    
    if (!session->stt_initialized && !ethervox_governor_has_audio_support(session->governor)) {
        // Unified voice model architecture: when the Governor was loaded
        // with audio support (see docs/UNIFIED_VOICE_MODEL_ARCHITECTURE.md),
        // it decodes ASR directly on its own llama_context - no separate
        // STT-runtime instance is initialized here at all.
        printf("🗣️  Initializing speech recognition (Granite Speech)...\n");
        ethervox_stt_config_t stt_config = ethervox_stt_get_default_config();
        stt_config.sample_rate = 16000;
        stt_config.enable_partial_results = false;  // Granite Speech is one-shot per utterance, no partials

        // Mode 1 (voice conversation) always uses the BASE variant - punctuated,
        // capitalized ASR, never speaker-attributed (that's Mode 2/PLUS only).
        stt_config.backend = ETHERVOX_STT_BACKEND_GRANITE_SPEECH;

        // Prefer the model/mmproj paths the caller resolved and passed in via
        // ethervox_conversation_config_t.stt (e.g. Android JNI resolves these
        // against the app's files dir, since getenv("HOME") is not a usable
        // model location on Android/iOS). Only fall back to the desktop
        // convenience default (~/.ethervox/models/granite-speech/) when the
        // caller left both NULL, matching this field's documented behavior
        // in conversation.h.
        static char granite_speech_model_path[512];
        static char granite_speech_mmproj_path[512];
        if (session->config.stt.model_path && session->config.stt.mmproj_path) {
            stt_config.model_path = session->config.stt.model_path;
            stt_config.mmproj_path = session->config.stt.mmproj_path;
        } else {
            const char* home = getenv("HOME");
            if (home) {
                snprintf(granite_speech_model_path, sizeof(granite_speech_model_path),
                         "%s/.ethervox/models/granite-speech/granite-speech-4.1-2b.Q4_K_M.gguf", home);
                snprintf(granite_speech_mmproj_path, sizeof(granite_speech_mmproj_path),
                         "%s/.ethervox/models/granite-speech/mmproj-granite-speech-4.1-2b-Q4_K_M.gguf", home);
                stt_config.model_path = granite_speech_model_path;
                stt_config.mmproj_path = granite_speech_mmproj_path;
            }
        }
        
        if (ethervox_stt_init(&session->stt_runtime, &stt_config) == 0) {
            session->stt_initialized = true;
            printf("[OK] Speech recognition ready\n");
        } else {
            printf("❌ Failed to initialize speech recognition\n");
            pthread_mutex_lock(&session->mutex);
            conversation_set_state(session, ETHERVOX_CONV_STATE_ERROR);
            pthread_mutex_unlock(&session->mutex);
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
            conversation_set_state(session, ETHERVOX_CONV_STATE_LISTENING);
        } else {
            // Wake word mode: wait for trigger
            conversation_set_state(session, ETHERVOX_CONV_STATE_IDLE);
            
            while (!session->thread_should_exit && session->state == ETHERVOX_CONV_STATE_IDLE) {
                pthread_cond_wait(&session->trigger_cond, &session->mutex);
            }
            
            if (session->thread_should_exit) {
                pthread_mutex_unlock(&session->mutex);
                break;
            }
            
            conversation_set_state(session, ETHERVOX_CONV_STATE_LISTENING);
            printf("\n🎤 Wake word detected, listening...\n");
        }
        
        // Reset per-turn interrupt flag now that we're about to start a new
        // listening turn; a barge-in interrupt from the *previous* turn must
        // not leak into this one.
        session->interrupt_requested = false;
        
        pthread_mutex_unlock(&session->mutex);
        
        // Listen for one utterance, using the shared RMS-energy VAD +
        // Granite Speech finalize() helper (see capture_utterance_with_vad's
        // doc comment for why the old per-chunk is_final-trusting loop that
        // used to live here never actually worked with Granite Speech).
        printf("🎤 Listening");
        fflush(stdout);

        char* recognized_text = NULL;

        // Start STT and audio capture. Skipped when the Governor itself has
        // audio decode support (unified voice model architecture) - there is
        // no separate stt_runtime to start in that case; see
        // capture_utterance_with_vad()'s use_governor_audio branch.
        bool has_governor_audio = ethervox_governor_has_audio_support(session->governor);
        if (!has_governor_audio && ethervox_stt_start(&session->stt_runtime) != 0) {
            ETHERVOX_LOG_ERROR("Failed to start STT");
            conversation_notify_error(session, "Failed to start speech recognition");
            pthread_mutex_lock(&session->mutex);
            conversation_set_state(session, ETHERVOX_CONV_STATE_IDLE);
            pthread_mutex_unlock(&session->mutex);
            usleep(100000);
            continue;
        }
        
        if (ethervox_audio_start_capture(&session->audio_runtime) != 0) {
            ETHERVOX_LOG_ERROR("Failed to start audio capture");
            conversation_notify_error(session, "Failed to start microphone capture");
            if (!has_governor_audio) {
                ethervox_stt_stop(&session->stt_runtime);
            }
            pthread_mutex_lock(&session->mutex);
            conversation_set_state(session, ETHERVOX_CONV_STATE_IDLE);
            pthread_mutex_unlock(&session->mutex);
            usleep(100000);
            continue;
        }

        // Barge-in support: when config.barge_in_enabled, keep the mic
        // stream open continuously through PROCESSING/SPEAKING instead of
        // stopping it here - run_barge_in_monitor() (started below, right
        // before the Governor call) polls it concurrently with Governor
        // generation and TTS playback, both of which block this thread via
        // a synchronous call and can't watch the mic themselves. See
        // conversation.h's barge_in_* config fields and plan.md Open
        // Question 1. When barge-in is disabled (default, or on a device
        // without real platform AEC - see platform_android.c's
        // AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION wiring), capture stops
        // here exactly as before; tap-to-interrupt remains the sole
        // barge-in mechanism in that case. ethervox_audio_start_capture()
        // above is the is_capturing-guarded wrapper (not the raw
        // driver.start_capture), since with barge-in enabled this call
        // re-runs every turn on a stream that may already be open.
        capture_utterance_with_vad(session, 30000, &recognized_text);

        if (!has_governor_audio) {
            ethervox_stt_stop(&session->stt_runtime);
        }
        if (!session->config.barge_in_enabled) {
            ethervox_audio_stop_capture(&session->audio_runtime);
        }
        
        pthread_mutex_lock(&session->mutex);
        
        if (!recognized_text || recognized_text[0] == '\0') {
            free(recognized_text);
            // No speech detected in always-listening mode - keep looping
            if (always_listening) {
                conversation_set_state(session, ETHERVOX_CONV_STATE_LISTENING);
                pthread_mutex_unlock(&session->mutex);
                usleep(100000);  // 100ms sleep to avoid tight loop
                continue;
            }
            // In wake word mode, return to idle
            conversation_set_state(session, ETHERVOX_CONV_STATE_IDLE);
            pthread_mutex_unlock(&session->mutex);
            continue;
        }
        
        // Speech detected - process with Governor
        conversation_set_state(session, ETHERVOX_CONV_STATE_PROCESSING);
        pthread_mutex_unlock(&session->mutex);
        
        printf("\n👤 User: %s\n", recognized_text);
        ETHERVOX_LOG_INFO("Processing user input with Governor: %s", recognized_text);

        if (session->config.on_user_transcript) {
            session->config.on_user_transcript(
                recognized_text,
                session->last_detected_language[0] ? session->last_detected_language : NULL,
                session->config.callback_user_data);
        }
        
        // Barge-in monitor spans this Governor call (THINKING) and, from
        // inside it, conversation_on_speak()'s TTS wait (SPEAKING) - both
        // are synchronous from this thread's point of view, so the monitor
        // is what actually notices new speech during either state. No-op
        // when config.barge_in_enabled is false.
        start_barge_in_monitor(session);

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
                &exec_context, NULL, &llm_response,
                &error_msg,
                NULL,  // metrics (optional)
                NULL,  // progress callback (optional)
                NULL,  // event callback (optional)
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
                // Barge-in (VAD-triggered or manual tap-to-interrupt) -
                // return to LISTENING/IDLE below like any other completed
                // turn instead of ending the whole session (previously this
                // `break`d out of conversation_thread entirely, contradicting
                // interruptVoiceConversation()'s documented "returns to
                // LISTENING without ending it" contract on the Kotlin side).
                // Any pre-roll audio the monitor captured is picked up by
                // the next capture_utterance_with_vad() call automatically.
                ETHERVOX_LOG_INFO("Conversation interrupted by user (barge-in)");
                if (llm_response) free(llm_response);
                if (error_msg) free(error_msg);
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

        // Must be stopped/joined before the next capture_utterance_with_vad()
        // call - both would otherwise read the mic concurrently.
        stop_barge_in_monitor(session);
        
        pthread_mutex_lock(&session->mutex);
        
        // Return to appropriate state based on mode
        if (always_listening) {
            // In always-listening mode, immediately go back to listening
            conversation_set_state(session, ETHERVOX_CONV_STATE_LISTENING);
            pthread_mutex_unlock(&session->mutex);
            ETHERVOX_LOG_DEBUG("Continuing in always-listening mode...");
        } else {
            // In wake word mode, return to idle and wait for next trigger
            conversation_set_state(session, ETHERVOX_CONV_STATE_IDLE);
            pthread_mutex_unlock(&session->mutex);
        }

        free(recognized_text);
    }
    
    pthread_mutex_lock(&session->mutex);
    conversation_set_state(session, ETHERVOX_CONV_STATE_UNINITIALIZED);
    session->thread_running = false;
    pthread_mutex_unlock(&session->mutex);

    // With barge_in_enabled, capture is left open across turns (see the
    // per-turn stop_capture guard above) - make sure it's actually released
    // when the session ends, not just when barge-in is disabled.
    if (session->config.barge_in_enabled) {
        ethervox_audio_stop_capture(&session->audio_runtime);
    }
    
    return NULL;
}

ethervox_conversation_config_t ethervox_conversation_get_default_config(void) {
    ethervox_conversation_config_t config = {0};
    
    // Granite Speech ASR configuration (BASE variant - Mode 1 conversation)
    config.stt.model_path = NULL; // Will auto-detect in ~/.ethervox/models/granite-speech/
    config.stt.mmproj_path = NULL; // Will auto-detect alongside model_path
    config.stt.sample_rate = 16000;
    
    // Piper configuration (desktop only - see ethervox_piper_config_t doc comment)
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
    
    // TTS is enabled by default (Mode 1 conversation). Mode 4 (voice-to-text)
    // callers should explicitly set this to false so conversation_on_speak()
    // skips synthesis entirely and just reports response text via
    // on_response_text.
    config.tts_enabled = true;

    // Barge-in defaults to OFF - see conversation.h's barge_in_enabled doc
    // comment. Callers must explicitly enable it after checking real
    // platform AEC availability; the numeric defaults below are best-effort
    // starting points only (plan.md Open Question 1 - "needs on-device
    // testing"), not validated against real hardware.
    config.barge_in_enabled = false;
    config.barge_in_energy_threshold = 0.05f;
    config.barge_in_min_speech_ms = 300;
    config.barge_in_grace_period_ms = 400;
    config.barge_in_preroll_ms = 500;
    
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
    pthread_cond_init(&session->speaking_done_cond, NULL);
    session->thread_running = false;
    session->thread_should_exit = false;
    session->interrupt_requested = false;
    session->speaking_done = false;
    session->state = ETHERVOX_CONV_STATE_UNINITIALIZED;
    
    // Initialize STT and audio
    session->stt_initialized = false;
    memset(&session->stt_runtime, 0, sizeof(ethervox_stt_runtime_t));
    session->audio_initialized = false;
    memset(&session->audio_runtime, 0, sizeof(ethervox_audio_runtime_t));
    session->audio_buffer = NULL;
    session->audio_capture_active = false;

    // Barge-in monitor state + pre-roll ring buffer (see conversation.h's
    // barge_in_* config fields). Sized from barge_in_preroll_ms at 16kHz
    // mono, matching the fixed capture sample rate used throughout this
    // file. Allocated unconditionally (cheap, a few hundred KB at most) so
    // toggling barge_in_enabled doesn't require a session re-init.
    session->barge_in_thread_active = false;
    session->barge_in_stop_requested = false;
    int preroll_ms = config->barge_in_preroll_ms > 0 ? config->barge_in_preroll_ms : 500;
    session->preroll_ring_capacity = (size_t)(16000 * preroll_ms / 1000);
    session->preroll_ring = (float*)calloc(session->preroll_ring_capacity, sizeof(float));
    session->preroll_ring_write_pos = 0;
    session->preroll_ring_filled = 0;
    session->pending_preroll_audio = NULL;
    session->pending_preroll_samples = 0;
    if (!session->preroll_ring) {
        ETHERVOX_LOG_WARN("Failed to allocate barge-in pre-roll buffer - "
                           "barge-in will still interrupt but without pre-roll audio");
        session->preroll_ring_capacity = 0;
    }
    
    // Always-listening mode
    session->always_listening = config->always_listening;
    session->pending_transcription = NULL;
    
    // Language detection
    session->last_detected_language[0] = '\0';  // Initialize to empty (use fallback detection)
    
    // TTS is now handled via ethervox_tts_host_t (no per-session initialization)
    // The host must be registered separately via ethervox_tts_set_host()
    
    // Initialize AEC if enabled
    session->aec_initialized = false;
    session->aec_context = NULL;
    
    // Load settings for AEC configuration
    ethervox_persistent_settings_t settings;
    bool settings_loaded = ethervox_is_success(ethervox_settings_load(&settings, NULL));
    
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
    
    ETHERVOX_LOG_INFO("Conversation session initialized (always_listening=%d, AEC=%d)",
                      session->always_listening, session->aec_initialized);
    
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
        conversation_set_state(session, ETHERVOX_CONV_STATE_LISTENING);
        pthread_cond_signal(&session->trigger_cond);
        ETHERVOX_LOG_DEBUG("Conversation triggered by wake word");
    } else {
        ETHERVOX_LOG_DEBUG("Conversation trigger ignored (already active)");
    }
    
    pthread_mutex_unlock(&session->mutex);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_conversation_interrupt(ethervox_conversation_session_t* session) {
    if (!session) {
        ETHERVOX_LOG_ERROR("conversation_interrupt: NULL session");
        return -EINVAL;
    }
    
    pthread_mutex_lock(&session->mutex);
    
    if (!session->thread_running) {
        pthread_mutex_unlock(&session->mutex);
        ETHERVOX_LOG_WARN("conversation_interrupt: thread not running");
        return -EINVAL;
    }
    
    session->interrupt_requested = true;
    
    // Cancel any in-flight Governor generation (Mode 1/4 barge-in during
    // THINKING); this is a no-op if the Governor isn't currently generating.
    if (session->governor) {
        ethervox_governor_request_interrupt(session->governor);
    }
    
    // Wake anything blocked on speaking_done_cond (mobile TTS wait in
    // conversation_on_speak()) and trigger_cond (wake-word wait) so the
    // interrupt takes effect immediately instead of waiting for a timeout.
    pthread_cond_broadcast(&session->speaking_done_cond);
    pthread_cond_broadcast(&session->trigger_cond);
    
    ETHERVOX_LOG_INFO("Conversation interrupted (barge-in)");
    
    pthread_mutex_unlock(&session->mutex);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_conversation_notify_speaking_done(ethervox_conversation_session_t* session) {
    if (!session) {
        ETHERVOX_LOG_ERROR("conversation_notify_speaking_done: NULL session");
        return -EINVAL;
    }
    
    pthread_mutex_lock(&session->mutex);
    session->speaking_done = true;
    pthread_cond_broadcast(&session->speaking_done_cond);
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
    
    // TTS is now handled via ethervox_tts_host_t (no per-session cleanup)
    
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

    // Free barge-in pre-roll buffers (the monitor thread itself is always
    // stopped/joined by ethervox_conversation_stop() above before we get here)
    free(session->preroll_ring);
    session->preroll_ring = NULL;
    free(session->pending_preroll_audio);
    session->pending_preroll_audio = NULL;
    
    // Destroy threading primitives
    pthread_mutex_destroy(&session->mutex);
    pthread_cond_destroy(&session->trigger_cond);
    pthread_cond_destroy(&session->speaking_done_cond);
    
    free(session);
    
    ETHERVOX_LOG_INFO("Conversation session cleaned up");
}

// Legacy functions - no longer used with ethervox_tts_host_t but kept for API compatibility
void* ethervox_conversation_get_phonemizer(ethervox_conversation_session_t* session) {
    (void)session;
    ETHERVOX_LOG_WARN("get_phonemizer: TTS is now handled via ethervox_tts_host_t (no phonemizer)");
    return NULL;
}

void* ethervox_conversation_get_tts(ethervox_conversation_session_t* session) {
    (void)session;
    ETHERVOX_LOG_WARN("get_tts: TTS is now handled via ethervox_tts_host_t (no session TTS context)");
    return NULL;
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
