/**
 * @file tts.c
 * @brief TTS backend abstraction implementation
 */

#include "ethervox/tts.h"
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations for backend implementations
extern ethervox_tts_context_t* ethervox_tts_piper_create(const ethervox_tts_config_t* config);
extern ethervox_result_t ethervox_tts_piper_synthesize(ethervox_tts_context_t* ctx, const char* text, ethervox_tts_audio_t* output);
extern ethervox_result_t ethervox_tts_piper_synthesize_ipa(ethervox_tts_context_t* ctx, const char* ipa_phonemes, ethervox_tts_audio_t* output);
extern void ethervox_tts_piper_destroy(ethervox_tts_context_t* ctx);
extern void* ethervox_tts_piper_get_phonemizer(void* piper_impl);

// Context structure
struct ethervox_tts_context {
    ethervox_tts_backend_t backend;
    void* impl;  // Backend-specific implementation
};

ethervox_tts_config_t ethervox_tts_default_config(void) {
    ethervox_tts_config_t config = {
        .backend = ETHERVOX_TTS_BACKEND_PIPER,
        .sample_rate = 16000,
        .channels = 1,
        .speaking_rate = 1.0f,
        .phoneme_variance = 0.667f,  // Default Piper noise_scale
        .prosody_variance = 0.8f,    // Default Piper noise_w
        .speaker_id = 0,             // Default speaker (neutral emotion)
        .model_path = NULL,
        .config_path = NULL,
        .voice_name = "en_US-libritts_r-medium"  // Changed to emotional model
    };
    return config;
}

ethervox_tts_context_t* ethervox_tts_create(const ethervox_tts_config_t* config) {
    if (!config) {
        fprintf(stderr, "[TTS] NULL config provided\n");
        return NULL;
    }
    
    ethervox_tts_context_t* ctx = (ethervox_tts_context_t*)calloc(1, sizeof(ethervox_tts_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->backend = config->backend;
    
    switch (config->backend) {
        case ETHERVOX_TTS_BACKEND_PIPER:
#ifdef HAVE_PIPER_TTS
            ctx->impl = ethervox_tts_piper_create(config);
            if (!ctx->impl) {
                fprintf(stderr, "[TTS] Failed to create Piper backend\n");
                free(ctx);
                return NULL;
            }
#else
            fprintf(stderr, "[TTS] Piper backend not available on this platform\n");
            free(ctx);
            return NULL;
#endif
            break;
            
        case ETHERVOX_TTS_BACKEND_SYSTEM:
            // TODO: Implement system TTS backend
            fprintf(stderr, "[TTS] System TTS backend not yet implemented\n");
            free(ctx);
            return NULL;
            
        default:
            fprintf(stderr, "[TTS] Unknown backend: %d\n", config->backend);
            free(ctx);
            return NULL;
    }
    
    return ctx;
}

ethervox_result_t ethervox_tts_synthesize_text(ethervox_tts_context_t* ctx,
                                 const char* text,
                                 ethervox_tts_audio_t* output) {
    ETHERVOX_CHECK_PTR(ctx);
    ETHERVOX_CHECK_PTR(text);
    ETHERVOX_CHECK_PTR(output);
    
    switch (ctx->backend) {
        case ETHERVOX_TTS_BACKEND_PIPER:
#ifdef HAVE_PIPER_TTS
            return ethervox_tts_piper_synthesize(ctx->impl, text, output);
#else
            return ETHERVOX_ERROR_NOT_SUPPORTED;
#endif
            
        case ETHERVOX_TTS_BACKEND_SYSTEM:
            // TODO
            return ETHERVOX_ERROR_NOT_IMPLEMENTED;
            
        default:
            return ETHERVOX_ERROR_NOT_SUPPORTED;
    }
}

ethervox_result_t ethervox_tts_synthesize_ipa(ethervox_tts_context_t* ctx,
                                const char* ipa_phonemes,
                                ethervox_tts_audio_t* output) {
    ETHERVOX_CHECK_PTR(ctx);
    ETHERVOX_CHECK_PTR(ipa_phonemes);
    ETHERVOX_CHECK_PTR(output);
    
    // Only Piper backend supports direct IPA synthesis
    if (ctx->backend == ETHERVOX_TTS_BACKEND_PIPER) {
#ifdef HAVE_PIPER_TTS
        return ethervox_tts_piper_synthesize_ipa(ctx->impl, ipa_phonemes, output);
#else
        return ETHERVOX_ERROR_NOT_SUPPORTED;
#endif
    }
    
    // System TTS doesn't support IPA - fall back to text synthesis
    return ethervox_tts_synthesize_text(ctx, ipa_phonemes, output);
}

bool ethervox_tts_is_ready(const ethervox_tts_context_t* ctx) {
    return ctx != NULL && ctx->impl != NULL;
}

ethervox_tts_backend_t ethervox_tts_get_backend(const ethervox_tts_context_t* ctx) {
    return ctx ? ctx->backend : ETHERVOX_TTS_BACKEND_NONE;
}

void ethervox_tts_destroy(ethervox_tts_context_t* ctx) {
    if (!ctx) return;
    
    switch (ctx->backend) {
        case ETHERVOX_TTS_BACKEND_PIPER:
#ifdef HAVE_PIPER_TTS
            ethervox_tts_piper_destroy(ctx->impl);
#endif
            break;
            
        case ETHERVOX_TTS_BACKEND_SYSTEM:
            // TODO
            break;
            
        default:
            break;
    }
    
    free(ctx);
}

void* ethervox_tts_get_phonemizer(ethervox_tts_context_t* ctx) {
    if (!ctx) {
        fprintf(stderr, "[TTS] get_phonemizer: ctx is NULL\n");
        return NULL;
    }
    if (!ctx->impl) {
        fprintf(stderr, "[TTS] get_phonemizer: ctx->impl is NULL\n");
        return NULL;
    }
    
    fprintf(stderr, "[TTS] get_phonemizer: backend=%d, calling backend accessor\n", ctx->backend);
    
    switch (ctx->backend) {
        case ETHERVOX_TTS_BACKEND_PIPER:
#ifdef HAVE_PIPER_TTS
            return ethervox_tts_piper_get_phonemizer(ctx->impl);
#else
            return NULL;
#endif
        default:
            fprintf(stderr, "[TTS] get_phonemizer: unsupported backend %d\n", ctx->backend);
            return NULL;
    }
}

void ethervox_tts_audio_free(ethervox_tts_audio_t* audio) {
    if (audio && audio->samples) {
        free(audio->samples);
        audio->samples = NULL;
        audio->sample_count = 0;
    }
}
