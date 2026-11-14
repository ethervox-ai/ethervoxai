/**
 * @file conversation.h
 * @brief Real-time voice conversation system with Granite Speech ASR and
 *        platform-native TTS (desktop: Piper; Android/iOS: OS TTS)
 *
 * Provides lightweight voice interaction for LLM conversations, triggered by
 * wake word detection. Separate from transcription pipeline (also Granite
 * Speech-based, PLUS variant) which is for meeting notes and long-form
 * dictation with speaker attribution.
 *
 * Architecture:
 * - Wake word detection → Signal conversation thread
 * - Granite Speech BASE ASR (one-shot per utterance) → Process speech
 * - Send to Governor → Get response
 * - TTS (desktop: Piper, mobile: platform-native) → Speak response
 * - Return to wake word listening
 *
 * Thread model: Background thread waits on condition variable, processes
 * conversation when signaled, returns to idle after timeout.
 */

#ifndef ETHERVOX_CONVERSATION_H
#define ETHERVOX_CONVERSATION_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations to avoid circular dependencies
typedef struct ethervox_governor ethervox_governor_t;

/**
 * @brief Conversation session state
 */
typedef enum {
    ETHERVOX_CONV_STATE_UNINITIALIZED = 0,
    ETHERVOX_CONV_STATE_IDLE,          // Waiting for wake word trigger
    ETHERVOX_CONV_STATE_LISTENING,     // Capturing user speech
    ETHERVOX_CONV_STATE_PROCESSING,    // Sending to Governor
    ETHERVOX_CONV_STATE_SPEAKING,      // Playing TTS response
    ETHERVOX_CONV_STATE_ERROR
} ethervox_conversation_state_t;

/**
 * @brief Granite Speech ASR configuration (subset relevant to Mode 1
 * conversation; see ethervox/stt.h's ethervox_stt_config_t for the full,
 * authoritative config used to actually initialize the backend - these
 * fields exist for API-level documentation/defaults only)
 */
typedef struct {
    const char* model_path;            // Path to Granite Speech GGUF (BASE variant)
    const char* mmproj_path;           // Path to companion mmproj GGUF (audio projector)
    uint32_t sample_rate;              // Audio sample rate (16000 Hz, fixed by the encoder)
} ethervox_granite_speech_config_t;

/**
 * @brief Piper TTS configuration (desktop only - Android/iOS use platform-
 * native TTS, which has no equivalent model-path config; see
 * src/dialogue/voice_conversation.c's conversation_on_speak)
 */
typedef struct {
    const char* model_path;            // Path to Piper .onnx model
    const char* config_path;           // Path to model config JSON
    float speed;                       // Speech speed multiplier (1.0 = normal)
    int sample_rate;                   // Output sample rate (22050 Hz typical)
} ethervox_piper_config_t;

/**
 * @brief Platform-native TTS callback (Android TextToSpeech / iOS
 * AVSpeechSynthesizer). Set ethervox_conversation_config_t.on_speak_request
 * to receive the assistant's utterances instead of the desktop Piper path -
 * see conversation_on_speak in voice_conversation.c. The callback is
 * fire-and-forget: it should hand `text` off to the platform TTS engine and
 * return immediately (mobile TTS engines report completion asynchronously
 * via their own listener, not by blocking this call) and honors
 * `allow_interrupt` on its own (i.e. keep listening for a barge-in tap while
 * speaking). `speaker_id` is the Governor's emotion-derived hint (see
 * src/plugins/conversation_tools/speak.c) - platform TTS APIs generally
 * don't support per-utterance speaker/voice switching, so implementations
 * are free to ignore it.
 */
typedef void (*ethervox_conversation_speak_cb)(const char* text, const char* language,
                                                int speaker_id, bool allow_interrupt,
                                                void* user_data);

/**
 * @brief State-change notification callback - fired every time the session
 * transitions between IDLE/LISTENING/PROCESSING/SPEAKING/ERROR states, so a
 * UI layer doesn't have to poll ethervox_conversation_get_state().
 */
typedef void (*ethervox_conversation_state_cb)(ethervox_conversation_state_t state,
                                                void* user_data);

/**
 * @brief Fired once per finalized user utterance (after Granite Speech
 * completes transcription, before the Governor is invoked) - lets a UI show
 * the user's own recognized text (e.g. as a chat bubble / live caption).
 */
typedef void (*ethervox_conversation_transcript_cb)(const char* text, const char* language,
                                                     void* user_data);

