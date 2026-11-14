/**
 * @file platform_ios.c
 * @brief iOS-specific audio platform implementation for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI. Proprietary and confidential. See LICENSE.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - Proprietary: internal use only, see LICENSE
 * - ShareAlike: Distribute under same license
 *
 * For full license terms, see: LICENSE
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 *
 * ARCHITECTURE CHANGE (Granite Speech / multiplatform voice integration):
 * New file - no iOS audio HAL existed before this change (only
 * macos/android/linux/windows/rpi/esp32 under src/audio/). AudioToolbox's
 * AudioQueue API is available on iOS just like macOS, so the capture/
 * playback ring-buffer plumbing below is adapted directly from
 * platform_macos.c. The one thing macOS never needed that iOS requires is
 * AVAudioSession configuration (category .playAndRecord, mode .voiceChat or
 * .measurement) before the AudioQueue will actually receive mic input -
 * without it, iOS silently denies capture even with the Info.plist
 * NSMicrophoneUsageDescription key present and user permission granted.
 * That configuration is Objective-C-only, so it lives in the small shim
 * ios_avaudiosession.m and is called here via a plain C function
 * (ios_configure_audio_session_for_voice), mirroring the pattern already
 * established by weather_http_ios.m for NSURLSession.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <AudioToolbox/AudioToolbox.h>

#include "ethervox/audio.h"
#include "ethervox/error.h"

#if defined(ETHERVOX_PLATFORM_IOS)

// Implemented in ios_avaudiosession.m (Objective-C shim - see file header).
extern int ios_configure_audio_session_for_voice(int enable_echo_cancellation);
extern void ios_deactivate_audio_session(void);

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
  bool echo_cancellation_requested;
} ios_audio_state_t;

// Audio queue callback for input
static void ios_input_callback(void* user_data, AudioQueueRef queue,
                                AudioQueueBufferRef buffer,
                                const AudioTimeStamp* start_time,
                                UInt32 num_packets,
                                const AudioStreamPacketDescription* packet_desc) {
  (void)queue;
  (void)start_time;
  (void)packet_desc;
  (void)num_packets;

  ios_audio_state_t* state = (ios_audio_state_t*)user_data;
  if (!state || !state->is_recording) {
    return;
  }

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

static ethervox_result_t ios_audio_init(ethervox_audio_runtime_t* runtime,
                                         const ethervox_audio_config_t* config) {
  ios_audio_state_t* state = (ios_audio_state_t*)calloc(1, sizeof(ios_audio_state_t));
  if (!state) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate iOS audio state");
  }

  state->sample_rate = config->sample_rate ? config->sample_rate : 16000;
  state->channels = config->channels ? config->channels : 1;
  state->echo_cancellation_requested = config->enable_echo_cancellation;

  // Allocate capture ring buffer (10 seconds of audio)
  state->ring_buffer_size = state->sample_rate * 10;
  state->ring_buffer = (int16_t*)calloc(state->ring_buffer_size, sizeof(int16_t));
  if (!state->ring_buffer) {
    free(state);
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate capture ring buffer");
  }

  // Allocate playback ring buffer (60 seconds of audio for long TTS responses)
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
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t ios_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  ios_audio_state_t* state = (ios_audio_state_t*)runtime->platform_data;
  if (!state) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio state not initialized");
  }

  if (state->is_recording) {
    return ETHERVOX_SUCCESS; // Already recording
  }

  // Must configure (and activate) the shared AVAudioSession before an
  // AudioQueue input stream can receive any samples - this is the step
  // platform_macos.c never needed. See ios_avaudiosession.m for the
  // .voiceChat (barge-in/AEC) vs .measurement (plain dictation) selection,
  // which mirrors platform_android.c's AAudio input-preset choice below.
  if (ios_configure_audio_session_for_voice(state->echo_cancellation_requested ? 1 : 0) != ETHERVOX_SUCCESS) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to configure AVAudioSession for capture");
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

  OSStatus status = AudioQueueNewInput(&format, ios_input_callback, state,
                                        NULL, kCFRunLoopCommonModes, 0,
                                        &state->capture_queue);
  if (status != noErr) {
    if (status == kAudioQueueErr_InvalidDevice) {
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_DEVICE_NOT_FOUND, "Invalid audio device");
    } else if (status == kAudioQueueErr_Permissions || status == -50) {
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Permission denied - check NSMicrophoneUsageDescription and user grant");
    } else {
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to create audio input queue");
    }
  }

  for (int i = 0; i < NUM_BUFFERS; i++) {
    status = AudioQueueAllocateBuffer(state->capture_queue, BUFFER_SIZE, &state->capture_buffers[i]);
    if (status != noErr) {
      AudioQueueDispose(state->capture_queue, true);
      state->capture_queue = NULL;
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to allocate audio buffer");
    }
    AudioQueueEnqueueBuffer(state->capture_queue, state->capture_buffers[i], 0, NULL);
  }

  status = AudioQueueStart(state->capture_queue, NULL);
  if (status != noErr) {
    AudioQueueDispose(state->capture_queue, true);
    state->capture_queue = NULL;
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to start audio queue");
  }

  state->is_recording = true;
  printf("🎤 Started microphone capture (iOS)\n");
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t ios_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  ios_audio_state_t* state = (ios_audio_state_t*)runtime->platform_data;
  if (!state || !state->is_recording) {
    return ETHERVOX_SUCCESS;
  }

  if (state->capture_queue) {
    AudioQueueStop(state->capture_queue, true);
    AudioQueueDispose(state->capture_queue, true);
    state->capture_queue = NULL;
  }

  state->is_recording = false;
  // Only deactivate the session once nothing else (playback) needs it -
  // Mode 1's full-duplex session keeps output running through SPEAKING, so
  // this is safe to call even mid-conversation: it's a no-op if playback
  // (or another capture start) reactivates the session right after.
  if (!state->is_playing) {
    ios_deactivate_audio_session();
  }
  printf("⏹️  Stopped microphone capture (iOS)\n");
  return ETHERVOX_SUCCESS;
}

// Audio queue callback for output (playback)
static void ios_output_callback(void* user_data, AudioQueueRef queue,
                                 AudioQueueBufferRef buffer) {
  ios_audio_state_t* state = (ios_audio_state_t*)user_data;
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

  while (samples_written < max_samples && state->playback_read_pos != state->playback_write_pos) {
    output[samples_written++] = state->playback_ring_buffer[state->playback_read_pos];
    state->playback_read_pos = (state->playback_read_pos + 1) % state->playback_ring_buffer_size;
  }

  pthread_mutex_unlock(&state->playback_lock);

  for (size_t i = samples_written; i < max_samples; i++) {
    output[i] = 0;
  }

  buffer->mAudioDataByteSize = max_samples * sizeof(int16_t);
  AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static ethervox_result_t ios_audio_start_playback(ethervox_audio_runtime_t* runtime) {
  ios_audio_state_t* state = (ios_audio_state_t*)runtime->platform_data;
  if (!state) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_INITIALIZED, "Audio state not initialized");
  }

  if (state->is_playing) {
    return ETHERVOX_SUCCESS; // Already playing
  }

  // Ensure the session is active/configured even if playback starts without
  // an active capture stream (e.g. TTS-only, no barge-in listening yet).
  if (ios_configure_audio_session_for_voice(state->echo_cancellation_requested ? 1 : 0) != ETHERVOX_SUCCESS) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to configure AVAudioSession for playback");
  }

  AudioStreamBasicDescription format = {0};
  format.mSampleRate = state->sample_rate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  format.mBitsPerChannel = 16;
  format.mChannelsPerFrame = state->channels;
  format.mBytesPerFrame = state->channels * sizeof(int16_t);
  format.mFramesPerPacket = 1;
  format.mBytesPerPacket = format.mBytesPerFrame;

  OSStatus status = AudioQueueNewOutput(&format, ios_output_callback, state,
                                         NULL, kCFRunLoopCommonModes, 0,
                                         &state->playback_queue);
  if (status != noErr) {
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to create audio output queue");
  }

  for (int i = 0; i < NUM_BUFFERS; i++) {
    status = AudioQueueAllocateBuffer(state->playback_queue, BUFFER_SIZE,
                                       &state->playback_buffers[i]);
    if (status != noErr) {
      AudioQueueDispose(state->playback_queue, true);
      state->playback_queue = NULL;
      ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to allocate playback buffer");
    }

    memset(state->playback_buffers[i]->mAudioData, 0, BUFFER_SIZE);
    state->playback_buffers[i]->mAudioDataByteSize = BUFFER_SIZE;
    AudioQueueEnqueueBuffer(state->playback_queue, state->playback_buffers[i], 0, NULL);
  }

  status = AudioQueueStart(state->playback_queue, NULL);
  if (status != noErr) {
    AudioQueueDispose(state->playback_queue, true);
    state->playback_queue = NULL;
    ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_AUDIO_INIT, "Failed to start playback queue");
  }

  state->is_playing = true;
  printf("🔊 Started audio playback (iOS)\n");
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t ios_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
  ios_audio_state_t* state = (ios_audio_state_t*)runtime->platform_data;
  if (!state || !state->is_playing) {
    return ETHERVOX_SUCCESS;
  }

  if (state->playback_queue) {
    AudioQueueStop(state->playback_queue, true);
    AudioQueueDispose(state->playback_queue, true);
    state->playback_queue = NULL;
  }

  state->is_playing = false;
  if (!state->is_recording) {
    ios_deactivate_audio_session();
  }
  printf("⏹️  Stopped audio playback (iOS)\n");
  return ETHERVOX_SUCCESS;
}

static ethervox_result_t ios_audio_write(ethervox_audio_runtime_t* runtime,
                                          const ethervox_audio_buffer_t* buffer) {
  ios_audio_state_t* state = (ios_audio_state_t*)runtime->platform_data;
  ETHERVOX_CHECK_PTR(state);
  ETHERVOX_CHECK_PTR(buffer);
  ETHERVOX_CHECK_PTR(buffer->data);

  if (!state->is_playing) {
    ethervox_result_t result = ios_audio_start_playback(runtime);
    if (ethervox_is_error(result)) {
      return result;
    }
  }

  int16_t* samples = (int16_t*)buffer->data;
  size_t sample_count = buffer->size / sizeof(int16_t);
  size_t samples_dropped = 0;

  pthread_mutex_lock(&state->playback_lock);

  size_t write_pos = state->playback_write_pos;
  size_t read_pos = state->playback_read_pos;
  size_t available;
  if (write_pos >= read_pos) {
    available = state->playback_ring_buffer_size - (write_pos - read_pos) - 1;
  } else {
    available = read_pos - write_pos - 1;
  }

  if (sample_count > available) {
    samples_dropped = sample_count - available;
    fprintf(stderr, "[Audio] Warning: Playback buffer near full, dropping %zu old samples\n", samples_dropped);
  }

  for (size_t i = 0; i < sample_count; i++) {
    size_t next_write = (state->playback_write_pos + 1) % state->playback_ring_buffer_size;

    if (next_write == state->playback_read_pos) {
      state->playback_read_pos = (state->playback_read_pos + 1) % state->playback_ring_buffer_size;
    }

    state->playback_ring_buffer[state->playback_write_pos] = samples[i];
    state->playback_write_pos = next_write;
  }

  pthread_mutex_unlock(&state->playback_lock);

  return ETHERVOX_SUCCESS;
}

static ethervox_result_t ios_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
  ios_audio_state_t* state = (ios_audio_state_t*)runtime->platform_data;
  ETHERVOX_CHECK_PTR(state);
  ETHERVOX_CHECK_PTR(buffer);

  pthread_mutex_lock(&state->lock);

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

  size_t to_read = (available < buffer->size) ? available : buffer->size;

  for (size_t i = 0; i < to_read; i++) {
    buffer->data[i] = (float)state->ring_buffer[state->read_pos] / 32768.0f;
    state->read_pos = (state->read_pos + 1) % state->ring_buffer_size;
  }

  buffer->size = to_read;

  pthread_mutex_unlock(&state->lock);

  return ETHERVOX_SUCCESS;
}

static void ios_audio_cleanup(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->platform_data) {
    return;
  }

  ios_audio_state_t* state = (ios_audio_state_t*)runtime->platform_data;

  if (state->is_recording && state->capture_queue) {
    AudioQueueStop(state->capture_queue, true);
    AudioQueueDispose(state->capture_queue, true);
  }

  if (state->is_playing && state->playback_queue) {
    AudioQueueStop(state->playback_queue, true);
    AudioQueueDispose(state->playback_queue, true);
  }

  if (state->is_recording || state->is_playing) {
    ios_deactivate_audio_session();
  }

  pthread_mutex_destroy(&state->lock);
  pthread_mutex_destroy(&state->playback_lock);
  free(state->ring_buffer);
  free(state->playback_ring_buffer);
  free(state);
  runtime->platform_data = NULL;
  printf("iOS AudioQueue driver cleaned up\n");
}

ethervox_result_t ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
  ETHERVOX_CHECK_PTR(runtime);

  runtime->driver.init = ios_audio_init;
  runtime->driver.start_capture = ios_audio_start_capture;
  runtime->driver.stop_capture = ios_audio_stop_capture;
  runtime->driver.start_playback = ios_audio_start_playback;
  runtime->driver.stop_playback = ios_audio_stop_playback;
  runtime->driver.read_audio = ios_audio_read;
  runtime->driver.write_audio = ios_audio_write;
  runtime->driver.cleanup = ios_audio_cleanup;

  return ETHERVOX_SUCCESS;
}

#endif  // ETHERVOX_PLATFORM_IOS
