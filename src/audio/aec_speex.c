/**
 * @file aec_speex.c
 * @brief Speex Acoustic Echo Cancellation wrapper
 */

#include "ethervox/aec.h"
#include "ethervox/logging.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_SPEEXDSP
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#endif

struct ethervox_aec_s {
#ifdef HAVE_SPEEXDSP
    SpeexEchoState* echo_state;        // Speex echo cancellation state
    SpeexPreprocessState* preprocess;  // Optional preprocessing (noise suppression, AGC)
#endif
    
    ethervox_aec_config_t config;      // Configuration
    
    float* reference_frame;            // Reference signal buffer (one frame)
    float* input_frame;                // Input signal buffer (one frame)
    float* output_frame;               // Output signal buffer (one frame)
    
    int16_t* reference_i16;            // int16 conversion buffer for Speex
    int16_t* input_i16;                // int16 conversion buffer for Speex
    int16_t* output_i16;               // int16 conversion buffer for Speex
    
    bool active;                       // AEC enabled/disabled
};

// Convert float32 [-1.0, 1.0] to int16 [-32768, 32767]
static void float_to_int16(const float* in, int16_t* out, size_t count) {
    for (size_t i = 0; i < count; i++) {
        float sample = in[i];
        // Clamp to [-1.0, 1.0]
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out[i] = (int16_t)(sample * 32767.0f);
    }
}

// Convert int16 [-32768, 32767] to float32 [-1.0, 1.0]
static void int16_to_float(const int16_t* in, float* out, size_t count) {
    for (size_t i = 0; i < count; i++) {
        out[i] = (float)in[i] / 32768.0f;
    }
}

ethervox_aec_config_t ethervox_aec_default_config(void) {
    ethervox_aec_config_t config = {
        .sample_rate = 16000,
        .frame_size = 160,           // 10ms @ 16kHz
        .filter_length = 1024,       // ~64ms echo tail
        .backend = ETHERVOX_AEC_SPEEX,
        .suppression_level = 0.5f,   // Moderate suppression
    };
    return config;
}

#ifdef HAVE_SPEEXDSP