/**
 * @brief Fired once per assistant utterance the Governor produced via the
 * `speak` tool, independent of whether it is actually synthesized to audio.
 * Always fires (even when tts_enabled is false, i.e. Mode 4 voice-to-text),
 * so a UI can show/caption the response text; on_speak_request is the
 * separate, TTS-gated hook that additionally triggers audio playback.
 */
typedef void (*ethervox_conversation_response_cb)(const char* text, const char* language,
                                                   void* user_data);

/**
 * @brief Fired on unrecoverable session errors (STT/audio init failures,
 * Governor errors) - complements the ETHERVOX_CONV_STATE_ERROR state with a
 * human-readable message for UI display.
 */
typedef void (*ethervox_conversation_error_cb)(const char* message, void* user_data);

/**
 * @brief Conversation session configuration
 */
typedef struct {
    ethervox_granite_speech_config_t stt;  // ASR configuration (BASE variant)
    ethervox_piper_config_t piper;         // TTS configuration (desktop only)
    
    int listen_timeout_ms;             // Silence timeout to stop listening
    int conversation_timeout_ms;       // Max conversation duration
    int audio_buffer_size;             // Size of audio ring buffer
    
    bool enable_beep_on_wake;          // Play feedback when wake word detected
    bool enable_beep_on_listen_end;    // Play feedback when listening ends
    bool always_listening;             // Continuously transcribe without wake word - this is
                                        // what Mode 1/4 (manual "Talk"/"Voice query" button, no
                                        // wake-word gating) sets on every platform, not just desktop;
                                        // the get_default_config() platform ifdef only controls the
                                        // *default*, callers are expected to override it explicitly.

    // Mode 4 (voice-to-text): set false to suppress all speech output while
    // still running the full ASR -> Governor pipeline; the response is only
    // ever delivered via on_user_transcript/on_speak_request's text
    // parameter for display, never synthesized. Defaults to true.
    bool tts_enabled;

    // --- Barge-in (VAD-based interrupt during THINKING/SPEAKING) ---
    // See plan.md Open Question 1 for the full design rationale. This is
    // additive to, not a replacement for, the explicit
    // ethervox_conversation_interrupt() call (manual "tap to interrupt" UI
    // action) - that path keeps working unconditionally regardless of these
    // settings, since VAD reliability depends on echo cancellation quality
    // that varies by OEM/device and can never be guaranteed.

    // Master switch. Defaults to false (see
    // ethervox_conversation_get_default_config()) - the caller (JNI/platform
    // bridge layer) is expected to set this only after an actual platform
    // AEC capability check (e.g. Android's
    // android.media.audiofx.AcousticEchoCanceler.isAvailable()), not
    // unconditionally, since a device with no real echo cancellation will
    // produce constant false triggers from its own TTS output. When true,
    // this also switches the Android capture stream to
    // AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION (see
    // ethervox_audio_config_t.enable_echo_cancellation and
    // src/audio/platform_android.c) to get platform AEC/NS/AGC in the first
    // place - without it, sensitivity tuning alone can't compensate for a
    // raw, uncancelled echo signal.
    bool barge_in_enabled;

    // RMS energy threshold above which incoming audio is treated as
    // "possible speech" while THINKING/SPEAKING. Deliberately higher than
    // capture_utterance_with_vad's plain-listening threshold (0.02f, see
    // voice_conversation.c) because this must reject AEC residual/echo
    // bleed from the device's own TTS output, not just room noise. Default
    // 0.05f - a starting point requiring on-device tuning, not a validated
    // value (see plan.md Open Question 1).
    float barge_in_energy_threshold;

    // Hysteresis: consecutive milliseconds of energy above
    // barge_in_energy_threshold required before a barge-in is committed.
    // Rejects single-frame transients (echo spikes, clicks/pops, coughs).
    // Default 300ms.
    int barge_in_min_speech_ms;

    // Grace period (ms) after the SPEAKING state begins during which
    // barge-in detection is suppressed entirely - covers the TTS onset
    // transient and gives platform AEC time to converge. Not applied during
    // THINKING (no audio is being output yet, so there is nothing
    // acoustically to guard against). Default 400ms.
    int barge_in_grace_period_ms;

    // How many milliseconds of audio immediately preceding a committed
    // barge-in to retain and hand to the next listening turn, so the user's
    // interrupting speech isn't clipped at the start. Default 500ms.
    int barge_in_preroll_ms;

    // Platform-native TTS + UI notification hooks (all optional, NULL = no-op
    // for the notification callbacks; on_speak_request NULL means "use the
    // desktop Piper path" - see conversation_on_speak). Mobile platforms
    // (Android/iOS JNI/bridge layers) are expected to set at least
    // on_speak_request; desktop callers may leave all four NULL to get the
    // original Piper + console-only behavior unchanged.
    ethervox_conversation_speak_cb on_speak_request;
    ethervox_conversation_state_cb on_state_change;
    ethervox_conversation_transcript_cb on_user_transcript;
    ethervox_conversation_response_cb on_response_text;
    ethervox_conversation_error_cb on_error;
    void* callback_user_data;           // Passed through to all four callbacks above
} ethervox_conversation_config_t;

