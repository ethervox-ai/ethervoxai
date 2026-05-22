/**
 * @file audio_stream_player.c
 * @brief Real-time streaming audio player implementation
 * 
 * macOS implementation using CoreAudio AudioQueue for low-latency playback
 */

#include "ethervox/audio_stream_player.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef TARGET_MACOS
#include <AudioToolbox/AudioToolbox.h>

#define NUM_BUFFERS 3
#define BUFFER_SIZE 16384  // Samples per buffer

struct audio_stream_player {
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[NUM_BUFFERS];
    bool buffer_in_use[NUM_BUFFERS];  // Track which buffers are queued
    int sample_rate;
    int channels;
    bool started;
    bool stopping;
    pthread_mutex_t mutex;
    pthread_cond_t buffer_available;
    int buffers_in_use;
    float* overlap_buffer;     // For crossfading between chunks
    size_t overlap_size;       // Number of samples to overlap
    size_t overlap_count;      // Actual samples in overlap buffer
};

// AudioQueue callback - called when a buffer finishes playing
static void audio_queue_callback(void* user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    audio_stream_player_t* player = (audio_stream_player_t*)user_data;
    
    pthread_mutex_lock(&player->mutex);
    
    // Mark this specific buffer as free
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (player->buffers[i] == buffer) {
            player->buffer_in_use[i] = false;
            break;
        }
    }
    
    player->buffers_in_use--;
    pthread_cond_signal(&player->buffer_available);
    pthread_mutex_unlock(&player->mutex);
}

audio_stream_player_t* audio_stream_player_create(int sample_rate, int channels) {
    audio_stream_player_t* player = (audio_stream_player_t*)calloc(1, sizeof(audio_stream_player_t));
    if (!player) return NULL;
    
    player->sample_rate = sample_rate;
    player->channels = channels;
    player->started = false;
    player->stopping = false;
    player->buffers_in_use = 0;
    
    for (int i = 0; i < NUM_BUFFERS; i++) {
        player->buffer_in_use[i] = false;
    }
    
    pthread_mutex_init(&player->mutex, NULL);
    pthread_cond_init(&player->buffer_available, NULL);
    
    // Set up audio format (Linear PCM float)
    AudioStreamBasicDescription format = {0};
    format.mSampleRate = sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsPacked;
    format.mBitsPerChannel = 32;
    format.mChannelsPerFrame = channels;
    format.mBytesPerFrame = sizeof(float) * channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = format.mBytesPerFrame;
    
    // Create output queue
    OSStatus status = AudioQueueNewOutput(&format, audio_queue_callback, player,
                                         NULL, NULL, 0, &player->queue);
    if (status != noErr) {
        ETHERVOX_LOG_ERROR("[AudioStream] Failed to create AudioQueue: %d", (int)status);
        pthread_mutex_destroy(&player->mutex);
        pthread_cond_destroy(&player->buffer_available);
        free(player);
        return NULL;
    }
    
    // Allocate buffers
    for (int i = 0; i < NUM_BUFFERS; i++) {
        status = AudioQueueAllocateBuffer(player->queue, BUFFER_SIZE * sizeof(float) * channels,
                                         &player->buffers[i]);
        if (status != noErr) {
            ETHERVOX_LOG_ERROR("[AudioStream] Failed to allocate buffer %d: %d", i, (int)status);
            for (int j = 0; j < i; j++) {
                AudioQueueFreeBuffer(player->queue, player->buffers[j]);
            }
            AudioQueueDispose(player->queue, true);
            pthread_mutex_destroy(&player->mutex);
            pthread_cond_destroy(&player->buffer_available);
            free(player);
            return NULL;
        }
    }
    
    return player;
}

ethervox_result_t audio_stream_player_start(audio_stream_player_t* player) {
    ETHERVOX_CHECK_PTR(player);
    if (player->started) {
        return ETHERVOX_ERROR_ALREADY_INITIALIZED;
    }
    
    OSStatus status = AudioQueueStart(player->queue, NULL);
    if (status != noErr) {
        ETHERVOX_LOG_ERROR("[AudioStream] Failed to start AudioQueue playback: %d", (int)status);
        return ETHERVOX_ERROR_AUDIO_INIT;
    }
    
    player->started = true;
    return ETHERVOX_SUCCESS;
}

