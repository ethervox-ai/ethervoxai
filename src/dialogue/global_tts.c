/**
 * @file global_tts.c
 * @brief Global TTS instance for shared use between CLI and conversation
 *
 * This provides a single TTS instance that can be initialized once at startup
 * and reused by multiple components (speak tool, /convon, etc).
 */

#include <pthread.h>
#include <stdlib.h>
#include "ethervox/tts.h"
#include "ethervox/settings.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"

// Global TTS instance (for standalone speak tool, shared with conversation)
ethervox_tts_context_t* g_global_tts = NULL;
pthread_mutex_t g_tts_mutex = PTHREAD_MUTEX_INITIALIZER;

ethervox_tts_context_t* ethervox_get_global_tts(void) {
    return g_global_tts;
}

ethervox_result_t ethervox_reload_global_tts(const void* tts_settings,
                               void (*chunk_callback)(const float*, size_t, void*),
                               void* callback_user_data) {
    if (!tts_settings) {
        ETHERVOX_LOG_ERROR("[TTS Reload] NULL settings provided");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const ethervox_tts_settings_t* settings = (const ethervox_tts_settings_t*)tts_settings;
    
    pthread_mutex_lock(&g_tts_mutex);
    
    ETHERVOX_LOG_INFO("[TTS Reload] Reloading TTS with voice: %s (engine: %s)",
                      settings->voice_en, settings->engine);
    
    // Destroy existing TTS instance
    if (g_global_tts) {
        ETHERVOX_LOG_DEBUG("[TTS Reload] Destroying old TTS instance...");
        ethervox_tts_destroy(g_global_tts);
        g_global_tts = NULL;
    }
    
    // Create new TTS instance with updated settings
    ethervox_tts_config_t tts_config = {
        .backend = ETHERVOX_TTS_BACKEND_PIPER,
        .model_path = settings->piper_model_path,
        .sample_rate = 16000,
        .channels = 1,
        .speaking_rate = settings->speed,
        .phoneme_variance = settings->phoneme_variance,
        .prosody_variance = settings->prosody_variance,
        .chunk_callback = chunk_callback,
        .callback_user_data = callback_user_data
    };
    
    ETHERVOX_LOG_DEBUG("[TTS Reload] Creating new TTS instance with model: %s",
                       tts_config.model_path);
    
    g_global_tts = ethervox_tts_create(&tts_config);
    
    pthread_mutex_unlock(&g_tts_mutex);
    
    if (!g_global_tts) {
        ETHERVOX_LOG_ERROR("[TTS Reload] Failed to create new TTS instance");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ETHERVOX_LOG_INFO("[TTS Reload] ✅ TTS reloaded successfully");
    return ETHERVOX_SUCCESS;
}