ethervox_aec_t* ethervox_aec_create(const ethervox_aec_config_t* config) {
    if (!config) {
        ETHERVOX_LOG_ERROR("Null AEC configuration");
        return NULL;
    }
    
    if (config->backend != ETHERVOX_AEC_SPEEX && config->backend != ETHERVOX_AEC_NONE) {
        ETHERVOX_LOG_ERROR("Unsupported AEC backend: %d", config->backend);
        return NULL;
    }
    
    if (config->backend == ETHERVOX_AEC_NONE) {
        // Passthrough mode - no actual AEC
        ethervox_aec_t* aec = (ethervox_aec_t*)calloc(1, sizeof(*aec));
        if (!aec) {
            return NULL;
        }
        aec->config = *config;
        aec->active = false;
        ETHERVOX_LOG_INFO("AEC created in passthrough mode (NONE backend)");
        return aec;
    }
    
    // Validate parameters
    if (config->sample_rate != 8000 && config->sample_rate != 16000 && 
        config->sample_rate != 32000 && config->sample_rate != 48000) {
        ETHERVOX_LOG_ERROR("Unsupported sample rate: %d (must be 8/16/32/48 kHz)", config->sample_rate);
        return NULL;
    }
    
    if (config->frame_size <= 0 || config->frame_size > 1024) {
        ETHERVOX_LOG_ERROR("Invalid frame size: %d (must be 1-1024 samples)", config->frame_size);
        return NULL;
    }
    
    if (config->filter_length <= 0 || config->filter_length > 8192) {
        ETHERVOX_LOG_ERROR("Invalid filter length: %d (must be 1-8192 samples)", config->filter_length);
        return NULL;
    }
    
    // Allocate AEC structure
    ethervox_aec_t* aec = (ethervox_aec_t*)calloc(1, sizeof(*aec));
    if (!aec) {
        ETHERVOX_LOG_ERROR("Failed to allocate AEC structure");
        return NULL;
    }
    
    aec->config = *config;
    
    // Allocate buffers
    aec->reference_frame = (float*)calloc(config->frame_size, sizeof(float));
    aec->input_frame = (float*)calloc(config->frame_size, sizeof(float));
    aec->output_frame = (float*)calloc(config->frame_size, sizeof(float));
    aec->reference_i16 = (int16_t*)calloc(config->frame_size, sizeof(int16_t));
    aec->input_i16 = (int16_t*)calloc(config->frame_size, sizeof(int16_t));
    aec->output_i16 = (int16_t*)calloc(config->frame_size, sizeof(int16_t));
    
    if (!aec->reference_frame || !aec->input_frame || !aec->output_frame ||
        !aec->reference_i16 || !aec->input_i16 || !aec->output_i16) {
        ETHERVOX_LOG_ERROR("Failed to allocate AEC buffers");
        ethervox_aec_destroy(aec);
        return NULL;
    }
    
    // Create Speex echo state
    aec->echo_state = speex_echo_state_init(config->frame_size, config->filter_length);
    if (!aec->echo_state) {
        ETHERVOX_LOG_ERROR("Failed to initialize Speex echo state");
        ethervox_aec_destroy(aec);
        return NULL;
    }
    
    // Set sample rate (cast to avoid const warning)
    int sample_rate = config->sample_rate;
    speex_echo_ctl(aec->echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &sample_rate);
    
    // Create preprocessor for additional noise suppression
    aec->preprocess = speex_preprocess_state_init(config->frame_size, config->sample_rate);
    if (!aec->preprocess) {
        ETHERVOX_LOG_ERROR("Failed to initialize Speex preprocessor");
        ethervox_aec_destroy(aec);
        return NULL;
    }
    
    // Configure preprocessor
    int denoise = (config->suppression_level > 0.1f) ? 1 : 0;
    int agc = 0;  // Disable AGC for now (could make voice level unpredictable)
    
    speex_preprocess_ctl(aec->preprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
    speex_preprocess_ctl(aec->preprocess, SPEEX_PREPROCESS_SET_AGC, &agc);
    
    // Link echo state to preprocessor
    speex_preprocess_ctl(aec->preprocess, SPEEX_PREPROCESS_SET_ECHO_STATE, aec->echo_state);
    
    aec->active = true;
    
    ETHERVOX_LOG_INFO("Speex AEC created: %d Hz, frame=%d samples (%.1f ms), filter=%d samples (%.1f ms)",
                      config->sample_rate, 
                      config->frame_size,
                      (float)config->frame_size * 1000.0f / (float)config->sample_rate,
                      config->filter_length,
                      (float)config->filter_length * 1000.0f / (float)config->sample_rate);
    
    return aec;
}

void ethervox_aec_set_reference(ethervox_aec_t* aec, const float* reference, size_t count) {
    if (!aec) {
        return;
    }
    
    if (!aec->active || aec->config.backend == ETHERVOX_AEC_NONE) {
        // Passthrough mode or inactive - no reference needed
        return;
    }
    
    if (!reference || count != (size_t)aec->config.frame_size) {
        ETHERVOX_LOG_ERROR("Invalid reference signal: expected %d samples, got %zu", 
                           aec->config.frame_size, count);
        return;
    }
    
    // Store reference frame
    memcpy(aec->reference_frame, reference, count * sizeof(float));
}

ethervox_result_t ethervox_aec_process(ethervox_aec_t* aec, 
                        float* mic_input, 
                        size_t count) {
    ETHERVOX_CHECK_PTR(aec);
    ETHERVOX_CHECK_PTR(mic_input);
    
    if (count != (size_t)aec->config.frame_size) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT,
            "Frame size mismatch: expected %d, got %zu", aec->config.frame_size, count);
    }
    
    // Passthrough mode or inactive - no modification needed
    if (!aec->active || aec->config.backend == ETHERVOX_AEC_NONE) {
        return ETHERVOX_SUCCESS;
    }
    
    // Convert float to int16 for Speex
    float_to_int16(aec->reference_frame, aec->reference_i16, count);
    float_to_int16(mic_input, aec->input_i16, count);
    
    // Perform echo cancellation
    speex_echo_cancellation(aec->echo_state, 
                           aec->input_i16,      // Microphone input (with echo)
                           aec->reference_i16,  // Speaker output (reference)
                           aec->output_i16);    // Cleaned output
    
    // Apply preprocessing (noise suppression)
    int vad = speex_preprocess_run(aec->preprocess, aec->output_i16);
    
    // Log AEC metrics periodically (every ~100 frames = ~1 second at 10ms frames)
    static int frame_count = 0;
    frame_count++;
    if (frame_count % 100 == 0) {
        // Calculate RMS energy before and after
        float input_rms = 0.0f, output_rms = 0.0f;
        for (size_t i = 0; i < count; i++) {
            input_rms += (float)(aec->input_i16[i] * aec->input_i16[i]);
            output_rms += (float)(aec->output_i16[i] * aec->output_i16[i]);
        }
        input_rms = sqrtf(input_rms / count);
        output_rms = sqrtf(output_rms / count);
        
        float suppression_db = 20.0f * log10f((input_rms + 1.0f) / (output_rms + 1.0f));
        
        ETHERVOX_LOG_DEBUG("[AEC] Echo suppression: %.1f dB, VAD: %s, Input RMS: %.0f, Output RMS: %.0f",
                          suppression_db, vad ? "VOICE" : "SILENCE", input_rms, output_rms);
    }
    
    // Convert back to float (in-place)
    int16_to_float(aec->output_i16, mic_input, count);
    
    return ETHERVOX_SUCCESS;
}

