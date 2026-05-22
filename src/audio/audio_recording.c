/**
 * @file audio_recording.c
 * @brief Audio recording utilities implementation
 *
 * Reuses existing audio capture infrastructure for training
 * and testing purposes. Provides simple API for recording to WAV files.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/audio_recording.h"
#include "ethervox/audio.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// WAV file format structures (simple RIFF WAV header)
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
} wav_riff_header_t;

typedef struct {
    char fmt[4];            // "fmt "
    uint32_t chunk_size;    // 16 for PCM
    uint16_t audio_format;  // 1 = PCM, 3 = IEEE float
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_fmt_chunk_t;

typedef struct {
    char data[4];           // "data"
    uint32_t data_size;     // Size of audio data
} wav_data_header_t;

/**
 * Write audio buffer to WAV file
 */
ethervox_result_t ethervox_audio_write_wav(
    const char* output_path,
    const float* samples,
    int num_samples,
    int sample_rate,
    int channels
) {
    ETHERVOX_CHECK_PTR(output_path);
    ETHERVOX_CHECK_PTR(samples);
    if (num_samples <= 0) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT, "Invalid sample count");
    }

    FILE* fp = fopen(output_path, "wb");
    if (!fp) {
        ETHERVOX_LOG_ERROR("Failed to create WAV file: %s", output_path);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_WRITE, "Cannot create WAV file");
    }

    // Convert float samples to 16-bit PCM
    int16_t* pcm_samples = (int16_t*)malloc(num_samples * sizeof(int16_t));
    if (!pcm_samples) {
        ETHERVOX_LOG_ERROR("Failed to allocate PCM buffer");
        fclose(fp);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "PCM buffer allocation failed");
    }

    for (int i = 0; i < num_samples; i++) {
        // Clamp to [-1, 1] and convert to 16-bit
        float sample = samples[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        pcm_samples[i] = (int16_t)(sample * 32767.0f);
    }

    // Calculate sizes
    uint32_t data_size = num_samples * sizeof(int16_t);
    uint32_t file_size = sizeof(wav_riff_header_t) + sizeof(wav_fmt_chunk_t) + 
                         sizeof(wav_data_header_t) + data_size - 8;

    // Write RIFF header
    wav_riff_header_t riff = {
        .riff = {'R', 'I', 'F', 'F'},
        .file_size = file_size,
        .wave = {'W', 'A', 'V', 'E'}
    };
    fwrite(&riff, sizeof(riff), 1, fp);

    // Write format chunk
    wav_fmt_chunk_t fmt = {
        .fmt = {'f', 'm', 't', ' '},
        .chunk_size = 16,
        .audio_format = 1,  // PCM
        .num_channels = channels,
        .sample_rate = sample_rate,
        .byte_rate = sample_rate * channels * sizeof(int16_t),
        .block_align = channels * sizeof(int16_t),
        .bits_per_sample = 16
    };
    fwrite(&fmt, sizeof(fmt), 1, fp);

    // Write data header
    wav_data_header_t data_hdr = {
        .data = {'d', 'a', 't', 'a'},
        .data_size = data_size
    };
    fwrite(&data_hdr, sizeof(data_hdr), 1, fp);

    // Write audio data
    fwrite(pcm_samples, 1, data_size, fp);

    free(pcm_samples);
    fclose(fp);

    ETHERVOX_LOG_INFO("Wrote %d samples to %s", num_samples, output_path);
    return ETHERVOX_SUCCESS;
}

/**
 * Record audio to WAV file
 * 
 * Uses existing audio capture infrastructure (same pattern as voice_tools.c)
 */
