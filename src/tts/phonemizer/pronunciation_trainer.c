// Copyright (C) 2025 Tim Königl. All rights reserved.
// Licensed under CC BY-NC-SA 4.0

#include "ethervox/pronunciation_trainer.h"
#include "ethervox/error.h"
#include "ethervox/tts.h"
#include "ethervox/stt.h"
#include "ethervox/audio_recording.h"
#include "ethervox/logging.h"
#include "pronunciation_overrides.h"
#include "phonemizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

// WAV file reading for mel extraction
#include <stdint.h>

#define MAX_VARIANTS 50
#define DEFAULT_MAX_VARIANTS 20
#define DEFAULT_MIN_SIMILARITY 0.75f
#define MEL_BANDS 80
#define SAMPLE_RATE 16000
#define FFT_SIZE 512
#define HOP_LENGTH 256

// Phoneme variant generation rules
static const char* vowel_alternatives[][5] = {
    {"i", "ɪ", "iː", NULL},           // ee vs ih
    {"ɪ", "i", "ɨ", NULL},            // ih vs ee
    {"e", "ɛ", "eɪ", NULL},           // ay vs eh
    {"ɛ", "e", "æ", NULL},            // eh vs e
    {"æ", "ɛ", "a", NULL},            // ae vs eh
    {"ɑ", "ɒ", "aː", NULL},           // ah vs o
    {"ɔ", "ɒ", "oː", NULL},           // aw vs o
    {"o", "oʊ", "ɔ", NULL},           // oh vs aw
    {"ʊ", "u", "uː", NULL},           // uh vs oo
    {"u", "ʊ", "uː", NULL},           // oo vs uh
    {"ʌ", "ə", "a", NULL},            // uh vs schwa
    {"ə", "ʌ", "ɪ", NULL},            // schwa vs uh
    {NULL}
};

static const char* stress_patterns[] = {
    "ˈ",    // Primary stress
    "ˌ",    // Secondary stress
    "",     // No stress
    NULL
};

pronunciation_training_config_t pronunciation_trainer_default_config(void) {
    pronunciation_training_config_t config = {
        .max_variants = DEFAULT_MAX_VARIANTS,
        .min_similarity = DEFAULT_MIN_SIMILARITY,
        .speaker_id = 0,
        .verbose = false,
        .save_audio_samples = false,
        .audio_output_dir = NULL
    };
    return config;
}

