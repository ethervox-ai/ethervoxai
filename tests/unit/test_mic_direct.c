/**
 * Direct microphone test - bypasses conversation system
 * Tests: CoreAudio capture → Whisper STT
 * (Using Whisper instead of Vosk since Vosk requires complex build)
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ethervox/audio.h"
#include "ethervox/stt.h"
#include "ethervox/logging.h"

int main() {
    printf("========================================\n");
    printf("Direct Microphone → Whisper STT Test\n");
    printf("========================================\n\n");
    
    // Initialize audio
    printf("1. Initializing CoreAudio...\n");
    ethervox_audio_runtime_t audio_runtime = {0};
    
    if (ethervox_audio_register_platform_driver(&audio_runtime) != 0) {
        fprintf(stderr, "❌ Failed to register audio driver\n");
        return ETHERVOX_SUCCESS;
    }
    
    ethervox_audio_config_t audio_config = {0};
    audio_config.sample_rate = 16000;
    audio_config.channels = 1;
    audio_config.bits_per_sample = 16;
    audio_config.buffer_size = 4096;
    
    if (audio_runtime.driver.init(&audio_runtime, &audio_config) != 0) {
        fprintf(stderr, "❌ Failed to initialize audio\n");
        return ETHERVOX_SUCCESS;
    }
    printf("✓ CoreAudio initialized\n\n");
    
    // Initialize Whisper STT (Vosk not available, using Whisper instead)
    printf("2. Initializing Whisper STT...\n");
    ethervox_stt_runtime_t stt_runtime = {0};
    ethervox_stt_config_t stt_config = ethervox_stt_get_default_config();
    stt_config.backend = ETHERVOX_STT_BACKEND_WHISPER;
    
    // Use base.bin model
    const char* home = getenv("HOME");
    static char model_path[512];
    snprintf(model_path, sizeof(model_path), "%s/.ethervox/models/whisper/base.bin", home);
    stt_config.model_path = model_path;
    
    stt_config.sample_rate = 16000;
    stt_config.enable_partial_results = 1; // Streaming mode
    
    if (ethervox_stt_init(&stt_runtime, &stt_config) != 0) {
        fprintf(stderr, "❌ Failed to initialize Whisper\n");
        fprintf(stderr, "   Check if model exists: %s\n", model_path);
        return ETHERVOX_SUCCESS;
    }
    printf("✓ Whisper STT initialized\n\n");
    
    // Start STT session
    if (ethervox_stt_start(&stt_runtime) != 0) {
        fprintf(stderr, "❌ Failed to start STT\n");
        return ETHERVOX_SUCCESS;
    }
    
    // Start audio capture
    printf("3. Starting microphone capture...\n");
    if (audio_runtime.driver.start_capture(&audio_runtime) != 0) {
        fprintf(stderr, "❌ Failed to start capture\n");
        return ETHERVOX_SUCCESS;
    }
    printf("✓ Microphone capture started\n\n");
    
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  🎤 SPEAK NOW! (5 seconds)                   ║\n");
    printf("║                                              ║\n");
    printf("║  Try: 'Hello', 'Test', 'What time is it'    ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    
    // Listen for 5 seconds
    int iterations = 250; // 5 seconds / 20ms
    bool got_speech = false;
    
    for (int i = 0; i < iterations; i++) {
        // Read 20ms of audio (320 samples at 16kHz)
        ethervox_audio_buffer_t buffer;
        buffer.size = 320;
        buffer.channels = 1;
        buffer.data = (float*)calloc(320, sizeof(float));
        
        if (!buffer.data) {
            fprintf(stderr, "Memory allocation failed\n");
            break;
        }
        
        int samples = audio_runtime.driver.read_audio(&audio_runtime, &buffer);
        
        if (samples > 0) {
            // Process with Vosk
            ethervox_stt_result_t result;
            memset(&result, 0, sizeof(result));
            
            if (ethervox_stt_process(&stt_runtime, &buffer, &result) == 0) {
                if (result.is_partial && result.text && strlen(result.text) > 0) {
                    printf("  [Partial] %s\n", result.text);
                    got_speech = true;
                    free(result.text);
                }
                if (result.is_final && result.text && strlen(result.text) > 0) {
                    printf("\n✓ [FINAL] %s\n\n", result.text);
                    got_speech = true;
                    free(result.text);
                    free(buffer.data);
                    break;
                }
            }
        }
        
        free(buffer.data);
        usleep(20000); // 20ms
    }
    
    // Finalize to get any remaining text
    ethervox_stt_result_t final_result;
    if (ethervox_stt_finalize(&stt_runtime, &final_result) == 0) {
        if (final_result.text && strlen(final_result.text) > 0) {
            printf("✓ [Finalized] %s\n", final_result.text);
            got_speech = true;
            free(final_result.text);
        }
    }
    
    printf("\n========================================\n");
    if (got_speech) {
        printf("✅ SUCCESS: Speech detected and processed!\n");
    } else {
        printf("⚠️  No speech detected (try speaking louder)\n");
    }
    printf("========================================\n");
    
    // Cleanup
    ethervox_stt_stop(&stt_runtime);
    ethervox_stt_cleanup(&stt_runtime);
    audio_runtime.driver.stop_capture(&audio_runtime);
    audio_runtime.driver.cleanup(&audio_runtime);
    
    return got_speech ? 0 : 1;
}