ethervox_result_t ethervox_audio_record_to_file(
    const char* output_path,
    int duration_seconds,
    int sample_rate,
    int channels
) {
    ETHERVOX_CHECK_PTR(output_path);
    if (duration_seconds <= 0) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT, "Invalid duration");
    }

    ETHERVOX_LOG_INFO("Recording %d seconds of audio to: %s", duration_seconds, output_path);
    
    // Initialize audio runtime
    ethervox_audio_runtime_t audio_runtime = {0};
    ethervox_audio_config_t config = ethervox_audio_get_default_config();
    config.sample_rate = sample_rate;
    config.channels = channels;
    
    ethervox_result_t result = ethervox_audio_init(&audio_runtime, &config);
    if (ethervox_is_error(result)) {
        ETHERVOX_LOG_ERROR("Failed to initialize audio runtime");
        return result;
    }
    
    // Start audio capture
    result = ethervox_audio_start_capture(&audio_runtime);
    if (ethervox_is_error(result)) {
        ETHERVOX_LOG_ERROR("Failed to start audio capture");
        ethervox_audio_cleanup(&audio_runtime);
        return result;
    }
    
    // Allocate buffer for entire recording
    int total_samples = sample_rate * channels * duration_seconds;
    float* recording_buffer = (float*)malloc(total_samples * sizeof(float));
    if (!recording_buffer) {
        ETHERVOX_LOG_ERROR("Failed to allocate recording buffer");
        ethervox_audio_stop_capture(&audio_runtime);
        ethervox_audio_cleanup(&audio_runtime);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Recording buffer allocation failed");
    }
    
    // Record in small chunks with sleep to allow audio accumulation
    const int chunk_ms = 100;  // Read every 100ms
    const int chunks_per_second = 1000 / chunk_ms;
    const int total_chunks = duration_seconds * chunks_per_second;
    int chunk_samples = (sample_rate * channels * chunk_ms) / 1000;
    
    // Allocate buffer for chunk reading
    int samples_recorded = 0;
    ethervox_audio_buffer_t audio_buf;
    audio_buf.data = (float*)malloc(chunk_samples * 2 * sizeof(float));  // 2x for safety
    audio_buf.size = chunk_samples * 2;
    audio_buf.channels = channels;
    audio_buf.timestamp_us = 0;
    
    if (!audio_buf.data) {
        ETHERVOX_LOG_ERROR("Failed to allocate audio buffer");
        free(recording_buffer);
        ethervox_audio_stop_capture(&audio_runtime);
        ethervox_audio_cleanup(&audio_runtime);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Audio buffer allocation failed");
    }
    
    ETHERVOX_LOG_INFO("Recording started... (press Ctrl+C to stop early)");
    
    for (int chunk = 0; chunk < total_chunks; chunk++) {
        // Sleep to allow audio to accumulate in driver buffer
#ifdef _WIN32
        Sleep(chunk_ms);  // Sleep in milliseconds on Windows
#else
        usleep(chunk_ms * 1000);  // Convert ms to microseconds
#endif
        
        // Read audio chunk
        ethervox_result_t read_result = ethervox_audio_read(&audio_runtime, &audio_buf);
        
        if (ethervox_is_success(read_result) && audio_buf.size > 0) {
            int samples_read = (int)audio_buf.size;
            // Clamp to available space
            int samples_to_copy = samples_read;
            if (samples_recorded + samples_to_copy > total_samples) {
                samples_to_copy = total_samples - samples_recorded;
            }
            
            // Copy to recording buffer
            memcpy(recording_buffer + samples_recorded, audio_buf.data, 
                   samples_to_copy * sizeof(float));
            samples_recorded += samples_to_copy;
        }
        
        // Show progress every second
        if ((chunk + 1) % chunks_per_second == 0) {
            int seconds_recorded = (chunk + 1) / chunks_per_second;
            ETHERVOX_LOG_INFO("  %d/%d seconds recorded...", seconds_recorded, duration_seconds);
        }
        
        if (samples_recorded >= total_samples) {
            break;
        }
    }
    
    // Stop capture
    ethervox_audio_stop_capture(&audio_runtime);
    ethervox_audio_cleanup(&audio_runtime);
    free(audio_buf.data);
    
    ETHERVOX_LOG_INFO("Recording complete: %d samples captured", samples_recorded);
    
    // Write to WAV file
    result = ethervox_audio_write_wav(output_path, recording_buffer, 
                                         samples_recorded, sample_rate, channels);
    
    free(recording_buffer);
    
    if (ethervox_is_success(result)) {
        ETHERVOX_LOG_INFO("Successfully saved audio to: %s", output_path);
    }
    
    return result;
}

/**
 * Record audio with automatic silence detection
 * 
 * TODO: Needs integration with audio capture system (same as above)
 */
ethervox_result_t ethervox_audio_record_with_vad(
    const char* output_path,
    int max_duration_seconds,
    float silence_threshold,
    int silence_duration_ms
) {
    (void)silence_threshold;
    (void)silence_duration_ms;
    
    ETHERVOX_LOG_WARN("VAD recording not yet implemented");
    
    // For now, fall back to simple recording
    return ethervox_audio_record_to_file(output_path, max_duration_seconds, 16000, 1);
}