/**
 * @brief Opaque conversation session runtime
 */
typedef struct ethervox_conversation_session ethervox_conversation_session_t;

// ============================================================================
// Barge-in detection primitives (pure, allocation-free, no threading/audio
// I/O) - extracted out of the internal monitor thread specifically so the
// hysteresis/grace-period decision logic and pre-roll ring-buffer math can
// be unit tested directly with synthetic inputs, independent of real audio
// hardware or thread timing. See src/dialogue/voice_conversation.c's
// run_barge_in_monitor() for the only production caller, and
// tests/unit/test_voice_conversation.c for the unit tests exercising these
// directly.
// ============================================================================

/**
 * @brief Hysteresis + grace-period state for one barge-in monitoring turn.
 * Reset (via ethervox_barge_in_detector_init) at the start of each
 * THINKING/SPEAKING span.
 */
typedef struct {
    float energy_threshold;
    int min_speech_ms;
    int grace_period_ms;
    int consecutive_speech_ms;       // Running hysteresis counter
    uint64_t speaking_started_at_ms; // 0 == SPEAKING not yet observed this turn
} ethervox_barge_in_detector_t;

/**
 * @brief Initialize/reset a detector for a new THINKING/SPEAKING turn.
 */
void ethervox_barge_in_detector_init(ethervox_barge_in_detector_t* detector,
                                      float energy_threshold, int min_speech_ms,
                                      int grace_period_ms);

/**
 * @brief Feed one audio chunk's measured RMS energy through the detector.
 *
 * @param detector State to update in place.
 * @param energy RMS energy of this chunk (see ethervox_audio_calculate_rms_energy).
 * @param chunk_ms Duration of this chunk in milliseconds.
 * @param is_speaking Whether the conversation state is currently SPEAKING (TTS
 *   playing). THINKING callers pass false - there is no output audio to
 *   guard against, so no grace period applies and detection is immediate.
 * @param now_ms Monotonic timestamp of this chunk (e.g. get_time_ms()); only
 *   used to time the grace period once is_speaking first becomes true.
 * @return true exactly once - on the chunk that pushes consecutive
 *   above-threshold time past min_speech_ms. The caller should act on the
 *   trigger and stop feeding this detector for the turn (it is not reset
 *   automatically after triggering).
 */
bool ethervox_barge_in_detector_process(ethervox_barge_in_detector_t* detector,
                                         float energy, int chunk_ms,
                                         bool is_speaking, uint64_t now_ms);

/**
 * @brief Write samples into a caller-owned ring buffer, advancing
 * *write_pos and *filled in place. Pure index/array math - no allocation,
 * no session/thread dependency.
 *
 * @param ring Ring buffer storage, capacity elements.
 * @param capacity Total capacity of ring, in samples.
 * @param write_pos In/out: next write index, wrapped into [0, capacity).
 * @param filled In/out: samples written so far, capped at capacity.
 * @param samples Samples to write.
 * @param count Number of samples to write.
 */
void ethervox_preroll_ring_write(float* ring, size_t capacity, size_t* write_pos,
                                  size_t* filled, const float* samples, size_t count);

/**
 * @brief Read out the ring buffer's contents in chronological order (oldest
 * first) into a caller-owned output buffer.
 *
 * @param ring Ring buffer storage, capacity elements.
 * @param capacity Total capacity of ring, in samples.
 * @param write_pos Current write index (as maintained by ethervox_preroll_ring_write).
 * @param filled Samples currently valid in ring (as maintained by ethervox_preroll_ring_write).
 * @param out Output buffer, at least out_capacity elements.
 * @param out_capacity Capacity of out.
 * @return Number of samples written to out (min(filled, out_capacity)).
 */
size_t ethervox_preroll_ring_snapshot(const float* ring, size_t capacity, size_t write_pos,
                                       size_t filled, float* out, size_t out_capacity);

