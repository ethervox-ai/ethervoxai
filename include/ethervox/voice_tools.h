/**
 * @file voice_tools.h
 * @brief Voice tools interface for Governor
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_VOICE_TOOLS_H
#define ETHERVOX_VOICE_TOOLS_H

#include <stdbool.h>
#include <stdint.h>
#include "ethervox/stt.h"
#include "ethervox/audio.h"
#include "ethervox/memory_tools.h"
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Voice recording session state
 */
typedef struct {
    bool is_recording;
    bool is_initialized;
    bool stop_requested;
    
    // STT runtime
    ethervox_stt_runtime_t stt_runtime;
    ethervox_audio_runtime_t audio_runtime;
    
    // Model configuration
    char* model_path;  // Allocated path to Whisper model
    
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
    
    // Speaker tracking
    int max_speaker_id;  // Highest speaker ID encountered in this session
    char** speaker_names;  // Array of speaker names (NULL if anonymous)
    int speaker_names_capacity;  // Allocated capacity for speaker_names array
    
    // Summarization state
    bool needs_summarization;  // Flag to trigger LLM summarization
    
} ethervox_voice_session_t;

/**
 * Initialize voice tools
 * 
 * @param session Voice session state
 * @param memory Memory store for saving transcripts
 * @return 0 on success, -1 on error
 */
int ethervox_voice_tools_init(ethervox_voice_session_t* session, void* memory);

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
