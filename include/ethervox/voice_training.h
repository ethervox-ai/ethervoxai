/**
 * @file voice_training.h
 * @brief Interactive voice training mode for pronunciation improvement
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_VOICE_TRAINING_H
#define ETHERVOX_VOICE_TRAINING_H

#include "ethervox/governor.h"
#include "ethervox/tts.h"
#include "ethervox/stt.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct phonemizer_context phonemizer_t;

/**
 * Run interactive voice training session
 * 
 * Workflow:
 * 1. LLM generates training sentences
 * 2. User reads them aloud
 * 3. System records and analyzes pronunciation
 * 4. Learns corrections and stores in pronunciation overrides
 * 
 * @param governor Governor instance for text generation
 * @param phonemizer Phonemizer for pronunciation analysis
 * @param tts TTS system for reference audio (internal tts_context_t*, pass as void*)
 * @param stt STT system for transcription (internal stt_context_t*, pass as void*, optional)
 * @return 0 on success, -1 on error
 */
ethervox_result_t ethervox_voice_training_run(
    ethervox_governor_t* governor,
    phonemizer_t* phonemizer,
    void* tts,
    void* stt
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_VOICE_TRAINING_H
