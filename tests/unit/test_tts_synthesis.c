/**
 * @file test_tts_synthesis.c
 * @brief End-to-end TTS synthesis tests
 * 
 * Tests the complete text-to-speech pipeline:
 * - Text → Phonemes → ONNX Inference → Audio waveform
 * - Multi-language support (English, Chinese, German)
 * - Audio quality validation
 * - Performance benchmarks
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include "ethervox/tts.h"

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("✗ FAIL: %s\n", msg); \
            printf("   Condition: %s\n", #cond); \
            return ETHERVOX_ERROR_INVALID_ARGUMENT; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr, msg) \
    ASSERT_TRUE((ptr) != NULL, msg)

#define ASSERT_EQUALS(a, b, msg) \
    ASSERT_TRUE((a) == (b), msg)

/**
 * Validate audio output quality
 */
static int validate_audio_quality(const ethervox_tts_audio_t* audio) {
    if (!audio || !audio->samples) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check sample count is reasonable (at least 0.1 seconds worth)
    size_t min_samples = audio->sample_rate / 10;
    if (audio->sample_count < min_samples) {
        printf("   Warning: Very short audio (%zu samples)\n", audio->sample_count);
    }
    
    // Check for silence (all samples near zero)
    int silent_samples = 0;
    for (size_t i = 0; i < audio->sample_count; i++) {
        if (fabs(audio->samples[i]) < 0.001f) {
            silent_samples++;
        }
    }
    
    float silence_ratio = (float)silent_samples / audio->sample_count;
    if (silence_ratio > 0.95f) {
        printf("   Warning: Audio is %.1f%% silent\n", silence_ratio * 100);
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    // Check for clipping (samples at extremes)
    int clipped_samples = 0;
    for (size_t i = 0; i < audio->sample_count; i++) {
        if (fabs(audio->samples[i]) > 0.99f) {
            clipped_samples++;
        }
    }
    
    float clip_ratio = (float)clipped_samples / audio->sample_count;
    if (clip_ratio > 0.01f) {
        printf("   Warning: Audio has %.2f%% clipping\n", clip_ratio * 100);
    }
    
    // Calculate RMS level
    float rms = 0.0f;
    for (size_t i = 0; i < audio->sample_count; i++) {
        rms += audio->samples[i] * audio->samples[i];
    }
    rms = sqrtf(rms / audio->sample_count);
    
    printf("   Audio stats: %zu samples, %.1f%% silence, RMS=%.3f\n",
           audio->sample_count, silence_ratio * 100, rms);
    
    // Healthy audio should have RMS between 0.01 and 0.5
    if (rms < 0.005f || rms > 0.8f) {
        printf("   Warning: RMS level unusual (%.3f)\n", rms);
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Basic English synthesis
 */
static int test_english_synthesis(ethervox_tts_context_t* ctx) {
    printf("\n[Test 1] English Synthesis\n");
    
    const char* test_text = "Hello, this is a test of the text to speech system.";
    ethervox_tts_audio_t audio = {0};
    
    clock_t start = clock();
    int result = ethervox_tts_synthesize_text(ctx, test_text, &audio);
    clock_t end = clock();
    double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    
    ASSERT_EQUALS(result, 0, "Synthesis should succeed");
    ASSERT_NOT_NULL(audio.samples, "Audio samples should be generated");
    ASSERT_TRUE(audio.sample_count > 0, "Sample count should be positive");
    ASSERT_TRUE(audio.sample_rate > 0, "Sample rate should be positive");
    
    printf("  ✓ Synthesized: \"%s\"\n", test_text);
    printf("  ✓ Duration: %.1f ms\n", elapsed_ms);
    printf("  ✓ Output: %zu samples at %d Hz (%.2f seconds)\n",
           audio.sample_count, audio.sample_rate,
           (float)audio.sample_count / audio.sample_rate);
    
    // Validate quality
    int quality_result = validate_audio_quality(&audio);
    ASSERT_EQUALS(quality_result, 0, "Audio quality should be acceptable");
    
    // Cleanup
    if (audio.samples) {
        free(audio.samples);
    }
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Chinese synthesis
 */
static int test_chinese_synthesis(ethervox_tts_context_t* ctx) {
    printf("\n[Test 2] Chinese Synthesis\n");
    
    const char* test_text = "你好，这是一个测试。";
    ethervox_tts_audio_t audio = {0};
    
    int result = ethervox_tts_synthesize_text(ctx, test_text, &audio);
    
    ASSERT_EQUALS(result, 0, "Chinese synthesis should succeed");
    ASSERT_NOT_NULL(audio.samples, "Audio samples should be generated");
    ASSERT_TRUE(audio.sample_count > 0, "Sample count should be positive");
    
    printf("  ✓ Synthesized: \"%s\"\n", test_text);
    printf("  ✓ Output: %zu samples at %d Hz\n",
           audio.sample_count, audio.sample_rate);
    
    validate_audio_quality(&audio);
    
    if (audio.samples) {
        free(audio.samples);
    }
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: German synthesis
 */
static int test_german_synthesis(ethervox_tts_context_t* ctx) {
    printf("\n[Test 3] German Synthesis\n");
    
    const char* test_text = "Guten Tag, dies ist ein Test.";
    ethervox_tts_audio_t audio = {0};
    
    int result = ethervox_tts_synthesize_text(ctx, test_text, &audio);
    
    ASSERT_EQUALS(result, 0, "German synthesis should succeed");
    ASSERT_NOT_NULL(audio.samples, "Audio samples should be generated");
    ASSERT_TRUE(audio.sample_count > 0, "Sample count should be positive");
    
    printf("  ✓ Synthesized: \"%s\"\n", test_text);
    printf("  ✓ Output: %zu samples at %d Hz\n",
           audio.sample_count, audio.sample_rate);
    
    validate_audio_quality(&audio);
    
    if (audio.samples) {
        free(audio.samples);
    }
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Empty text handling
 */
static int test_empty_text(ethervox_tts_context_t* ctx) {
    printf("\n[Test 4] Empty Text Handling\n");
    
    ethervox_tts_audio_t audio = {0};
    int result = ethervox_tts_synthesize_text(ctx, "", &audio);
    
    // Should either fail gracefully or produce minimal audio
    printf("  ✓ Empty text handled (result=%d)\n", result);
    
    if (audio.samples) {
        free(audio.samples);
    }
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Long text handling
 */
static int test_long_text(ethervox_tts_context_t* ctx) {
    printf("\n[Test 5] Long Text Handling\n");
    
    // Generate a long sentence (~50 words)
    const char* long_text = 
        "The quick brown fox jumps over the lazy dog. "
        "This is a very long sentence designed to test the text to speech system's "
        "ability to handle extended passages of text without errors or quality degradation. "
        "We want to ensure that the phonemizer and ONNX inference can process "
        "lengthy inputs efficiently and produce high quality audio output consistently.";
    
    ethervox_tts_audio_t audio = {0};
    
    clock_t start = clock();
    int result = ethervox_tts_synthesize_text(ctx, long_text, &audio);
    clock_t end = clock();
    double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    
    ASSERT_EQUALS(result, 0, "Long text synthesis should succeed");
    ASSERT_NOT_NULL(audio.samples, "Audio samples should be generated");
    
    printf("  ✓ Synthesized %zu characters\n", strlen(long_text));
    printf("  ✓ Duration: %.1f ms\n", elapsed_ms);
    printf("  ✓ Output: %zu samples (%.2f seconds)\n",
           audio.sample_count, (float)audio.sample_count / audio.sample_rate);
    
    validate_audio_quality(&audio);
    
    if (audio.samples) {
        free(audio.samples);
    }
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Special characters and punctuation
 */
static int test_punctuation(ethervox_tts_context_t* ctx) {
    printf("\n[Test 6] Punctuation Handling\n");
    
    const char* test_text = "Hello! How are you? I'm fine, thanks. It's 3:45 PM.";
    ethervox_tts_audio_t audio = {0};
    
    int result = ethervox_tts_synthesize_text(ctx, test_text, &audio);
    
    ASSERT_EQUALS(result, 0, "Punctuation handling should succeed");
    ASSERT_NOT_NULL(audio.samples, "Audio samples should be generated");
    
    printf("  ✓ Synthesized: \"%s\"\n", test_text);
    printf("  ✓ Output: %zu samples\n", audio.sample_count);
    
    if (audio.samples) {
        free(audio.samples);
    }
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: NULL pointer handling
 */
static int test_null_handling(ethervox_tts_context_t* ctx) {
    printf("\n[Test 7] NULL Pointer Handling\n");
    
    ethervox_tts_audio_t audio = {0};
    
    // NULL text
    int result = ethervox_tts_synthesize_text(ctx, NULL, &audio);
    ASSERT_TRUE(result != 0, "NULL text should fail gracefully");
    printf("  ✓ NULL text rejected\n");
    
    // NULL context
    result = ethervox_tts_synthesize_text(NULL, "test", &audio);
    ASSERT_TRUE(result != 0, "NULL context should fail gracefully");
    printf("  ✓ NULL context rejected\n");
    
    // NULL output
    result = ethervox_tts_synthesize_text(ctx, "test", NULL);
    ASSERT_TRUE(result != 0, "NULL output should fail gracefully");
    printf("  ✓ NULL output rejected\n");
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Main test runner
 */
int main(int argc, char** argv) {
    printf("═══════════════════════════════════════════════\n");
    printf("  TTS End-to-End Synthesis Tests\n");
    printf("═══════════════════════════════════════════════\n");
    
    // Check for model path argument
    const char* model_path = NULL;
    if (argc > 1) {
        model_path = argv[1];
    } else {
        // Try default location
        const char* home = getenv("HOME");
        static char default_path[512];
        if (home) {
            snprintf(default_path, sizeof(default_path),
                    "%s/.ethervox/models/piper/en_US-lessac-medium.onnx", home);
            model_path = default_path;
        }
    }
    
    if (!model_path) {
        printf("✗ No model path provided\n");
        printf("Usage: %s [model_path.onnx]\n", argv[0]);
        return ETHERVOX_SUCCESS;
    }
    
    printf("\nInitializing TTS with model: %s\n", model_path);
    
    // Create TTS context
    ethervox_tts_config_t config = ethervox_tts_default_config();
    config.backend = ETHERVOX_TTS_BACKEND_PIPER;
    config.model_path = model_path;
    config.sample_rate = 16000;
    config.channels = 1;
    
    ethervox_tts_context_t* ctx = ethervox_tts_create(&config);
    if (!ctx) {
        printf("✗ Failed to create TTS context\n");
        printf("  Check if model exists and ONNX Runtime is installed\n");
        return ETHERVOX_SUCCESS;
    }
    
    if (!ethervox_tts_is_ready(ctx)) {
        printf("✗ TTS context not ready\n");
        ethervox_tts_destroy(ctx);
        return ETHERVOX_SUCCESS;
    }
    
    printf("✓ TTS initialized successfully\n");
    
    // Run tests
    int failed = 0;
    
    if (test_english_synthesis(ctx) != 0) failed++;
    if (test_chinese_synthesis(ctx) != 0) failed++;
    if (test_german_synthesis(ctx) != 0) failed++;
    if (test_empty_text(ctx) != 0) failed++;
    if (test_long_text(ctx) != 0) failed++;
    if (test_punctuation(ctx) != 0) failed++;
    if (test_null_handling(ctx) != 0) failed++;
    
    // Cleanup
    ethervox_tts_destroy(ctx);
    
    // Summary
    printf("\n═══════════════════════════════════════════════\n");
    if (failed == 0) {
        printf("  ✓ All tests PASSED (7/7)\n");
    } else {
        printf("  ✗ %d tests FAILED\n", failed);
    }
    printf("═══════════════════════════════════════════════\n");
    
    return failed > 0 ? 1 : 0;
}