/**
 * @brief Get default conversation configuration
 * 
 * @return Default configuration with recommended settings
 */
ethervox_conversation_config_t ethervox_conversation_get_default_config(void);

/**
 * @brief Initialize conversation session
 * 
 * Creates background thread that waits for wake word triggers. Thread
 * lifecycle: idle → triggered → listening → processing → speaking → idle.
 * 
 * @param config Session configuration
 * @param governor_runtime Governor instance for LLM interaction
 * @return Session handle on success, NULL on failure
 */
ethervox_conversation_session_t* ethervox_conversation_init(
    const ethervox_conversation_config_t* config,
    ethervox_governor_t* governor_runtime
);

/**
 * @brief Start conversation session (enable background thread)
 * 
 * Thread begins waiting for wake word signals. Does not block.
 * 
 * @param session Session handle
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_conversation_start(ethervox_conversation_session_t* session);

/**
 * @brief Stop conversation session (disable background thread)
 * 
 * Completes current conversation if active, then stops accepting new triggers.
 * 
 * @param session Session handle
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_conversation_stop(ethervox_conversation_session_t* session);

/**
 * @brief Trigger conversation from wake word detection
 * 
 * Signals background thread to begin listening. Safe to call from wake word
 * callback. If already processing a conversation, this is a no-op.
 * 
 * @param session Session handle
 * @return 0 on success, negative on error
 */
ethervox_result_t ethervox_conversation_trigger(ethervox_conversation_session_t* session);

/**
 * @brief Get current conversation state
 * 
 * Thread-safe query of session state.
 * 
 * @param session Session handle
 * @return Current state
 */
ethervox_conversation_state_t ethervox_conversation_get_state(
    const ethervox_conversation_session_t* session
);

/**
 * @brief Explicit barge-in / interrupt trigger
 *
 * Cancels an in-flight Governor generation (ETHERVOX_CONV_STATE_PROCESSING)
 * or in-progress speech (ETHERVOX_CONV_STATE_SPEAKING) and returns the
 * session to LISTENING as soon as the current step notices the request.
 * Safe to call from any thread - this is the primary barge-in mechanism for
 * platforms without full-duplex echo cancellation (see
 * src/dialogue/voice_conversation.c's AEC note): a UI "tap to interrupt"
 * affordance should call this directly rather than waiting for VAD.
 *
 * @param session Session handle
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_conversation_interrupt(
    ethervox_conversation_session_t* session
);

/**
 * @brief Report that platform-native TTS playback finished (or failed)
 *
 * Mobile TTS engines (Android TextToSpeech / iOS AVSpeechSynthesizer) report
 * playback completion asynchronously through their own listener interface,
 * unlike desktop Piper which this session plays and blocks on directly.
 * Callers using on_speak_request must call this once per utterance when
 * their platform TTS listener fires (onDone()/onError() or equivalent), or
 * the conversation thread will stall in ETHERVOX_CONV_STATE_SPEAKING
 * waiting for it (bounded by an internal safety-net timeout either way).
 *
 * @param session Session handle
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_conversation_notify_speaking_done(
    ethervox_conversation_session_t* session
);

/**
 * @brief Check if conversation is active
 * 
 * @param session Session handle
 * @return true if listening, processing, or speaking; false if idle or error
 */
bool ethervox_conversation_is_active(
    const ethervox_conversation_session_t* session
);

/**
 * @brief Cleanup and free conversation session
 * 
 * Stops background thread, releases Vosk/Piper resources, frees memory.
 * 
 * @param session Session handle (may be NULL)
 */
void ethervox_conversation_cleanup(ethervox_conversation_session_t* session);

/**
 * @brief Get phonemizer context from conversation session
 * 
 * For voice training and pronunciation improvement.
 * 
 * @param session Conversation session
 * @return Phonemizer context (internal type, cast to void*) or NULL if not available
 */
void* ethervox_conversation_get_phonemizer(ethervox_conversation_session_t* session);

/**
 * @brief Get TTS context from conversation session
 * 
 * For voice training audio synthesis.
 * 
 * @param session Conversation session
 * @return TTS context (internal type, cast to void*) or NULL if not available
 */
void* ethervox_conversation_get_tts(ethervox_conversation_session_t* session);

/**
 * @brief Get STT context from conversation session
 * 
 * For voice training transcription.
 * 
 * @param session Conversation session
 * @return STT context (internal type, cast to void*) or NULL if not available
 */
void* ethervox_conversation_get_stt(ethervox_conversation_session_t* session);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_CONVERSATION_H
