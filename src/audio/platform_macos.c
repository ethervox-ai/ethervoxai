/**
 * @file platform_macos.c
 * @brief macOS-specific audio platform implementation for EthervoxAI (stub)
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
#include <time.h>

#include "ethervox/audio.h"

#ifdef ETHERVOX_PLATFORM_MACOS

typedef struct {
  bool is_recording;
  bool is_playing;
} macos_audio_state_t;

static int macos_audio_init(ethervox_audio_runtime_t* runtime,
                            const ethervox_audio_config_t* config) {
  (void)config;

  macos_audio_state_t* state = (macos_audio_state_t*)calloc(1, sizeof(macos_audio_state_t));
  if (!state) {
    return -1;
  }

  runtime->platform_data = state;
  printf("macOS audio driver initialized (stub)\n");
  return 0;
}

static int macos_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state) {
    return -1;
  }

  state->is_recording = true;
  printf("macOS audio capture requested (not implemented)\n");
  return -1;
}

static int macos_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state) {
    return -1;
  }

  state->is_recording = false;
  return 0;
}

static int macos_audio_start_playback(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state) {
    return -1;
  }

  state->is_playing = true;
  printf("macOS audio playback requested (not implemented)\n");
  return -1;
}

static int macos_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
  macos_audio_state_t* state = (macos_audio_state_t*)runtime->platform_data;
  if (!state) {
    return -1;
  }

  state->is_playing = false;
  return 0;
}

static int macos_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
  (void)runtime;
  (void)buffer;
  return -1;
}

static void macos_audio_cleanup(ethervox_audio_runtime_t* runtime) {
  if (!runtime || !runtime->platform_data) {
    return;
  }

  free(runtime->platform_data);
  runtime->platform_data = NULL;
  printf("macOS audio driver cleaned up\n");
}

int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
  if (!runtime) {
    return -1;
  }

  runtime->driver.init = macos_audio_init;
  runtime->driver.start_capture = macos_audio_start_capture;
  runtime->driver.stop_capture = macos_audio_stop_capture;
  runtime->driver.start_playback = macos_audio_start_playback;
  runtime->driver.stop_playback = macos_audio_stop_playback;
  runtime->driver.read_audio = macos_audio_read;
  runtime->driver.cleanup = macos_audio_cleanup;

  return 0;
}

#endif  // ETHERVOX_PLATFORM_MACOS
