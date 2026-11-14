/**
 * @file voice_tools.h
 * @brief Voice tools interface for Governor (Mode 2: Transcription, Granite Speech Plus/SAA)
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_VOICE_TOOLS_H
#define ETHERVOX_VOICE_TOOLS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "ethervox/stt.h"
#include "ethervox/audio.h"
#include "ethervox/memory_tools.h"
#include "ethervox/error.h"
#include "ethervox/event_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

// Granite Speech Plus's finalize() is one-shot per call (see
// granite_speech_backend.c) - there is no VAD-driven segment boundary like
// Whisper had. Mode 2's capture thread instead accumulates audio for
// GRANITE_SPEECH_CHUNK_SECONDS, calls finalize() to transcribe that chunk,
// carries a bounded tail of the transcript forward as prefix_text (for
// consistent [Speaker N]: numbering across chunks), then starts the next
// chunk. Kept comfortably under the 60s accumulator cap (see stt_core.c)
// to leave margin for the audio_capture_thread's own read/scheduling
// latency.
#define GRANITE_SPEECH_CHUNK_SECONDS 45
// Upper bound on how much of the running transcript is carried forward as
// prefix_text between chunks. Full-transcript carry-forward would keep
// speaker numbering perfectly consistent but risks exceeding the model's
// prompt budget (4096 ctx, shared with audio tokens) on long sessions - a
// bounded tail is a pragmatic trade-off pending real on-device tuning.
#define GRANITE_SPEECH_PREFIX_CARRY_MAX_CHARS 1500

/**
 * Voice recording session state
 */
typedef struct {
    bool is_recording;
    bool is_initialized;
    bool stop_requested;
    
    // STT runtime (always Granite Speech Plus/SAA for this mode)
    ethervox_stt_runtime_t stt_runtime;
    ethervox_audio_runtime_t audio_runtime;
    
    // Model configuration - Granite Speech ships as a (model, mmproj) GGUF
    // pair; both paths are heap-allocated and owned by the session.
    char* model_path;   // Path to granite-speech-4.1-2b-plus GGUF
    char* mmproj_path;  // Path to the companion mmproj (audio projector) GGUF
    
    // Accumulated transcript
    char* full_transcript;
    size_t transcript_len;
    size_t transcript_capacity;
    
    // Session metadata
    uint64_t session_start_time;
    uint32_t segment_count;
    
    // Last saved transcript file path
    char last_transcript_file[1024];
    
    // Memory store for saving transcripts
    void* memory_store;  // ethervox_memory_store_t*
    
    // Background processing thread
    void* capture_thread;  // pthread_t*
    
    // Chunked-decoding state (see GRANITE_SPEECH_CHUNK_SECONDS above):
    // tracks when the current chunk started so audio_capture_thread knows
    // when to finalize() and start the next chunk, and holds the bounded
    // transcript tail passed back in as config.prefix_text.
    time_t chunk_start_time;
    char prefix_carry[GRANITE_SPEECH_PREFIX_CARRY_MAX_CHARS];
    
    // Speaker tracking. Granite Speech Plus's native SAA tags are
    // 1-indexed ("[Speaker 1]: ..."), unlike the old 0-indexed heuristic
    // format - max_speaker_id IS the speaker count directly (no +1).
    int max_speaker_id;  // Highest speaker ID encountered in this session (0 = none yet)
    char** speaker_names;  // Array of speaker names, 1-indexed (index 0 unused), NULL if anonymous
    int speaker_names_capacity;  // Allocated capacity for speaker_names array
    
    // Summarization state
    bool needs_summarization;  // Flag to trigger LLM summarization

    // TASK-C3.5: streaming transcription events. Fired once per finalized
    // chunk (see GRANITE_SPEECH_CHUNK_SECONDS), during capture, in addition
    // to - not instead of - full_transcript above. NULL if not registered
    // (existing hosts that never call the setter see no behaviour change).
    ethervox_event_cb transcription_event_cb;
    void* transcription_event_user_data;

    // TASK-C3.5 backpressure: bounded async delivery queue (opaque, see
    // voice_tools.c) so a slow event consumer never stalls the capture
    // thread. Allocated lazily on first event push; NULL until then.
    void* event_queue;

    // TASK-N7.8: unbounded producer/consumer queue of raw captured audio
    // (opaque, see voice_tools.c). audio_capture_thread() only reads
    // hardware audio and pushes it here; a separate worker thread owns all
    // STT interaction (ethervox_stt_process()/finalize()/start()), so a slow
    // finalize() (measured up to ~60s per chunk - see N7.7) never blocks
    // audio capture. Allocated lazily on first start_listen(); NULL until
    // then.
    void* audio_finalize_queue;

} ethervox_voice_session_t;


/**
 * Initialize voice tools
 * 
 * @param session Voice session state
 * @param memory Memory store for saving transcripts
 * @param pool Model pool for budget enforcement (N6.3, NULL for legacy platforms)
 * @return 0 on success, -1 on error
 */
int ethervox_voice_tools_init(ethervox_voice_session_t* session, void* memory, struct ethervox_model_pool* pool);

/**
 * Start listening session
 * 
 * @param session Voice session
 * @return 0 on success, -1 on error
 */
int ethervox_voice_tools_start_listen(ethervox_voice_session_t* session);

/**
 * Stop listening session and get transcript
 * 
 * @param session Voice session
 * @param transcript_out Full transcript (caller should not free - internal buffer)
 * @return 0 on success, -1 on error
 */
int ethervox_voice_tools_stop_listen(ethervox_voice_session_t* session, const char** transcript_out);

/**
 * Check if currently recording
 */
bool ethervox_voice_tools_is_recording(const ethervox_voice_session_t* session);

/**
 * Cleanup voice tools
 */
void ethervox_voice_tools_cleanup(ethervox_voice_session_t* session);

/**
 * Register a callback for streaming transcription segment events (TASK-C3.5).
 *
 * The callback receives an ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT event once per
 * chunk finalized during capture (see GRANITE_SPEECH_CHUNK_SECONDS), before
 * the session is stopped. This is additive: ethervox_voice_tools_stop_listen()
 * still returns the same complete transcript regardless of whether a callback
 * is registered.
 *
 * @param session Voice session
 * @param callback Callback to invoke per segment, or NULL to clear
 * @param user_data Opaque pointer passed back to the callback
 */
void ethervox_voice_tools_set_event_callback(ethervox_voice_session_t* session,
                                              ethervox_event_cb callback,
                                              void* user_data);

/**
 * Prompt user to assign names to speakers and update transcript file
 * Called after transcription ends
 * 
 * @param session Voice session with completed transcript
 * @return 0 on success, -1 on error, 1 if user declined naming
 */
int ethervox_voice_tools_assign_speaker_names(ethervox_voice_session_t* session);

/**
 * Register base voice tools with Governor
 * 
 * Base tools: listen_and_summarize (always available)
 * 
 * @param registry Governor tool registry
 * @param session Voice session state
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_voice_tools_register(void* registry, ethervox_voice_session_t* session);

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
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_voice_tools_register_training(
    void* registry, 
    ethervox_voice_session_t* session,
    void* phonemizer_ctx,
    void* tts_ctx,
    void* stt_ctx
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_VOICE_TOOLS_H
