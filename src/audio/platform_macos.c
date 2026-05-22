/**
 * @file platform_macos.c
 * @brief macOS-specific audio platform implementation for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 *
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <AudioToolbox/AudioToolbox.h>

#include "ethervox/audio.h"
#include "ethervox/error.h"

#ifdef ETHERVOX_PLATFORM_MACOS

#define BUFFER_SIZE 4096
#define NUM_BUFFERS 3

typedef struct {
  AudioQueueRef capture_queue;
  AudioQueueBufferRef capture_buffers[NUM_BUFFERS];
  bool is_recording;
  
  AudioQueueRef playback_queue;
  AudioQueueBufferRef playback_buffers[NUM_BUFFERS];
  bool is_playing;
  
  // Ring buffer for captured audio
  int16_t* ring_buffer;
  size_t ring_buffer_size;
  size_t write_pos;
  size_t read_pos;
  pthread_mutex_t lock;
  
  // Ring buffer for playback audio (TTS output)
  int16_t* playback_ring_buffer;
  size_t playback_ring_buffer_size;
  size_t playback_write_pos;
  size_t playback_read_pos;
  pthread_mutex_t playback_lock;
  
  uint32_t sample_rate;
  uint8_t channels;
} macos_audio_state_t;

// Audio queue callback for input
static void input_callback(void* user_data, AudioQueueRef queue,
                          AudioQueueBufferRef buffer,
                          const AudioTimeStamp* start_time,
                          UInt32 num_packets,
                          const AudioStreamPacketDescription* packet_desc) {
  (void)queue;
  (void)start_time;
  (void)packet_desc;
  (void)num_packets;
  
  macos_audio_state_t* state = (macos_audio_state_t*)user_data;
  if (!state || !state->is_recording) {
    return;
  }
  
  // Copy audio data to ring buffer
  int16_t* samples = (int16_t*)buffer->mAudioData;
  size_t sample_count = buffer->mAudioDataByteSize / sizeof(int16_t);
  
  pthread_mutex_lock(&state->lock);
  
  for (size_t i = 0; i < sample_count; i++) {
    state->ring_buffer[state->write_pos] = samples[i];
    state->write_pos = (state->write_pos + 1) % state->ring_buffer_size;
    
    // Overwrite old data if buffer full
    if (state->write_pos == state->read_pos) {
      state->read_pos = (state->read_pos + 1) % state->ring_buffer_size;
    }
  }
  
  pthread_mutex_unlock(&state->lock);
  
  // Re-enqueue buffer for more recording
  AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static ethervox_result_t macos_audio_init(ethervox_audio_runtime_t* runtime,
                            const ethervox_audio_config_t* config) {
  macos_audio_state_t* state = (macos_audio_state_t*)calloc(1, sizeof(macos_audio_state_t));
  if (!state) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate macOS audio state");
  }
  
  state->sample_rate = config->sample_rate ? config->sample_rate : 16000;
  state->channels = config->channels ? config->channels : 1;
  
  // Allocate capture ring buffer (10 seconds of audio)
  state->ring_buffer_size = state->sample_rate * 10;
  state->ring_buffer = (int16_t*)calloc(state->ring_buffer_size, sizeof(int16_t));
  if (!state->ring_buffer) {
    free(state);
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate capture ring buffer");
  }
  
  // Allocate playback ring buffer (60 seconds of audio for long TTS responses)
  // This is ~1.9MB at 16kHz mono which is acceptable for desktop
  state->playback_ring_buffer_size = state->sample_rate * 60;
  state->playback_ring_buffer = (int16_t*)calloc(state->playback_ring_buffer_size, sizeof(int16_t));
  if (!state->playback_ring_buffer) {
    free(state->ring_buffer);
    free(state);
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate playback ring buffer");
  }
  
  pthread_mutex_init(&state->lock, NULL);
  pthread_mutex_init(&state->playback_lock, NULL);

  runtime->platform_data = state;
  // Debug message removed - too verbose for normal startup
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t macos_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio state not initialized");
  }
  
  if (state->is_recording) {
    return ETHERVOX_SUCCESS; // Already recording
  }
  
  // Configure audio format
  AudioStreamBasicDescription format = {0};
  format.mSampleRate = state->sample_rate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  format.mBitsPerChannel = 16;
  format.mChannelsPerFrame = state->channels;
  format.mBytesPerFrame = state->channels * sizeof(int16_t);
  format.mFramesPerPacket = 1;
  format.mBytesPerPacket = format.mBytesPerFrame;
  
  // Create audio queue
  OSStatus status = AudioQueueNewInput(&format, input_callback, state,
                                       NULL, kCFRunLoopCommonModes, 0,
                                       &state->capture_queue);
  if (status != noErr) {
    if (status == kAudioQueueErr_InvalidDevice) {
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_DEVICE_NOT_FOUND, "Invalid audio device");
    } else if (status == kAudioQueueErr_Permissions || status == -50) {
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Permission denied or device busy - check microphone permissions");
    } else {
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to create audio input queue");
    }
  }
  
  // Allocate and enqueue buffers
  for (int i = 0; i < NUM_BUFFERS; i++) {
    status = AudioQueueAllocateBuffer(state->capture_queue, BUFFER_SIZE, &state->capture_buffers[i]);
    if (status != noErr) {
      AudioQueueDispose(state->capture_queue, true);
      state->capture_queue = NULL;
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to allocate audio buffer");
    }
    AudioQueueEnqueueBuffer(state->capture_queue, state->capture_buffers[i], 0, NULL);
  }
  
  // Start recording
  status = AudioQueueStart(state->capture_queue, NULL);
  if (status != noErr) {
    AudioQueueDispose(state->capture_queue, true);
    state->capture_queue = NULL;
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to start audio queue");
  }
  
  state->is_recording = true;
  printf("🎤 Started microphone capture\n");
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t macos_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state || !state->is_recording) {
    return ETHERVOX_SUCCESS;
  }

  // Stop and dispose queue
  if (state->capture_queue) {
    AudioQueueStop(state->capture_queue, true);
    AudioQueueDispose(state->capture_queue, true);
    state->capture_queue = NULL;
  }
  
  state->is_recording = false;
  printf("⏹️  Stopped microphone capture\n");
  return ETHERVOX_SUCCESS;
}

// Audio queue callback for output (playback)
static void output_callback(void* user_data, AudioQueueRef queue,
                           AudioQueueBufferRef buffer) {
  macos_audio_state_t* state = (macos_audio_state_t*)user_data;
  if (!state) {
    memset(buffer->mAudioData, 0, buffer->mAudioDataBytesCapacity);
    buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
    return;
  }
  
  int16_t* output = (int16_t*)buffer->mAudioData;
  size_t max_samples = buffer->mAudioDataBytesCapacity / sizeof(int16_t);
  size_t samples_written = 0;
  
  pthread_mutex_lock(&state->playback_lock);
  
  // Read available samples from playback ring buffer
  while (samples_written < max_samples && state->playback_read_pos != state->playback_write_pos) {
    output[samples_written++] = state->playback_ring_buffer[state->playback_read_pos];
    state->playback_read_pos = (state->playback_read_pos + 1) % state->playback_ring_buffer_size;
  }
  
  pthread_mutex_unlock(&state->playback_lock);
  
  // Fill remaining with silence
  for (size_t i = samples_written; i < max_samples; i++) {
    output[i] = 0;
  }
  
  buffer->mAudioDataByteSize = max_samples * sizeof(int16_t);
  AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static ethervox_result_t macos_audio_start_playback(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio state not initialized");
  }
  
  if (state->is_playing) {
    return ETHERVOX_SUCCESS; // Already playing
  }
  
  // Configure audio format (same as capture)
  AudioStreamBasicDescription format = {0};
  format.mSampleRate = state->sample_rate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  format.mBitsPerChannel = 16;
  format.mChannelsPerFrame = state->channels;
  format.mBytesPerFrame = state->channels * sizeof(int16_t);
  format.mFramesPerPacket = 1;
  format.mBytesPerPacket = format.mBytesPerFrame;
  
  // Create audio queue for output
  OSStatus status = AudioQueueNewOutput(&format, output_callback, state,
                                       NULL, kCFRunLoopCommonModes, 0,
                                       &state->playback_queue);
  if (status != noErr) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to create audio output queue");
  }
  
  // Allocate buffers for playback
  for (int i = 0; i < NUM_BUFFERS; i++) {
    status = AudioQueueAllocateBuffer(state->playback_queue, BUFFER_SIZE, 
                                     &state->playback_buffers[i]);
    if (status != noErr) {
      AudioQueueDispose(state->playback_queue, true);
      state->playback_queue = NULL;
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to allocate playback buffer");
    }
    
    // Prime buffers with silence
    memset(state->playback_buffers[i]->mAudioData, 0, BUFFER_SIZE);
    state->playback_buffers[i]->mAudioDataByteSize = BUFFER_SIZE;
    AudioQueueEnqueueBuffer(state->playback_queue, state->playback_buffers[i], 0, NULL);
  }
  
  // Start playback
  status = AudioQueueStart(state->playback_queue, NULL);
  if (status != noErr) {
    AudioQueueDispose(state->playback_queue, true);
    state->playback_queue = NULL;
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to start playback queue");
  }
  
  state->is_playing = true;
  printf("🔊 Started audio playback\n");
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t macos_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state || !state->is_playing) {
    return ETHERVOX_SUCCESS;
  }
  
  if (state->playback_queue) {
    AudioQueueStop(state->playback_queue, true);
    AudioQueueDispose(state->playback_queue, true);
    state->playback_queue = NULL;
  }
  
  state->is_playing = false;
  printf("⏹️  Stopped audio playback\n");
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t macos_audio_write(ethervox_audio_runtime_t* runtime, 
                             const ethervox_audio_buffer_t* buffer) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  ETHERVOX_CHECK_PTR(state);
  ETHERVOX_CHECK_PTR(buffer);
  ETHERVOX_CHECK_PTR(buffer->data);
  
  if (!state->is_playing) {
    // Auto-start playback if not already started
    ethervox_result_t result = macos_audio_start_playback(runtime);
    if (ethervox_is_error(result)) {
      return result;
    }
  }
  
  // Buffer data is int16_t samples (already converted from float32 in caller)
  int16_t* samples = (int16_t*)buffer->data;
  size_t sample_count = buffer->size / sizeof(int16_t);
  size_t samples_written = 0;
  size_t samples_dropped = 0;
  
  pthread_mutex_lock(&state->playback_lock);
  
  // Check available space in ring buffer
  size_t write_pos = state->playback_write_pos;
  size_t read_pos = state->playback_read_pos;
  size_t available;
  if (write_pos >= read_pos) {
    available = state->playback_ring_buffer_size - (write_pos - read_pos) - 1;
  } else {
    available = read_pos - write_pos - 1;
  }
  
  if (sample_count > available) {
    // Not enough space - will need to drop old samples
    samples_dropped = sample_count - available;
    fprintf(stderr, "[Audio] Warning: Playback buffer near full, dropping %zu old samples\n", samples_dropped);
  }
  
  // Write samples to playback ring buffer
  for (size_t i = 0; i < sample_count; i++) {
    size_t next_write = (state->playback_write_pos + 1) % state->playback_ring_buffer_size;
    
    // Check if buffer is full
    if (next_write == state->playback_read_pos) {
      // Buffer full - drop oldest sample to make room
      state->playback_read_pos = (state->playback_read_pos + 1) % state->playback_ring_buffer_size;
    }
    
    state->playback_ring_buffer[state->playback_write_pos] = samples[i];
    state->playback_write_pos = next_write;
    samples_written++;
  }
  
  pthread_mutex_unlock(&state->playback_lock);
  
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t macos_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  ETHERVOX_CHECK_PTR(state);
  ETHERVOX_CHECK_PTR(buffer);
  
  pthread_mutex_lock(&state->lock);
  
  // Calculate available samples
  size_t available;
  if (state->write_pos >= state->read_pos) {
    available = state->write_pos - state->read_pos;
  } else {
    available = state->ring_buffer_size - state->read_pos + state->write_pos;
  }
  
  if (available == 0) {
    pthread_mutex_unlock(&state->lock);
    buffer->size = 0;
    return ETHERVOX_SUCCESS; // No data available (not an error)
  }
  
  // Read up to buffer capacity
  size_t to_read = (available < buffer->size) ? available : buffer->size;
  
  for (size_t i = 0; i < to_read; i++) {
    // Convert int16 to float32 normalized to [-1.0, 1.0]
    buffer->data[i] = (float)state->ring_buffer[state->read_pos] / 32768.0f;
    state->read_pos = (state->read_pos + 1) % state->ring_buffer_size;
  }
  
  buffer->size = to_read;
  
  pthread_mutex_unlock(&state->lock);
  
  return ETHERVOX_SUCCESS;
}

static void macos_audio_cleanup(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->platform_data) {
    return;
  }
  
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  
  // Stop capture if active
  if (state->is_recording && state->capture_queue) {
    AudioQueueStop(state->capture_queue, true);
    AudioQueueDispose(state->capture_queue, true);
  }
  
  // Stop playback if active
  if (state->is_playing && state->playback_queue) {
    AudioQueueStop(state->playback_queue, true);
    AudioQueueDispose(state->playback_queue, true);
  }
  
  pthread_mutex_destroy(&state->lock);
  free(state->ring_buffer);
  free(state);
  runtime->platform_data = NULL;
  printf("macOS CoreAudio driver cleaned up\n");
}

ethervox_result_t ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);

  runtime->driver.init = macos_audio_init;
  runtime->driver.start_capture = macos_audio_start_capture;
  runtime->driver.stop_capture = macos_audio_stop_capture;
  runtime->driver.start_playback = macos_audio_start_playback;
  runtime->driver.stop_playback = macos_audio_stop_playback;
  runtime->driver.read_audio = macos_audio_read;
  runtime->driver.write_audio = macos_audio_write;
  runtime->driver.cleanup = macos_audio_cleanup;

  return ETHERVOX_SUCCESS;
}

#endif  // ETHERVOX_PLATFORM_MACOS

// ============================================================================
// Shared Audio Utility Functions (Platform-Independent)
// ============================================================================

#include <math.h>

/**
 * Calculate RMS (root mean square) energy of audio samples
 * 
 * Used for voice activity detection (VAD) and speech energy measurement.
 * This is a shared utility to avoid code duplication across modules.
 * 
 * @param samples Float audio samples normalized to [-1, 1]
 * @param count Number of samples
 * @return RMS energy value (typically 0.0-1.0 range)
 */
float ethervox_audio_calculate_rms_energy(const float* samples, uint32_t count) {
    if (!samples || count == 0) {
        return 0.0f;
    }
    
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        double sample = (double)samples[i];
        sum_sq += sample * sample;
    }
    
    return (float)sqrt(sum_sq / (double)count);
}
