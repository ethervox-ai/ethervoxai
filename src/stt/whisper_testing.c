/**
 * @file whisper_test.c
 * @brief Whisper testing utilities - process test WAV files
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ethervox/stt.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"

#define LOG_ERROR(...) ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(...) ethervox_log(ETHERVOX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * Simple WAV file reader - handles standard PCM WAV with optional LIST chunks
 */
typedef struct {
    char chunk_id[4];
    uint32_t chunk_size;
} wav_chunk_header_t;

typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
} __attribute__((packed)) wav_riff_header_t;

typedef struct {
    uint16_t audio_format;  // 1 = PCM
    uint16_t num_channels;  // 1 = mono, 2 = stereo
    uint32_t sample_rate;   // e.g., 16000
    uint32_t byte_rate;     // sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align;   // num_channels * bits_per_sample/8
    uint16_t bits_per_sample; // 16 for int16_t
} __attribute__((packed)) wav_fmt_data_t;

/**
 * Test Whisper with a WAV file
 * 
 * @param runtime STT runtime (must be initialized)
 * @param wav_file Path to 16kHz mono WAV file
 * @param result Output transcription result
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_whisper_test_wav(
    ethervox_stt_runtime_t* runtime,
    const char* wav_file,
    ethervox_stt_result_t* result
) {
    ETHERVOX_CHECK_PTR(runtime);
    ETHERVOX_CHECK_PTR(wav_file);
    ETHERVOX_CHECK_PTR(result);
    
    // Open WAV file
    FILE* fp = fopen(wav_file, "rb");
    if (!fp) {
        printf("❌ Failed to open WAV file: %s\n", wav_file);
        LOG_ERROR("Failed to open WAV file: %s", wav_file);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    printf("📁 Opened WAV file successfully\n");
    
    // Read RIFF header
    wav_riff_header_t riff_header;
    if (fread(&riff_header, sizeof(riff_header), 1, fp) != 1) {
        printf("❌ Failed to read RIFF header\n");
        fclose(fp);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    if (memcmp(riff_header.riff, "RIFF", 4) != 0 || memcmp(riff_header.wave, "WAVE", 4) != 0) {
        printf("❌ Invalid WAV file format\n");
        fclose(fp);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    printf("📊 Valid RIFF/WAVE header\n");
    
    // Find fmt chunk
    wav_fmt_data_t fmt_data;
    uint32_t sample_rate = 0, data_size = 0;
    uint16_t num_channels = 0, bits_per_sample = 0;
    bool found_fmt = false, found_data = false;
    
    while (!feof(fp) && (!found_fmt || !found_data)) {
        wav_chunk_header_t chunk;
        if (fread(&chunk, sizeof(chunk), 1, fp) != 1) break;
        
        if (memcmp(chunk.chunk_id, "fmt ", 4) == 0) {
            if (fread(&fmt_data, sizeof(fmt_data), 1, fp) != 1) {
                printf("❌ Failed to read fmt chunk\n");
                fclose(fp);
                return ETHERVOX_ERROR_STT_PROCESSING;
            }
            sample_rate = fmt_data.sample_rate;
            num_channels = fmt_data.num_channels;
            bits_per_sample = fmt_data.bits_per_sample;
            found_fmt = true;
            // Skip any extra fmt data
            if (chunk.chunk_size > sizeof(fmt_data)) {
                fseek(fp, chunk.chunk_size - sizeof(fmt_data), SEEK_CUR);
            }
            printf("📊 Found fmt chunk\n");
        } else if (memcmp(chunk.chunk_id, "data", 4) == 0) {
            data_size = chunk.chunk_size;
            found_data = true;
            printf("📊 Found data chunk (%u bytes)\n", data_size);
            // Data follows immediately, don't skip
            break;
        } else {
            // Skip unknown chunk
            printf("⏭️  Skipping chunk: %.4s (%u bytes)\n", chunk.chunk_id, chunk.chunk_size);
            fseek(fp, chunk.chunk_size, SEEK_CUR);
        }
    }
    
    if (!found_fmt || !found_data) {
        printf("❌ Missing required WAV chunks (fmt: %d, data: %d)\n", found_fmt, found_data);
        fclose(fp);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    printf("WAV file info:\n");
    printf("  Sample rate: %u Hz\n", sample_rate);
    printf("  Channels: %u\n", num_channels);
    printf("  Bits per sample: %u\n", bits_per_sample);
    printf("  Data size: %u bytes (%.2f seconds)\n", data_size, (float)data_size / (sample_rate * num_channels * (bits_per_sample/8)));
    printf("\n");
    
    // Check if we need resampling (we expect 16kHz mono 16-bit)
    if (sample_rate != 16000) {
        printf("❌ Sample rate must be 16000 Hz (got %u Hz)\n", sample_rate);
        printf("   Convert with: ffmpeg -i input.wav -ar 16000 -ac 1 -sample_fmt s16 output.wav\n");
        LOG_ERROR("Sample rate must be 16000 Hz (got %u Hz)", sample_rate);
        fclose(fp);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    if (num_channels != 1) {
        printf("❌ Must be mono audio (got %u channels)\n", num_channels);
        LOG_ERROR("Must be mono audio (got %u channels)", num_channels);
        fclose(fp);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    if (bits_per_sample != 16) {
        printf("❌ Must be 16-bit audio (got %u bits)\n", bits_per_sample);
        LOG_ERROR("Must be 16-bit audio (got %u bits)", bits_per_sample);
        fclose(fp);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    // Allocate buffer for audio data
    uint32_t sample_count = data_size / 2; // 16-bit = 2 bytes per sample
    int16_t* samples_i16 = (int16_t*)malloc(data_size);
    if (!samples_i16) {
        printf("❌ Failed to allocate audio buffer\n");
        LOG_ERROR("Failed to allocate audio buffer");
        fclose(fp);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    // Read audio data
    size_t read = fread(samples_i16, 1, data_size, fp);
    fclose(fp);
    
    if (read != data_size) {
        printf("❌ Failed to read audio data (got %zu, expected %u)\n", read, data_size);
        LOG_ERROR("Failed to read audio data (got %zu, expected %u)", read, data_size);
        free(samples_i16);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    printf("✅ Loaded %.2f seconds of audio\n", (float)sample_count / 16000.0f);
    printf("Processing...\n\n");
    
    // Start STT session before processing
    if (ethervox_stt_start(runtime) != 0) {
        printf("❌ Failed to start STT session\n");
        LOG_ERROR("Failed to start STT session");
        free(samples_i16);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    // Convert to float and process in chunks (simulate streaming)
    const size_t chunk_size = 16000 * 3; // 3 seconds per chunk
    ethervox_audio_buffer_t audio_buf;
    audio_buf.data = (float*)malloc(chunk_size * sizeof(float));
    audio_buf.channels = 1;
    audio_buf.timestamp_us = 0;
    
    if (!audio_buf.data) {
        LOG_ERROR("Failed to allocate conversion buffer");
        free(samples_i16);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
    
    memset(result, 0, sizeof(ethervox_stt_result_t));
    char* full_transcript = NULL;
    size_t transcript_len = 0;
    
    int chunks_processed = 0;
    for (size_t offset = 0; offset < sample_count; offset += chunk_size) {
        size_t chunk_samples = (offset + chunk_size < sample_count) ? chunk_size : (sample_count - offset);
        chunks_processed++;
        
        printf("  Chunk %d: Processing %.2f-%.2f sec (%zu samples)...\n",
               chunks_processed,
               (float)offset / 16000.0f,
               (float)(offset + chunk_samples) / 16000.0f,
               chunk_samples);
        
        // Convert int16 to float
        for (size_t i = 0; i < chunk_samples; i++) {
            ((float*)audio_buf.data)[i] = (float)samples_i16[offset + i] / 32768.0f;
        }
        
            audio_buf.size = chunk_samples;
        audio_buf.timestamp_us = (offset * 1000000ULL) / 16000;
        
        // Process chunk
        ethervox_stt_result_t chunk_result;
        ethervox_result_t ret = ethervox_stt_process(runtime, &audio_buf, &chunk_result);
        
        printf("     Result: ret=%d", ret);
        if (ethervox_is_success(ret) && chunk_result.text && strlen(chunk_result.text) > 0) {
            printf(", text=\"%s\"\n", chunk_result.text);
            LOG_INFO("[%.2f-%.2f] %s", 
                    (float)offset / 16000.0f,
                    (float)(offset + chunk_samples) / 16000.0f,
                    chunk_result.text);
            
            // Append to full transcript
            size_t new_len = transcript_len + strlen(chunk_result.text) + 2;
            full_transcript = (char*)realloc(full_transcript, new_len);
            if (full_transcript) {
                if (transcript_len > 0) {
                    strcat(full_transcript, " ");
                }
                strcat(full_transcript, chunk_result.text);
                transcript_len = strlen(full_transcript);
            }
            
            ethervox_stt_result_free(&chunk_result);
        } else {
            printf(", no text\n");
        }
    }
    
    free(audio_buf.data);
    free(samples_i16);
    
    printf("\n");
    printf("Processed %d chunks total\n", chunks_processed);
    
    if (full_transcript && transcript_len > 0) {
        printf("✅ Got transcription (%zu chars)\n", transcript_len);
        result->text = full_transcript;
        result->confidence = 0.9f;
        result->is_final = true;
        result->language = "en";
        LOG_INFO("Complete transcript: %s", full_transcript);
        return ETHERVOX_SUCCESS;
    } else {
        printf("❌ No transcription produced from any chunk\n");
        LOG_ERROR("No transcription produced");
        if (full_transcript) free(full_transcript);
        return ETHERVOX_ERROR_STT_PROCESSING;
    }
}

/**
 * Quick test using the JFK sample
 */