bool ethervox_aec_is_active(const ethervox_aec_t* aec) {
    return aec ? aec->active : false;
}

void ethervox_aec_reset(ethervox_aec_t* aec) {
    if (!aec || !aec->active || aec->config.backend == ETHERVOX_AEC_NONE) {
        return;
    }
    
    if (aec->echo_state) {
        speex_echo_state_reset(aec->echo_state);
    }
    
    // Clear buffers
    if (aec->reference_frame) {
        memset(aec->reference_frame, 0, aec->config.frame_size * sizeof(float));
    }
    if (aec->input_frame) {
        memset(aec->input_frame, 0, aec->config.frame_size * sizeof(float));
    }
    if (aec->output_frame) {
        memset(aec->output_frame, 0, aec->config.frame_size * sizeof(float));
    }
    
    ETHERVOX_LOG_DEBUG("AEC reset");
}

void ethervox_aec_destroy(ethervox_aec_t* aec) {
    if (!aec) {
        return;
    }
    
    if (aec->preprocess) {
        speex_preprocess_state_destroy(aec->preprocess);
    }
    
    if (aec->echo_state) {
        speex_echo_state_destroy(aec->echo_state);
    }
    
    free(aec->reference_frame);
    free(aec->input_frame);
    free(aec->output_frame);
    free(aec->reference_i16);
    free(aec->input_i16);
    free(aec->output_i16);
    
    free(aec);
    
    ETHERVOX_LOG_DEBUG("AEC destroyed");
}

#else  // !HAVE_SPEEXDSP

// Stub implementations when SpeexDSP is not available (e.g., Android builds)

ethervox_aec_t* ethervox_aec_create(const ethervox_aec_config_t* config) {
    ETHERVOX_LOG_WARN("SpeexDSP not available - AEC disabled (passthrough mode)");
    return NULL;  // Null AEC context means passthrough in the calling code
}

void ethervox_aec_set_reference(ethervox_aec_t* aec, const float* reference, size_t count) {
    // No-op
}

ethervox_result_t ethervox_aec_process(ethervox_aec_t* aec, float* mic_input, size_t count) {
    // Passthrough - no processing
    return ETHERVOX_SUCCESS;
}

void ethervox_aec_reset(ethervox_aec_t* aec) {
    // No-op
}

void ethervox_aec_destroy(ethervox_aec_t* aec) {
    // No-op
}

#endif  // HAVE_SPEEXDSP
