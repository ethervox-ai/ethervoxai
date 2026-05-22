/**
 * @file audio_stream_player.h
 * @brief Real-time streaming audio player for TTS chunks
 * 
 * Provides low-latency audio playback using platform-specific APIs:
 * - macOS: CoreAudio AudioQueue
 * - Linux: ALSA (future)
 * - Windows: WASAPI (future)
 */

#ifndef ETHERVOX_AUDIO_STREAM_PLAYER_H
#define ETHERVOX_AUDIO_STREAM_PLAYER_H

#include <stddef.h>
#include <stdbool.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_stream_player audio_stream_player_t;

/**
 * Create streaming audio player
 * @param sample_rate Audio sample rate (e.g., 16000)
 * @param channels Number of channels (1 = mono, 2 = stereo)
 * @return Player context, or NULL on failure
 */
audio_stream_player_t* audio_stream_player_create(int sample_rate, int channels);

/**
 * Start playback (must be called before feeding chunks)
 * @param player Player context
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t audio_stream_player_start(audio_stream_player_t* player);

/**
 * Feed audio chunk for playback (non-blocking)
 * Chunks are queued and played in order. This function returns immediately.
 * @param player Player context
 * @param samples Audio samples (float, -1.0 to 1.0)
 * @param sample_count Number of samples
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t audio_stream_player_write(audio_stream_player_t* player, 
                               const float* samples, 
                               size_t sample_count);

/**
 * Wait for all queued audio to finish playing
 * @param player Player context
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t audio_stream_player_wait(audio_stream_player_t* player);

/**
 * Stop playback and clear queue
 * @param player Player context
 */
void audio_stream_player_stop(audio_stream_player_t* player);

/**
 * Destroy player and free resources
 * @param player Player context
 */
void audio_stream_player_destroy(audio_stream_player_t* player);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_AUDIO_STREAM_PLAYER_H
