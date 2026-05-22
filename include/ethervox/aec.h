/**
 * @file aec.h
 * @brief Acoustic Echo Cancellation (AEC) API
 * 
 * Removes speaker output (TTS) from microphone input to prevent the AI
 * from hearing itself speak, while still allowing user interruptions.
 */

#ifndef ETHERVOX_AEC_H
#define ETHERVOX_AEC_H

#include <stddef.h>
#include <stdbool.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * AEC backend type
 */
typedef enum {
    ETHERVOX_AEC_NONE = 0,      /**< No AEC processing */
    ETHERVOX_AEC_SPEEX,         /**< Speex AEC (lightweight, adequate quality) */
    ETHERVOX_AEC_WEBRTC,        /**< WebRTC APM (higher quality, more complex) */
} ethervox_aec_backend_t;

/**
 * Opaque AEC context
 */
typedef struct ethervox_aec_s ethervox_aec_t;

/**
 * AEC configuration
 */
typedef struct {
    int sample_rate;              /**< Audio sample rate (e.g., 16000 Hz) */
    int frame_size;               /**< Samples per frame (e.g., 160 = 10ms @ 16kHz) */
    int filter_length;            /**< Echo tail length in samples (e.g., 1024 = ~64ms) */
    ethervox_aec_backend_t backend; /**< AEC backend to use */
    float suppression_level;      /**< Echo suppression strength (0.0-1.0, default 0.5) */
} ethervox_aec_config_t;

/**
 * Create AEC context
 * 
 * @param config AEC configuration
 * @return AEC context, or NULL on failure
 */
ethervox_aec_t* ethervox_aec_create(const ethervox_aec_config_t* config);

/**
 * Set reference signal (speaker output / TTS)
 * 
 * Call this BEFORE playing TTS audio to prime the AEC with what will be heard.
 * The reference signal is used to predict and remove echo from microphone input.
 * 
 * @param aec AEC context
 * @param samples Reference audio samples (speaker output)
 * @param count Number of samples
 * 
 * @note Reference signal must be time-aligned with microphone input
 */
void ethervox_aec_set_reference(ethervox_aec_t* aec, const float* samples, size_t count);

/**
 * Process microphone input (remove echo)
 * 
 * Removes echo from microphone input based on previously set reference signal.
 * Modifies samples in-place.
 * 
 * @param aec AEC context
 * @param mic_input Microphone samples (will be modified in-place)
 * @param count Number of samples (must match frame_size from config)
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_aec_process(ethervox_aec_t* aec, float* mic_input, size_t count);

/**
 * Check if AEC is currently active (TTS playing)
 * 
 * @param aec AEC context
 * @return true if reference signal is set and AEC is processing
 */
bool ethervox_aec_is_active(const ethervox_aec_t* aec);

/**
 * Reset AEC state (after TTS finishes)
 * 
 * Clears reference signal and internal buffers. Call this when TTS playback
 * completes to stop echo cancellation.
 * 
 * @param aec AEC context
 */
void ethervox_aec_reset(ethervox_aec_t* aec);

/**
 * Destroy AEC context
 * 
 * @param aec AEC context (may be NULL)
 */
void ethervox_aec_destroy(ethervox_aec_t* aec);

/**
 * Get default AEC configuration
 * 
 * @return Default configuration with sensible defaults
 */
ethervox_aec_config_t ethervox_aec_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_AEC_H