ethervox_result_t ethervox_whisper_test_jfk(ethervox_stt_runtime_t* runtime) {
    ETHERVOX_CHECK_PTR(runtime);
    const char* test_paths[] = {
        "external/whisper.cpp/samples/jfk.wav",
        "../external/whisper.cpp/samples/jfk.wav",
        "../../external/whisper.cpp/samples/jfk.wav",
        NULL
    };
    
    const char* wav_file = NULL;
    for (int i = 0; test_paths[i] != NULL; i++) {
        FILE* test = fopen(test_paths[i], "rb");
        if (test) {
            fclose(test);
            wav_file = test_paths[i];
            break;
        }
    }
    
    if (!wav_file) {
        printf("❌ JFK test file not found. Tried:\n");
        for (int i = 0; test_paths[i] != NULL; i++) {
            printf("  - %s\n", test_paths[i]);
        }
        LOG_ERROR("JFK test file not found");
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_NOT_FOUND, "JFK test file not found");
    }
    
    printf("========================================\n");
    printf("Whisper Test Mode - JFK Sample\n");
    printf("========================================\n");
    printf("File: %s\n", wav_file);
    printf("Expected: \"And so my fellow Americans, ask not what your country can do for you...\"\n");
    printf("\n");
    
    ethervox_stt_result_t result;
    ethervox_result_t ret = ethervox_whisper_test_wav(runtime, wav_file, &result);
    
    if (ethervox_is_success(ret)) {
        printf("\n");
        printf("✅ Test PASSED - Whisper infrastructure working!\n");
        printf("Transcription: %s\n", result.text);
        LOG_INFO("Whisper test passed");
        ethervox_stt_result_free(&result);
        return 0;
    } else {
        printf("\n❌ Test FAILED (error code: %d)\n", ret);
        LOG_ERROR("Whisper test failed with code %d", ret);
        return ret;
    }
}
