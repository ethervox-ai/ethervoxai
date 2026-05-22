/**
 * @file test_aec_infrastructure.c
 * @brief Test suite for AEC infrastructure (reference buffer + Speex AEC)
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "ethervox/reference_buffer.h"
#include "ethervox/aec.h"

#define SAMPLE_RATE 16000
#define FRAME_SIZE 160  // 10ms @ 16kHz

// Generate sine wave test signal
static void generate_sine_wave(float* buffer, size_t count, float frequency, float amplitude) {
    for (size_t i = 0; i < count; i++) {
        float t = (float)i / SAMPLE_RATE;
        buffer[i] = amplitude * sinf(2.0f * M_PI * frequency * t);
    }
}

// Test 1: Reference buffer basic operations
static void test_reference_buffer_basic(void) {
    printf("\n=== Test 1: Reference Buffer Basic Operations ===\n");
    
    size_t capacity = 1000;
    ethervox_reference_buffer_t* buffer = ethervox_reference_buffer_create(capacity);
    assert(buffer != NULL);
    printf("✓ Created reference buffer with capacity %zu\n", capacity);
    
    // Test write
    float samples[100];
    generate_sine_wave(samples, 100, 440.0f, 0.5f);
    size_t written = ethervox_reference_buffer_write(buffer, samples, 100);
    assert(written == 100);
    printf("✓ Wrote %zu samples\n", written);
    
    // Check available count
    size_t available = ethervox_reference_buffer_available(buffer);
    assert(available == 100);
    printf("✓ Available samples: %zu\n", available);
    
    // Test read
    float output[100];
    size_t read_count = ethervox_reference_buffer_read(buffer, output, 100);
    assert(read_count == 100);
    printf("✓ Read %zu samples\n", read_count);
    
    // Verify data integrity
    for (size_t i = 0; i < 100; i++) {
        assert(fabsf(samples[i] - output[i]) < 0.0001f);
    }
    printf("✓ Data integrity verified\n");
    
    // Test empty buffer
    assert(ethervox_reference_buffer_is_empty(buffer));
    printf("✓ Buffer is empty after read\n");
    
    ethervox_reference_buffer_destroy(buffer);
    printf("✓ Reference buffer test passed\n");
}

// Test 2: Reference buffer wraparound
static void test_reference_buffer_wraparound(void) {
    printf("\n=== Test 2: Reference Buffer Wraparound ===\n");
    
    size_t capacity = 200;
    ethervox_reference_buffer_t* buffer = ethervox_reference_buffer_create(capacity);
    
    // Fill buffer to 75% capacity
    float samples[150];
    for (size_t i = 0; i < 150; i++) {
        samples[i] = (float)i;
    }
    ethervox_reference_buffer_write(buffer, samples, 150);
    printf("✓ Wrote 150 samples to 200-sample buffer\n");
    
    // Read 100 samples (leaves 50 in buffer)
    float output[100];
    ethervox_reference_buffer_read(buffer, output, 100);
    assert(ethervox_reference_buffer_available(buffer) == 50);
    printf("✓ Read 100 samples, 50 remaining\n");
    
    // Write 100 more (should wrap around)
    float more_samples[100];
    for (size_t i = 0; i < 100; i++) {
        more_samples[i] = (float)(i + 1000);
    }
    size_t written = ethervox_reference_buffer_write(buffer, more_samples, 100);
    assert(written == 100);
    assert(ethervox_reference_buffer_available(buffer) == 150);
    printf("✓ Wrote 100 samples with wraparound, total available: 150\n");
    
    ethervox_reference_buffer_destroy(buffer);
    printf("✓ Wraparound test passed\n");
}

// Test 3: Reference buffer overflow
static void test_reference_buffer_overflow(void) {
    printf("\n=== Test 3: Reference Buffer Overflow ===\n");
    
    size_t capacity = 100;
    ethervox_reference_buffer_t* buffer = ethervox_reference_buffer_create(capacity);
    
    // Try to write more than capacity
    float samples[150];
    for (size_t i = 0; i < 150; i++) {
        samples[i] = (float)i;
    }
    
    size_t written = ethervox_reference_buffer_write(buffer, samples, 150);
    // Should only write up to capacity
    assert(written <= capacity);
    printf("✓ Overflow protection: tried to write 150, wrote %zu\n", written);
    
    ethervox_reference_buffer_destroy(buffer);
    printf("✓ Overflow test passed\n");
}

// Test 4: AEC passthrough mode (no Speex)
static void test_aec_passthrough(void) {
    printf("\n=== Test 4: AEC Passthrough Mode ===\n");
    
    ethervox_aec_config_t config = ethervox_aec_default_config();
    config.backend = ETHERVOX_AEC_NONE;
    
    ethervox_aec_t* aec = ethervox_aec_create(&config);
    assert(aec != NULL);
    printf("✓ Created AEC with NONE backend\n");
    
    // Should not be active
    assert(!ethervox_aec_is_active(aec));
    printf("✓ AEC is inactive (passthrough mode)\n");
    
    // Process should pass through unchanged
    float input[FRAME_SIZE];
    float original[FRAME_SIZE];
    generate_sine_wave(input, FRAME_SIZE, 440.0f, 0.5f);
    memcpy(original, input, sizeof(input));
    
    ethervox_result_t result = ethervox_aec_process(aec, input, FRAME_SIZE);
    assert(result == ETHERVOX_SUCCESS);
    
    // Verify input unchanged (passthrough)
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        assert(fabsf(input[i] - original[i]) < 0.0001f);
    }
    printf("✓ Passthrough verified: input unchanged\n");
    
    ethervox_aec_destroy(aec);
    printf("✓ AEC passthrough test passed\n");
}

// Test 5: AEC Speex backend (if available)
static void test_aec_speex(void) {
    printf("\n=== Test 5: AEC Speex Backend ===\n");
    
    ethervox_aec_config_t config = ethervox_aec_default_config();
    config.backend = ETHERVOX_AEC_SPEEX;
    config.sample_rate = SAMPLE_RATE;
    config.frame_size = FRAME_SIZE;
    config.filter_length = 1024;
    config.suppression_level = 0.5f;
    
    ethervox_aec_t* aec = ethervox_aec_create(&config);
    if (!aec) {
        printf("⚠️  Speex AEC not available (library not found)\n");
        printf("   Install with: brew install speex (macOS) or apt-get install libspeex-dev (Linux)\n");
        return;
    }
    
    printf("✓ Created AEC with Speex backend\n");
    
    // Should be active
    assert(ethervox_aec_is_active(aec));
    printf("✓ AEC is active\n");
    
    // Generate reference signal (440Hz sine wave = TTS output)
    float reference[FRAME_SIZE];
    generate_sine_wave(reference, FRAME_SIZE, 440.0f, 0.5f);
    
    // Set reference
    ethervox_aec_set_reference(aec, reference, FRAME_SIZE);
    printf("✓ Set reference signal (440Hz sine wave)\n");
    
    // Generate microphone input with echo + voice
    float mic_input[FRAME_SIZE];
    float voice[FRAME_SIZE];
    generate_sine_wave(voice, FRAME_SIZE, 300.0f, 0.3f);  // 300Hz voice
    
    // Mix: 50% echo + 50% voice
    float original_input[FRAME_SIZE];
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        mic_input[i] = 0.5f * reference[i] + 0.5f * voice[i];
        original_input[i] = mic_input[i];
    }
    printf("✓ Generated mic input (50%% echo + 50%% voice)\n");
    
    // Process with AEC (in-place)
    ethervox_result_t result = ethervox_aec_process(aec, mic_input, FRAME_SIZE);
    assert(result == ETHERVOX_SUCCESS);
    printf("✓ AEC processing successful\n");
    
    // Calculate RMS energy of original vs processed
    float input_rms = 0.0f, output_rms = 0.0f;
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        input_rms += original_input[i] * original_input[i];
        output_rms += mic_input[i] * mic_input[i];  // mic_input now contains processed output
    }
    input_rms = sqrtf(input_rms / FRAME_SIZE);
    output_rms = sqrtf(output_rms / FRAME_SIZE);
    
    printf("   Input RMS:  %.4f\n", input_rms);
    printf("   Output RMS: %.4f\n", output_rms);
    printf("   Echo reduction: %.1f%%\n", (1.0f - output_rms / input_rms) * 100.0f);
    
    // Reset AEC state
    ethervox_aec_reset(aec);
    printf("✓ AEC reset\n");
    
    ethervox_aec_destroy(aec);
    printf("✓ AEC Speex test passed\n");
}

// Test 6: Full AEC + reference buffer workflow
static void test_full_workflow(void) {
    printf("\n=== Test 6: Full AEC + Reference Buffer Workflow ===\n");
    
    // Create reference buffer (2 seconds @ 16kHz)
    ethervox_reference_buffer_t* ref_buffer = ethervox_reference_buffer_create(SAMPLE_RATE * 2);
    assert(ref_buffer != NULL);
    printf("✓ Created 2-second reference buffer\n");
    
    // Create AEC
    ethervox_aec_config_t config = ethervox_aec_default_config();
    ethervox_aec_t* aec = ethervox_aec_create(&config);
    assert(aec != NULL);
    printf("✓ Created AEC engine\n");
    
    // Simulate TTS generating 5 frames of audio
    printf("\nSimulating TTS playback + microphone capture:\n");
    for (int frame = 0; frame < 5; frame++) {
        // TTS generates samples
        float tts_samples[FRAME_SIZE];
        generate_sine_wave(tts_samples, FRAME_SIZE, 440.0f + frame * 50.0f, 0.5f);
        
        // Write to reference buffer (TTS playback thread)
        size_t written = ethervox_reference_buffer_write(ref_buffer, tts_samples, FRAME_SIZE);
        assert(written == FRAME_SIZE);
        
        // Read reference for AEC (microphone capture thread)
        float reference[FRAME_SIZE];
        size_t read = ethervox_reference_buffer_read(ref_buffer, reference, FRAME_SIZE);
        assert(read == FRAME_SIZE);
        
        // Set AEC reference
        ethervox_aec_set_reference(aec, reference, FRAME_SIZE);
        
        // Simulate microphone input (with echo)
        float mic_input[FRAME_SIZE];
        for (size_t i = 0; i < FRAME_SIZE; i++) {
            mic_input[i] = 0.7f * reference[i];  // 70% echo
        }
        
        // Process with AEC (in-place)
        ethervox_result_t process_result = ethervox_aec_process(aec, mic_input, FRAME_SIZE);
        assert(process_result == ETHERVOX_SUCCESS);
        
        printf("  Frame %d: processed %d samples\n", frame + 1, FRAME_SIZE);
    }
    
    printf("✓ Full workflow test passed\n");
    
    ethervox_reference_buffer_destroy(ref_buffer);
    ethervox_aec_destroy(aec);
}

int main(void) {
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║      EthervoxAI AEC Infrastructure Test Suite         ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    // Run tests
    test_reference_buffer_basic();
    test_reference_buffer_wraparound();
    test_reference_buffer_overflow();
    test_aec_passthrough();
    test_aec_speex();
    test_full_workflow();
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║              ✓ All Tests Passed                        ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    return ETHERVOX_SUCCESS;
}
