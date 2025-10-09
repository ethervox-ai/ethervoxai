/**
 * @file platform_linux.c
 * @brief Linux-specific audio platform implementation for EthervoxAI
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
#include <stdio.h>

#ifdef ETHERVOX_PLATFORM_LINUX
#include <alsa/asoundlib.h>
#include <stdbool.h>

typedef struct {
    snd_pcm_t* pcm_capture;
    snd_pcm_t* pcm_playback;
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_uframes_t buffer_frames;
    char* audio_buffer;
    bool is_recording;
    bool is_playing;
} linux_audio_data_t;

static int linux_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)malloc(sizeof(linux_audio_data_t));
    if (!audio_data) {
        return -1;
    }
    
    memset(audio_data, 0, sizeof(linux_audio_data_t));
    runtime->platform_data = audio_data;
    
    audio_data->buffer_frames = config->buffer_size;
    audio_data->audio_buffer = (char*)malloc(audio_data->buffer_frames * config->channels * sizeof(int16_t));
    
    printf("Linux ALSA audio driver initialized\n");
    return 0;
}

static int linux_audio_start_capture(ethervox_audio_runtime_t* runtime) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    int err;
    
    // Open PCM device for recording
    err = snd_pcm_open(&audio_data->pcm_capture, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        printf("Cannot open audio device for capture: %s\n", snd_strerror(err));
        return -1;
    }
    
    // Allocate hardware parameters object
    snd_pcm_hw_params_alloca(&audio_data->hw_params);
    
    // Configure hardware parameters
    snd_pcm_hw_params_any(audio_data->pcm_capture, audio_data->hw_params);
    snd_pcm_hw_params_set_access(audio_data->pcm_capture, audio_data->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(audio_data->pcm_capture, audio_data->hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(audio_data->pcm_capture, audio_data->hw_params, runtime->config.channels);
    
    unsigned int sample_rate = runtime->config.sample_rate;
    snd_pcm_hw_params_set_rate_near(audio_data->pcm_capture, audio_data->hw_params, &sample_rate, 0);
    
    snd_pcm_hw_params_set_periods(audio_data->pcm_capture, audio_data->hw_params, 2, 0);
    snd_pcm_hw_params_set_period_size_near(audio_data->pcm_capture, audio_data->hw_params, &audio_data->buffer_frames, 0);
    
    // Apply hardware parameters
    err = snd_pcm_hw_params(audio_data->pcm_capture, audio_data->hw_params);
    if (err < 0) {
        printf("Cannot set hardware parameters: %s\n", snd_strerror(err));
        return -1;
    }
    
    // Prepare device
    err = snd_pcm_prepare(audio_data->pcm_capture);
    if (err < 0) {
        printf("Cannot prepare audio interface for use: %s\n", snd_strerror(err));
        return -1;
    }
    
    audio_data->is_recording = true;
    printf("Linux audio capture started\n");
    return 0;
}

static int linux_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    
    if (audio_data->pcm_capture) {
        snd_pcm_close(audio_data->pcm_capture);
        audio_data->pcm_capture = NULL;
    }
    
    audio_data->is_recording = false;
    printf("Linux audio capture stopped\n");
    return 0;
}

static int linux_audio_start_playback(ethervox_audio_runtime_t* runtime) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    int err;
    
    // Open PCM device for playback
    err = snd_pcm_open(&audio_data->pcm_playback, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        printf("Cannot open audio device for playback: %s\n", snd_strerror(err));
        return -1;
    }
    
    audio_data->is_playing = true;
    printf("Linux audio playback started\n");
    return 0;
}

static int linux_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    
    if (audio_data->pcm_playback) {
        snd_pcm_close(audio_data->pcm_playback);
        audio_data->pcm_playback = NULL;
    }
    
    audio_data->is_playing = false;
    printf("Linux audio playback stopped\n");
    return 0;
}

static void linux_audio_cleanup(ethervox_audio_runtime_t* runtime) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    
    if (audio_data) {
        if (audio_data->audio_buffer) {
            free(audio_data->audio_buffer);
        }
        free(audio_data);
        runtime->platform_data = NULL;
    }
    printf("Linux audio driver cleaned up\n");
}

int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
    runtime->driver.init = linux_audio_init;
    runtime->driver.start_capture = linux_audio_start_capture;
    runtime->driver.stop_capture = linux_audio_stop_capture;
    runtime->driver.start_playback = linux_audio_start_playback;
    runtime->driver.stop_playback = linux_audio_stop_playback;
    runtime->driver.cleanup = linux_audio_cleanup;
    
    return 0;
}

#endif // ETHERVOX_PLATFORM_LINUX