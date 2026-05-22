// Copyright (C) 2025 Tim Königl. All rights reserved.
// Licensed under CC BY-NC-SA 4.0

#ifndef ETHERVOX_PRONUNCIATION_TRAINER_H
#define ETHERVOX_PRONUNCIATION_TRAINER_H

#include <stdbool.h>
#include <stddef.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct phonemizer_context phonemizer_context_t;
typedef struct tts_context tts_context_t;
typedef struct stt_context stt_context_t;

/**
 * Pronunciation training result
 */
typedef struct {
    char* best_phonemes;         // Best matching phoneme sequence (IPA format)
    char* best_arpabet;          // Best matching in ARPABET format  
    float similarity_score;       // Audio similarity (0-1, higher = better)
    int variants_tested;          // Number of phoneme variants tested
    bool success;                 // Training succeeded
    char* error_message;          // Error details if failed
} pronunciation_training_result_t;

/**
 * Configuration for pronunciation training
 */
typedef struct {
    int max_variants;             // Max phoneme variants to test (default: 20)
    float min_similarity;         // Min similarity to accept (default: 0.75)
    int speaker_id;               // TTS speaker to use for synthesis (default: 0)
    bool verbose;                 // Log detailed progress (default: false)
    bool save_audio_samples;      // Save WAV files for debugging (default: false)
    char* audio_output_dir;       // Directory for audio samples (if enabled)
} pronunciation_training_config_t;

/**
 * Initialize pronunciation trainer with default configuration
 * 
 * @return Default training configuration
 */
pronunciation_training_config_t pronunciation_trainer_default_config(void);

/**
 * Train pronunciation by comparing user audio to synthesized variants
 * 
 * Workflow:
 * 1. Transcribe user_audio_path using STT to get reference text
 * 2. Generate phoneme variants for the word
 * 3. Synthesize each variant using TTS
 * 4. Compare synthesized audio to user audio using mel spectrogram distance
 * 5. Return best matching phoneme sequence
 * 
 * @param word Target word to train pronunciation for
 * @param user_audio_path Path to user's audio recording (WAV format)
 * @param phonemizer Phonemizer context for generating variants
 * @param tts TTS context for synthesizing test variants
 * @param stt STT context for transcribing user audio (optional verification)
 * @param config Training configuration (NULL = defaults)
 * @param[out] result Training result with best phonemes and score
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t pronunciation_trainer_train(
    const char* word,
    const char* user_audio_path,
    phonemizer_context_t* phonemizer,
    tts_context_t* tts,
    stt_context_t* stt,
    const pronunciation_training_config_t* config,
    pronunciation_training_result_t* result
);

/**
 * Generate phoneme variants for testing
 * 
 * Creates variations by:
 * - Adjusting stress patterns (primary/secondary)
 * - Testing vowel alternatives (e.g., ɪ vs i, æ vs ɛ)
 * - Adding/removing glides and reduced vowels
 * - Testing common mispronunciation patterns
 * 
 * @param word Target word
 * @param base_phonemes Base phoneme sequence (IPA or ARPABET)
 * @param phonemizer Phonemizer for rule-based variants
 * @param max_variants Maximum number of variants to generate
 * @param[out] variants Array of phoneme variant strings (caller must free)
 * @param[out] variant_count Number of variants generated
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t pronunciation_trainer_generate_variants(
    const char* word,
    const char* base_phonemes,
    phonemizer_context_t* phonemizer,
    int max_variants,
    char*** variants,
    int* variant_count
);

/**
 * Compare two audio files using mel spectrogram distance
 * 
 * Uses Dynamic Time Warping (DTW) on mel spectrograms to measure
 * perceptual similarity. Handles different audio lengths.
 * 
 * @param audio_path1 First audio file (WAV format)
 * @param audio_path2 Second audio file (WAV format)
 * @param[out] similarity Similarity score (0-1, higher = more similar)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t pronunciation_trainer_compare_audio(
    const char* audio_path1,
    const char* audio_path2,
    float* similarity
);

/**
 * Extract mel spectrogram features from audio file
 * 
 * @param audio_path Path to audio file (WAV format)
 * @param n_mels Number of mel bands (default: 80)
 * @param[out] mel_data Mel spectrogram data (caller must free)
 * @param[out] n_frames Number of time frames
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t pronunciation_trainer_extract_mels(
    const char* audio_path,
    int n_mels,
    float** mel_data,
    int* n_frames
);

/**
 * Calculate DTW distance between two mel spectrograms
 * 
 * @param mels1 First mel spectrogram
 * @param n_frames1 Number of frames in mels1
 * @param mels2 Second mel spectrogram
 * @param n_frames2 Number of frames in mels2
 * @param n_mels Number of mel bands (must match for both)
 * @param[out] distance DTW distance (lower = more similar)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t pronunciation_trainer_dtw_distance(
    const float* mels1,
    int n_frames1,
    const float* mels2,
    int n_frames2,
    int n_mels,
    float* distance
);

/**
 * Free pronunciation training result
 * 
 * @param result Result to free (NULL-safe)
 */
void pronunciation_training_result_free(pronunciation_training_result_t* result);

/**
 * Free phoneme variant array
 * 
 * @param variants Array of variant strings
 * @param count Number of variants
 */
void pronunciation_trainer_free_variants(char** variants, int count);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PRONUNCIATION_TRAINER_H
