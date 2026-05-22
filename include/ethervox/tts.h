/**
 * @file tts.h
 * @brief Text-to-Speech backend abstraction
 * 
 * Unified interface for TTS engines (Piper with custom phonemizer, system TTS).
 * Generates raw PCM audio for AEC reference and playback.
 */

#ifndef ETHERVOX_TTS_H
#define ETHERVOX_TTS_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TTS backend types
typedef enum {
    ETHERVOX_TTS_BACKEND_NONE = 0,
    ETHERVOX_TTS_BACKEND_PIPER,    // Neural TTS via ONNX Runtime + custom phonemizer
    ETHERVOX_TTS_BACKEND_SYSTEM    // Platform TTS (macOS say, Windows SAPI)
} ethervox_tts_backend_t;

// Callback for streaming audio chunks during synthesis
// Called for each chunk as it's generated, allowing playback to start immediately
typedef void (*ethervox_tts_chunk_callback_t)(
    const float* samples,
    size_t sample_count,
    void* user_data
);

// TTS configuration
typedef struct {
    ethervox_tts_backend_t backend;
    int sample_rate;               // Output sample rate (16000 recommended)
    int channels;                  // 1=mono, 2=stereo
    float speaking_rate;           // 0.5-2.0, 1.0=normal (Piper: inverse of length_scale)
    float phoneme_variance;        // 0.0-1.0, controls phoneme duration randomness (Piper: noise_scale)
    float prosody_variance;        // 0.0-1.0, controls pitch/intonation variance (Piper: noise_w)
    int speaker_id;                // Multi-speaker model voice/emotion selector (0-903 for LibriTTS-R)
    const char* model_path;        // Path to Piper .onnx model (Piper only)
    const char* config_path;       // Path to Piper .json config (Piper only)
    const char* voice_name;        // Voice identifier (backend-specific)
    
    // Streaming configuration (sentence-level, not phoneme-level)
    ethervox_tts_chunk_callback_t chunk_callback;  // Called for each audio chunk (NULL = no streaming)
    void* callback_user_data;      // User data passed to chunk_callback
} ethervox_tts_config_t;

// Audio buffer for TTS output
typedef struct {
    float* samples;      // PCM samples (float32, normalized -1.0 to 1.0)
    size_t sample_count; // Number of samples
    int sample_rate;     // Sample rate (Hz)
    int channels;        // Number of channels
} ethervox_tts_audio_t;

// Opaque TTS context
typedef struct ethervox_tts_context ethervox_tts_context_t;

/**
 * Get default TTS configuration
 */
ethervox_tts_config_t ethervox_tts_default_config(void);

/**
 * Create TTS context
 * 
 * @param config TTS configuration
 * @return TTS context or NULL on error
 */
ethervox_tts_context_t* ethervox_tts_create(const ethervox_tts_config_t* config);

/**
 * Synthesize speech from text
 * 
 * @param ctx TTS context
 * @param text Input text to synthesize
 * @param output Audio buffer (caller must free output->samples)
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_tts_synthesize_text(ethervox_tts_context_t* ctx, 
                                 const char* text,
                                 ethervox_tts_audio_t* output);

/**
 * Synthesize speech from IPA phonemes directly (bypass phonemizer)
 * Used for pronunciation training where IPA is already known
 * 
 * @param ctx TTS context
 * @param ipa_phonemes IPA phoneme string (e.g., "ð i", "ˈɹ ɪ ð ə m")
 * @param output Audio buffer (caller must free output->samples)
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_tts_synthesize_ipa(ethervox_tts_context_t* ctx,
                                const char* ipa_phonemes,
                                ethervox_tts_audio_t* output);

/**
 * Check if TTS context is ready
 */
bool ethervox_tts_is_ready(const ethervox_tts_context_t* ctx);

/**
 * Get current backend type
 */
ethervox_tts_backend_t ethervox_tts_get_backend(const ethervox_tts_context_t* ctx);

/**
 * Get phonemizer from TTS context (for voice training)
 * Returns opaque pointer to phonemizer_t
 */
void* ethervox_tts_get_phonemizer(ethervox_tts_context_t* ctx);

/**
 * Destroy TTS context
 */
void ethervox_tts_destroy(ethervox_tts_context_t* ctx);

/**
 * Free audio buffer
 */
void ethervox_tts_audio_free(ethervox_tts_audio_t* audio);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_TTS_H
