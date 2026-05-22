/**
 * @file audio_recording.h
 * @brief Audio recording utilities for training and testing
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_AUDIO_RECORDING_H
#define ETHERVOX_AUDIO_RECORDING_H

#include "ethervox/audio.h"
#include <stdint.h>
#include <stdbool.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Record audio to WAV file
 * 
 * Uses existing audio capture infrastructure to record audio
 * and save it to a WAV file for training or testing purposes.
 * 
 * @param output_path Path to output WAV file
 * @param duration_seconds Recording duration in seconds
 * @param sample_rate Sample rate (default: 16000 Hz)
 * @param channels Number of channels (default: 1 = mono)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_audio_record_to_file(
    const char* output_path,
    int duration_seconds,
    int sample_rate,
    int channels
);

/**
 * Record audio with automatic silence detection
 * 
 * Starts recording when speech is detected and stops after
 * a period of silence. Useful for natural speech capture.
 * 
 * @param output_path Path to output WAV file
 * @param max_duration_seconds Maximum recording duration
 * @param silence_threshold Silence detection threshold (0.0-1.0)
 * @param silence_duration_ms Duration of silence to stop recording
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_audio_record_with_vad(
    const char* output_path,
    int max_duration_seconds,
    float silence_threshold,
    int silence_duration_ms
);

/**
 * Write audio buffer to WAV file
 * 
 * Low-level function to write raw audio samples to WAV format.
 * 
 * @param output_path Path to output WAV file
 * @param samples Audio samples (float, normalized to [-1, 1])
 * @param num_samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @param channels Number of channels
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_audio_write_wav(
    const char* output_path,
    const float* samples,
    int num_samples,
    int sample_rate,
    int channels
);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_AUDIO_RECORDING_H