// Helper: Read WAV file audio data
static int read_wav_audio(const char* path, float** audio_data, int* n_samples) {
    ETHERVOX_LOG_DEBUG("[read_wav_audio] Opening: %s\n", path);
    FILE* f = fopen(path, "rb");
    if (!f) {
        ETHERVOX_LOG_DEBUG("[read_wav_audio] ERROR: Failed to open audio file: %s\n", path);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Read WAV header (44 bytes)
    uint8_t header[44];
    if (fread(header, 1, 44, f) != 44) {
        ETHERVOX_LOG_DEBUG("[read_wav_audio] ERROR: Invalid WAV file (header too small): %s\n", path);
        fclose(f);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Extract audio format info
    uint16_t audio_format = header[20] | (header[21] << 8);
    uint16_t num_channels = header[22] | (header[23] << 8);
    uint32_t sample_rate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
    uint16_t bits_per_sample = header[34] | (header[35] << 8);
    uint32_t data_size = header[40] | (header[41] << 8) | (header[42] << 16) | (header[43] << 24);

    fprintf(stderr, "[read_wav_audio] Format: %d, Channels: %d, Sample rate: %d, Bits: %d, Data size: %d\n",
            audio_format, num_channels, sample_rate, bits_per_sample, data_size);
    
    if (audio_format != 1) { // PCM
        ETHERVOX_LOG_DEBUG("[read_wav_audio] ERROR: Unsupported audio format: %d\n", audio_format);
        fclose(f);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    int samples = data_size / (bits_per_sample / 8) / num_channels;
    *n_samples = samples;
    ETHERVOX_LOG_DEBUG("[read_wav_audio] Calculated samples: %d\n", samples);
    
    *audio_data = (float*)malloc(samples * sizeof(float));
    if (!*audio_data) {
        ETHERVOX_LOG_DEBUG("[read_wav_audio] ERROR: Failed to allocate memory for %d samples\n", samples);
        fclose(f);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Read and convert to float [-1, 1]
    if (bits_per_sample == 16) {
        int16_t* buffer = (int16_t*)malloc(data_size);
        if (!buffer || fread(buffer, 1, data_size, f) != data_size) {
            free(*audio_data);
            free(buffer);
            fclose(f);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }

        for (int i = 0; i < samples; i++) {
            // Average channels if stereo
            float sample = 0.0f;
            for (int ch = 0; ch < num_channels; ch++) {
                sample += buffer[i * num_channels + ch] / 32768.0f;
            }
            (*audio_data)[i] = sample / num_channels;
        }
        fprintf(stderr, "[read_wav_audio] Successfully read %d samples (first: %.4f, last: %.4f)\n", 
                samples, (*audio_data)[0], (*audio_data)[samples-1]);
        free(buffer);
    } else {
        ETHERVOX_LOG_DEBUG("[read_wav_audio] ERROR: Unsupported bit depth: %d\n", bits_per_sample);
        free(*audio_data);
        fclose(f);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    fclose(f);
    return ETHERVOX_SUCCESS;
}

// Helper: Compute mel filterbank
static void create_mel_filterbank(float** filterbank, int n_mels, int fft_bins, int sample_rate) {
    float mel_min = 0.0f;
    float mel_max = 2595.0f * log10f(1.0f + (sample_rate / 2.0f) / 700.0f);
    
    float* mel_points = (float*)malloc((n_mels + 2) * sizeof(float));
    for (int i = 0; i < n_mels + 2; i++) {
        mel_points[i] = mel_min + (mel_max - mel_min) * i / (n_mels + 1);
    }
    
    // Convert mel to Hz
    float* hz_points = (float*)malloc((n_mels + 2) * sizeof(float));
    for (int i = 0; i < n_mels + 2; i++) {
        hz_points[i] = 700.0f * (powf(10.0f, mel_points[i] / 2595.0f) - 1.0f);
    }
    
    // Convert Hz to FFT bins
    int* bin_points = (int*)malloc((n_mels + 2) * sizeof(int));
    for (int i = 0; i < n_mels + 2; i++) {
        bin_points[i] = (int)floorf((fft_bins + 1) * hz_points[i] / sample_rate);
    }
    
    // Create triangular filters
    for (int m = 0; m < n_mels; m++) {
        for (int k = 0; k < fft_bins; k++) {
            filterbank[m][k] = 0.0f;
            
            if (k < bin_points[m]) continue;
            if (k > bin_points[m + 2]) continue;
            
            if (k <= bin_points[m + 1]) {
                if (bin_points[m + 1] != bin_points[m]) {
                    filterbank[m][k] = (float)(k - bin_points[m]) / (bin_points[m + 1] - bin_points[m]);
                }
            } else {
                if (bin_points[m + 2] != bin_points[m + 1]) {
                    filterbank[m][k] = (float)(bin_points[m + 2] - k) / (bin_points[m + 2] - bin_points[m + 1]);
                }
            }
        }
    }
    
    free(mel_points);
    free(hz_points);
    free(bin_points);
}

// Helper: Compute FFT magnitude (simple DFT for now)
static void compute_fft_magnitude(const float* frame, int fft_size, float* magnitude) {
    for (int k = 0; k < fft_size / 2; k++) {
        float real = 0.0f, imag = 0.0f;
        for (int n = 0; n < fft_size; n++) {
            float angle = -2.0f * M_PI * k * n / fft_size;
            real += frame[n] * cosf(angle);
            imag += frame[n] * sinf(angle);
        }
        magnitude[k] = sqrtf(real * real + imag * imag);
    }
}

ethervox_result_t pronunciation_trainer_extract_mels(const char* audio_path, int n_mels, float** mel_data, int* n_frames) {
    float* audio = NULL;
    int n_samples = 0;
    
    if (read_wav_audio(audio_path, &audio, &n_samples) != 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int num_frames = (n_samples - FFT_SIZE) / HOP_LENGTH + 1;
    *n_frames = num_frames;
    
    // Allocate mel spectrogram (n_mels x n_frames)
    *mel_data = (float*)calloc(n_mels * num_frames, sizeof(float));
    if (!*mel_data) {
        free(audio);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Create mel filterbank
    float** filterbank = (float**)malloc(n_mels * sizeof(float*));
    for (int i = 0; i < n_mels; i++) {
        filterbank[i] = (float*)malloc((FFT_SIZE / 2) * sizeof(float));
    }
    create_mel_filterbank(filterbank, n_mels, FFT_SIZE / 2, SAMPLE_RATE);
    
    // Process each frame
    float* fft_frame = (float*)malloc(FFT_SIZE * sizeof(float));
    float* magnitude = (float*)malloc((FFT_SIZE / 2) * sizeof(float));
    
    float mel_min = 1e10f;
    float mel_max = -1e10f;
    
    for (int frame_idx = 0; frame_idx < num_frames; frame_idx++) {
        int start = frame_idx * HOP_LENGTH;
        
        // Extract frame with Hann window
        for (int i = 0; i < FFT_SIZE; i++) {
            if (start + i < n_samples) {
                float window = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
                fft_frame[i] = audio[start + i] * window;
            } else {
                fft_frame[i] = 0.0f;
            }
        }
        
        // Compute FFT magnitude
        compute_fft_magnitude(fft_frame, FFT_SIZE, magnitude);
        
        // Apply mel filterbank
        for (int m = 0; m < n_mels; m++) {
            float mel_value = 0.0f;
            for (int k = 0; k < FFT_SIZE / 2; k++) {
                mel_value += magnitude[k] * filterbank[m][k];
            }
            // Convert to log scale
            float log_mel = logf(mel_value + 1e-10f);
            (*mel_data)[frame_idx * n_mels + m] = log_mel;
            
            if (log_mel < mel_min) mel_min = log_mel;
            if (log_mel > mel_max) mel_max = log_mel;
        }
    }
    
    // Normalize mel spectrogram to [0, 1] range for better DTW comparison
    float mel_range = mel_max - mel_min;
    if (mel_range > 0.0f) {
        for (int i = 0; i < n_mels * num_frames; i++) {
            (*mel_data)[i] = ((*mel_data)[i] - mel_min) / mel_range;
        }
    }
    
    fprintf(stderr, "[Mel Extract] Frames: %d, Mels: %d, Range: [%.2f, %.2f] -> normalized [0,1]\n",
            num_frames, n_mels, mel_min, mel_max);
    
    free(fft_frame);
    free(magnitude);
    for (int i = 0; i < n_mels; i++) {
        free(filterbank[i]);
    }
    free(filterbank);
    free(audio);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t pronunciation_trainer_dtw_distance(
    const float* mels1, int n_frames1,
    const float* mels2, int n_frames2,
    int n_mels, float* distance
) {
    // Dynamic Time Warping with accumulated cost matrix
    float** cost = (float**)malloc((n_frames1 + 1) * sizeof(float*));
    for (int i = 0; i <= n_frames1; i++) {
        cost[i] = (float*)malloc((n_frames2 + 1) * sizeof(float));
    }
    
    // Initialize with infinity
    for (int i = 0; i <= n_frames1; i++) {
        for (int j = 0; j <= n_frames2; j++) {
            cost[i][j] = INFINITY;
        }
    }
    cost[0][0] = 0.0f;
    
    // Compute DTW cost
    for (int i = 1; i <= n_frames1; i++) {
        for (int j = 1; j <= n_frames2; j++) {
            // Euclidean distance between frames
            float frame_dist = 0.0f;
            for (int m = 0; m < n_mels; m++) {
                float diff = mels1[(i-1) * n_mels + m] - mels2[(j-1) * n_mels + m];
                frame_dist += diff * diff;
            }
            frame_dist = sqrtf(frame_dist);
            
            // Find minimum path
            float min_cost = cost[i-1][j-1];
            if (cost[i-1][j] < min_cost) min_cost = cost[i-1][j];
            if (cost[i][j-1] < min_cost) min_cost = cost[i][j-1];
            
            cost[i][j] = frame_dist + min_cost;
        }
    }
    
    *distance = cost[n_frames1][n_frames2];
    
    // Normalize by shorter sequence length (better for speech comparison)
    int min_length = (n_frames1 < n_frames2) ? n_frames1 : n_frames2;
    if (min_length > 0) {
        *distance /= min_length;
    }
    
    fprintf(stderr, "[DTW] Raw distance: %.6f, normalized by min(%d,%d): %.6f\n",
            cost[n_frames1][n_frames2], n_frames1, n_frames2, *distance);
    
    for (int i = 0; i <= n_frames1; i++) {
        free(cost[i]);
    }
    free(cost);
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t pronunciation_trainer_compare_audio(const char* audio_path1, const char* audio_path2, float* similarity) {
    float* mels1 = NULL;
    float* mels2 = NULL;
    int n_frames1 = 0, n_frames2 = 0;
    
    ETHERVOX_LOG_DEBUG("[Audio Compare] Extracting mel spectrograms...");
    fprintf(stderr, "  Audio 1: %s\n", audio_path1);
    if (pronunciation_trainer_extract_mels(audio_path1, MEL_BANDS, &mels1, &n_frames1) != 0) {
        fprintf(stderr, "  ERROR: Failed to extract mels from audio 1\n");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    fprintf(stderr, "  Audio 1: %d frames extracted\n", n_frames1);
    
    fprintf(stderr, "  Audio 2: %s\n", audio_path2);
    if (pronunciation_trainer_extract_mels(audio_path2, MEL_BANDS, &mels2, &n_frames2) != 0) {
        fprintf(stderr, "  ERROR: Failed to extract mels from audio 2\n");
        free(mels1);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    fprintf(stderr, "  Audio 2: %d frames extracted\n", n_frames2);
    
    ETHERVOX_LOG_DEBUG("[Audio Compare] Computing DTW distance...");
    float distance = 0.0f;
    if (pronunciation_trainer_dtw_distance(mels1, n_frames1, mels2, n_frames2, MEL_BANDS, &distance) != 0) {
        fprintf(stderr, "  ERROR: DTW distance computation failed\n");
        free(mels1);
        free(mels2);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    fprintf(stderr, "  DTW distance: %.6f\n", distance);
    
    // Convert distance to similarity score (0-1)
    // Use exponential decay: similarity = exp(-distance)
    *similarity = expf(-distance);
    fprintf(stderr, "  Similarity score: %.6f (from exp(-%.6f))\n", *similarity, distance);
    
    free(mels1);
    free(mels2);
    return ETHERVOX_SUCCESS;
}

ethervox_result_t pronunciation_trainer_generate_variants(
    const char* word,
    const char* base_phonemes,
    phonemizer_context_t* phonemizer,
    int max_variants,
    char*** variants,
    int* variant_count
) {
    if (!word || !base_phonemes || !variants || !variant_count) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    *variants = (char**)malloc(MAX_VARIANTS * sizeof(char*));
    if (!*variants) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    *variant_count = 0;
    
    // Add base phonemes as first variant
    (*variants)[(*variant_count)++] = strdup(base_phonemes);
    
    // Generate stress variants
    char buffer[512];
    for (int stress_idx = 0; stress_patterns[stress_idx] && *variant_count < max_variants; stress_idx++) {
        // Add stress marker before first vowel
        const char* p = base_phonemes;
        char* out = buffer;
        bool stress_added = false;
        
        while (*p && out < buffer + sizeof(buffer) - 10) {
            // Check if character is a vowel
            if (!stress_added && strchr("iɪeɛæɑɔouʊʌəaː", *p)) {
                // Add stress marker
                const char* stress = stress_patterns[stress_idx];
                while (*stress) {
                    *out++ = *stress++;
                }
                stress_added = true;
            }
            *out++ = *p++;
        }
        *out = '\0';
        
        if (stress_added && strcmp(buffer, base_phonemes) != 0) {
            (*variants)[(*variant_count)++] = strdup(buffer);
        }
    }
    
    // Generate vowel alternative variants
    for (int vowel_set = 0; vowel_alternatives[vowel_set][0] && *variant_count < max_variants; vowel_set++) {
        const char* target = vowel_alternatives[vowel_set][0];
        
        // Check if base contains this vowel
        if (!strstr(base_phonemes, target)) continue;
        
        // Try each alternative
        for (int alt_idx = 1; vowel_alternatives[vowel_set][alt_idx] && *variant_count < max_variants; alt_idx++) {
            const char* replacement = vowel_alternatives[vowel_set][alt_idx];
            
            // Simple string replace (replace first occurrence)
            const char* pos = strstr(base_phonemes, target);
            if (pos) {
                size_t prefix_len = pos - base_phonemes;
                size_t target_len = strlen(target);
                size_t suffix_len = strlen(pos + target_len);
                
                if (prefix_len + strlen(replacement) + suffix_len < sizeof(buffer)) {
                    strncpy(buffer, base_phonemes, prefix_len);
                    strcpy(buffer + prefix_len, replacement);
                    strcpy(buffer + prefix_len + strlen(replacement), pos + target_len);
                    
                    (*variants)[(*variant_count)++] = strdup(buffer);
                }
            }
        }
    }
    
    // Limit to max_variants
    if (*variant_count > max_variants) {
        for (int i = max_variants; i < *variant_count; i++) {
            free((*variants)[i]);
        }
        *variant_count = max_variants;
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t pronunciation_trainer_train(
    const char* word,
    const char* user_audio_path,
    phonemizer_context_t* phonemizer,
    tts_context_t* tts,
    stt_context_t* stt,
    const pronunciation_training_config_t* config,
    pronunciation_training_result_t* result
) {
    if (!word || !user_audio_path || !phonemizer || !tts || !result) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Use default config if not provided
    pronunciation_training_config_t default_config = pronunciation_trainer_default_config();
    if (!config) config = &default_config;
    
    memset(result, 0, sizeof(pronunciation_training_result_t));
    
    if (config->verbose) {
        printf("Training pronunciation for: %s\n", word);
        printf("User audio: %s\n", user_audio_path);
    }
    
    // Get base phonemes from phonemizer
    char base_phonemes[512];
    if (phonemizer_text_to_ipa(phonemizer, word, base_phonemes, sizeof(base_phonemes)) != 0) {
        result->error_message = strdup("Failed to phonemize word");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (config->verbose) {
        printf("Base phonemes: %s\n", base_phonemes);
    }
    
    // Check if phonemizer returned empty string
    if (strlen(base_phonemes) == 0) {
        if (config->verbose) {
            ETHERVOX_LOG_ERROR("Phonemizer returned empty string for word '%s'\n", word);
        }
        result->error_message = strdup("Phonemizer returned empty result");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Generate variants
    char** variants = NULL;
    int variant_count = 0;
    if (pronunciation_trainer_generate_variants(word, base_phonemes, phonemizer, 
                                                config->max_variants, &variants, &variant_count) != 0) {
        result->error_message = strdup("Failed to generate phoneme variants");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (config->verbose) {
        printf("Generated %d phoneme variants\n", variant_count);
    }
    
    result->variants_tested = variant_count;
    float best_similarity = 0.0f;
    char* best_variant = NULL;
    char base_phonemes_copy[512];
    strncpy(base_phonemes_copy, base_phonemes, sizeof(base_phonemes_copy) - 1);
    base_phonemes_copy[sizeof(base_phonemes_copy) - 1] = '\0';
    
    // Test each variant
    for (int i = 0; i < variant_count; i++) {
        if (config->verbose) {
            printf("Testing variant %d/%d: %s\n", i + 1, variant_count, variants[i]);
        }
        
        // Synthesize this variant using IPA directly (bypass phonemizer)
        ethervox_tts_audio_t tts_output = {0};
        if (ethervox_tts_synthesize_ipa((ethervox_tts_context_t*)tts, variants[i], &tts_output) != 0) {
            if (config->verbose) {
                fprintf(stderr, "  Failed to synthesize variant %d\n", i);
            }
            continue;
        }
        
        // Save to temp file (ethervox_audio_write_wav expects float samples)
        char temp_path[256];
        snprintf(temp_path, sizeof(temp_path), "/tmp/ethervox_variant_%d.wav", i);
        
        if (ethervox_audio_write_wav(temp_path, tts_output.samples, tts_output.sample_count, 
                                     tts_output.sample_rate, tts_output.channels) != 0) {
            if (config->verbose) {
                fprintf(stderr, "  Failed to write variant audio to %s\n", temp_path);
            }
            ethervox_tts_audio_free(&tts_output);
            continue;
        }
        
        ethervox_tts_audio_free(&tts_output);
        
        // Compare synthesized variant to user audio
        float similarity = 0.0f;
        if (pronunciation_trainer_compare_audio(temp_path, user_audio_path, &similarity) == 0) {
            fprintf(stderr, "  Similarity score: %.3f%s\n", similarity, 
                    similarity > best_similarity ? " (new best!)" : "");
            if (similarity > best_similarity) {
                best_similarity = similarity;
                if (best_variant) free(best_variant);
                best_variant = strdup(variants[i]);
            }
        } else {
            fprintf(stderr, "  Failed to compare audio for variant %d\n", i);
        }
        
        // Clean up temp file unless debugging
        if (!config->save_audio_samples) {
            remove(temp_path);
        }
    }
    
    pronunciation_trainer_free_variants(variants, variant_count);
    
    fprintf(stderr, "\nBest similarity: %.3f (threshold: %.3f)\n", 
            best_similarity, config->min_similarity);
    
    if (best_similarity >= config->min_similarity && best_variant) {
        result->best_phonemes = best_variant;
        result->best_arpabet = NULL; // TODO: Convert IPA to ARPABET if needed
        result->similarity_score = best_similarity;
        result->success = true;
        
        if (config->verbose) {
            printf("Training successful! Best match: %s (score: %.3f)\n", 
                   best_variant, best_similarity);
        }
    } else {
        if (best_variant) free(best_variant);
        result->success = false;
        result->error_message = strdup("No variant met minimum similarity threshold");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    return ETHERVOX_SUCCESS;
}

void pronunciation_training_result_free(pronunciation_training_result_t* result) {
    if (!result) return;
    
    if (result->best_phonemes) free(result->best_phonemes);
    if (result->best_arpabet) free(result->best_arpabet);
    if (result->error_message) free(result->error_message);
    
    memset(result, 0, sizeof(pronunciation_training_result_t));
}

void pronunciation_trainer_free_variants(char** variants, int count) {
    if (!variants) return;
    
    for (int i = 0; i < count; i++) {
        if (variants[i]) free(variants[i]);
    }
    free(variants);
}
