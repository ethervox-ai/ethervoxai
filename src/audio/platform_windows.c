/**
 * @file platform_windows.c
 * @brief Windows-specific audio platform implementation for EthervoxAI
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

#include "ethervox/audio.h"

#ifdef ETHERVOX_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma comment(lib, "winmm.lib")

typedef struct {
  HWAVEIN wave_in;
  HWAVEOUT wave_out;
  WAVEHDR wave_header_in;
  WAVEHDR wave_header_out;
  WAVEFORMATEX wave_format;
  bool is_recording;
  bool is_playing;
} windows_audio_data_t;

static int windows_audio_init(ethervox_audio_runtime_t* runtime,
                              const ethervox_audio_config_t* config) {
  windows_audio_data_t* audio_data = (windows_audio_data_t*)malloc(sizeof(windows_audio_data_t));
  if (!audio_data) {
    return -1;
  }

  memset(audio_data, 0, sizeof(windows_audio_data_t));
  runtime->platform_data = audio_data;

  // Setup wave format
  audio_data->wave_format.wFormatTag = WAVE_FORMAT_PCM;
  audio_data->wave_format.nChannels = config->channels;
  audio_data->wave_format.nSamplesPerSec = config->sample_rate;
  audio_data->wave_format.wBitsPerSample = config->bits_per_sample;
  audio_data->wave_format.nBlockAlign = (config->channels * config->bits_per_sample) / 8;
  audio_data->wave_format.nAvgBytesPerSec =
      config->sample_rate * audio_data->wave_format.nBlockAlign;
  audio_data->wave_format.cbSize = 0;

  printf("Windows audio driver initialized\n");
  return 0;
}

static int windows_audio_start_capture(ethervox_audio_runtime_t* runtime) {
  windows_audio_data_t* audio_data = (windows_audio_data_t*)runtime->platform_data;

  MMRESULT result =
      waveInOpen(&audio_data->wave_in, WAVE_MAPPER, &audio_data->wave_format, 0, 0, CALLBACK_NULL);
  if (result != MMSYSERR_NOERROR) {
    printf("Failed to open wave input device: %d\n", result);
    return -1;
  }

  audio_data->is_recording = true;
  printf("Windows audio capture started\n");
  return 0;
}

static int windows_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
  windows_audio_data_t* audio_data = (windows_audio_data_t*)runtime->platform_data;

  if (audio_data->wave_in) {
    waveInClose(audio_data->wave_in);
    audio_data->wave_in = NULL;
  }

  audio_data->is_recording = false;
  printf("Windows audio capture stopped\n");
  return 0;
}

static int windows_audio_start_playback(ethervox_audio_runtime_t* runtime) {
  windows_audio_data_t* audio_data = (windows_audio_data_t*)runtime->platform_data;

  MMRESULT result = waveOutOpen(&audio_data->wave_out, WAVE_MAPPER, &audio_data->wave_format, 0, 0,
                                CALLBACK_NULL);
  if (result != MMSYSERR_NOERROR) {
    printf("Failed to open wave output device: %d\n", result);
    return -1;
  }

  audio_data->is_playing = true;
  printf("Windows audio playback started\n");
  return 0;
}

static int windows_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
  windows_audio_data_t* audio_data = (windows_audio_data_t*)runtime->platform_data;

  if (audio_data->wave_out) {
    waveOutClose(audio_data->wave_out);
    audio_data->wave_out = NULL;
  }

  audio_data->is_playing = false;
  printf("Windows audio playback stopped\n");
  return 0;
}

static void windows_audio_cleanup(ethervox_audio_runtime_t* runtime) {
  if (runtime->platform_data) {
    free(runtime->platform_data);
    runtime->platform_data = NULL;
  }
  printf("Windows audio driver cleaned up\n");
}

int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
  runtime->driver.init = windows_audio_init;
  runtime->driver.start_capture = windows_audio_start_capture;
  runtime->driver.stop_capture = windows_audio_stop_capture;
  runtime->driver.start_playback = windows_audio_start_playback;
  runtime->driver.stop_playback = windows_audio_stop_playback;
  runtime->driver.cleanup = windows_audio_cleanup;

  return 0;
}

#endif  // ETHERVOX_PLATFORM_WINDOWS