ethervox_result_t audio_stream_player_write(audio_stream_player_t* player, 
                               const float* samples, 
                               size_t sample_count) {
    ETHERVOX_CHECK_PTR(player);
    if (!player->started || player->stopping) {
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    if (!samples || sample_count == 0) {
        return ETHERVOX_SUCCESS;
    }
    
    size_t offset = 0;
    
    while (offset < sample_count) {
        // Wait for available buffer
        pthread_mutex_lock(&player->mutex);
        while (player->buffers_in_use >= NUM_BUFFERS && !player->stopping) {
            pthread_cond_wait(&player->buffer_available, &player->mutex);
        }
        
        if (player->stopping) {
            pthread_mutex_unlock(&player->mutex);
            return ETHERVOX_ERROR_NOT_INITIALIZED;
        }
        
        // Find free buffer
        int buffer_index = -1;
        for (int i = 0; i < NUM_BUFFERS; i++) {
            if (!player->buffer_in_use[i]) {
                buffer_index = i;
                player->buffer_in_use[i] = true;
                break;
            }
        }
        
        pthread_mutex_unlock(&player->mutex);
        
        if (buffer_index < 0) continue;  // Should never happen
        
        AudioQueueBufferRef buffer = player->buffers[buffer_index];
        
        // Fill buffer
        size_t chunk_size = sample_count - offset;
        if (chunk_size > BUFFER_SIZE) chunk_size = BUFFER_SIZE;
        
        memcpy(buffer->mAudioData, samples + offset, chunk_size * sizeof(float) * player->channels);
        buffer->mAudioDataByteSize = chunk_size * sizeof(float) * player->channels;
        
        // Enqueue buffer
        pthread_mutex_lock(&player->mutex);
        player->buffers_in_use++;
        pthread_mutex_unlock(&player->mutex);
        
        OSStatus status = AudioQueueEnqueueBuffer(player->queue, buffer, 0, NULL);
        if (status != noErr) {
            ETHERVOX_LOG_ERROR("[AudioStream] Failed to enqueue audio buffer: %d", (int)status);
            pthread_mutex_lock(&player->mutex);
            player->buffers_in_use--;
            pthread_mutex_unlock(&player->mutex);
            return ETHERVOX_ERROR_AUDIO_INIT;
        }
        
        offset += chunk_size;
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t audio_stream_player_wait(audio_stream_player_t* player) {
    ETHERVOX_CHECK_PTR(player);
    if (!player->started) {
        return ETHERVOX_ERROR_NOT_INITIALIZED;
    }
    
    // Wait until all buffers are played by the AudioQueue
    pthread_mutex_lock(&player->mutex);
    while (player->buffers_in_use > 0) {
        // Use timed wait to prevent indefinite hang if callback missed
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 5;  // 5 second timeout
        
        int result = pthread_cond_timedwait(&player->buffer_available, &player->mutex, &timeout);
        if (result == ETIMEDOUT) {
            ETHERVOX_LOG_ERROR("[AudioStream] Wait timeout - %d buffers still in use", player->buffers_in_use);
            break;
        }
    }
    pthread_mutex_unlock(&player->mutex);
    
    // Stop the queue gracefully (immediate=false lets queued audio finish)
    AudioQueueStop(player->queue, false);
    
    // Calculate how long the last buffer should take to play
    // Buffer size is BUFFER_SIZE samples at sample_rate Hz
    float buffer_duration = (float)BUFFER_SIZE / (float)player->sample_rate;
    int wait_us = (int)(buffer_duration * 1000000.0f) + 100000;  // Add 100ms margin
    usleep(wait_us);
    
    return ETHERVOX_SUCCESS;
}

void audio_stream_player_stop(audio_stream_player_t* player) {
    if (!player) return;
    
    pthread_mutex_lock(&player->mutex);
    player->stopping = true;
    pthread_cond_broadcast(&player->buffer_available);
    pthread_mutex_unlock(&player->mutex);
    
    if (player->started) {
        AudioQueueStop(player->queue, true);
        player->started = false;
    }
}

void audio_stream_player_destroy(audio_stream_player_t* player) {
    if (!player) return;
    
    audio_stream_player_stop(player);
    
    // Free buffers
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (player->buffers[i]) {
            AudioQueueFreeBuffer(player->queue, player->buffers[i]);
        }
    }
    
    // Dispose queue
    if (player->queue) {
        AudioQueueDispose(player->queue, true);
    }
    
    pthread_mutex_destroy(&player->mutex);
    pthread_cond_destroy(&player->buffer_available);
    free(player);
}

#else
// Stub implementation for non-macOS platforms
struct audio_stream_player {
    int dummy;
};

audio_stream_player_t* audio_stream_player_create(int sample_rate, int channels) {
    ETHERVOX_LOG_ERROR("[AudioStream] Streaming playback not implemented for this platform");
    return NULL;
}

ethervox_result_t audio_stream_player_start(audio_stream_player_t* player) { 
    return ETHERVOX_ERROR_NOT_SUPPORTED; 
}
ethervox_result_t audio_stream_player_write(audio_stream_player_t* player, const float* samples, size_t sample_count) { 
    return ETHERVOX_ERROR_NOT_SUPPORTED; 
}
ethervox_result_t audio_stream_player_wait(audio_stream_player_t* player) { 
    return ETHERVOX_ERROR_NOT_SUPPORTED; 
}
void audio_stream_player_stop(audio_stream_player_t* player) {}
void audio_stream_player_destroy(audio_stream_player_t* player) { free(player); }

#endif
