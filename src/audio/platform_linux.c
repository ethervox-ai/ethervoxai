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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef ETHERVOX_PLATFORM_LINUX
#include <alsa/asoundlib.h>
#include <stdbool.h>

typedef struct {
    snd_pcm_t* pcm_capture;
    snd_pcm_t* pcm_playback;
    snd_pcm_uframes_t buffer_frames;
    bool is_recording;
    bool is_playing;
} linux_audio_data_t;

static int linux_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)calloc(1, sizeof(linux_audio_data_t));
    if (!audio_data) {
        return -1;
    }
    runtime->platform_data = audio_data;
    
    audio_data->buffer_frames = config->buffer_size;
    printf("Linux ALSA audio driver initialized\n");
    return 0;
}

static int linux_audio_start_capture(ethervox_audio_runtime_t* runtime) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    int err;
    snd_pcm_hw_params_t* hw_params = NULL;

    const char* env_device = getenv("ETHERVOX_ALSA_DEVICE");
    const char* candidates[3] = { env_device, "default", "sysdefault" };
    const size_t candidate_count = env_device && *env_device ? 3 : 2;

    const char* opened_device = NULL;
    for (size_t i = 0; i < candidate_count; ++i) {
        const char* device = candidates[i];
        if (!device || !*device) {
            continue;
        }

        err = snd_pcm_open(&audio_data->pcm_capture, device, SND_PCM_STREAM_CAPTURE, 0);
        if (err >= 0) {
            opened_device = device;
            break;
        }

        printf("ALSA: failed to open capture device '%s': %s\n", device, snd_strerror(err));
    }

    if (!opened_device) {
        printf("Cannot open audio device for capture: %s\n", snd_strerror(err));
        return -1;
    }

    printf("ALSA: using capture device '%s'\n", opened_device);
    
    // Allocate hardware parameters object
    err = snd_pcm_hw_params_malloc(&hw_params);
    if (err < 0) {
        printf("Cannot allocate hardware parameters: %s\n", snd_strerror(err));
        snd_pcm_close(audio_data->pcm_capture);
        audio_data->pcm_capture = NULL;
        return -1;
    }
    
    // Configure hardware parameters
    snd_pcm_hw_params_any(audio_data->pcm_capture, hw_params);
    snd_pcm_hw_params_set_access(audio_data->pcm_capture, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(audio_data->pcm_capture, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(audio_data->pcm_capture, hw_params, runtime->config.channels);
    
    unsigned int sample_rate = runtime->config.sample_rate;
    snd_pcm_hw_params_set_rate_near(audio_data->pcm_capture, hw_params, &sample_rate, 0);
    
    snd_pcm_hw_params_set_periods(audio_data->pcm_capture, hw_params, 2, 0);
    snd_pcm_hw_params_set_period_size_near(audio_data->pcm_capture, hw_params, &audio_data->buffer_frames, 0);
    
    // Apply hardware parameters
    err = snd_pcm_hw_params(audio_data->pcm_capture, hw_params);
    snd_pcm_hw_params_free(hw_params);
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

    const char* env_device = getenv("ETHERVOX_ALSA_PLAYBACK");
    const char* candidates[3] = { env_device, "default", "sysdefault" };
    const size_t candidate_count = env_device && *env_device ? 3 : 2;

    const char* opened_device = NULL;
    for (size_t i = 0; i < candidate_count; ++i) {
        const char* device = candidates[i];
        if (!device || !*device) {
            continue;
        }

        err = snd_pcm_open(&audio_data->pcm_playback, device, SND_PCM_STREAM_PLAYBACK, 0);
        if (err >= 0) {
            opened_device = device;
            break;
        }

        printf("ALSA: failed to open playback device '%s': %s\n", device, snd_strerror(err));
    }

    if (!opened_device) {
        printf("Cannot open audio device for playback: %s\n", snd_strerror(err));
        return -1;
    }

    printf("ALSA: using playback device '%s'\n", opened_device);
    
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

static uint64_t linux_get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static int linux_audio_read(ethervox_audio_runtime_t* runtime, ethervox_audio_buffer_t* buffer) {
    if (!runtime || !buffer || !runtime->platform_data) {
        return -1;
    }

    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    if (!audio_data->pcm_capture || !audio_data->is_recording) {
        return -1;
    }

    const uint32_t channels = runtime->config.channels;
    const uint32_t frames_requested = runtime->config.buffer_size;
    const size_t samples_total = (size_t)frames_requested * channels;
    const size_t bytes_total = samples_total * sizeof(int16_t);

    int16_t* capture_buffer = (int16_t*)malloc(bytes_total);
    if (!capture_buffer) {
        return -1;
    }

    uint32_t frames_read = 0;
    while (frames_read < frames_requested) {
        snd_pcm_sframes_t rc = snd_pcm_readi(
            audio_data->pcm_capture,
            capture_buffer + (size_t)frames_read * channels,
            frames_requested - frames_read
        );

        if (rc == -EPIPE) {
            snd_pcm_prepare(audio_data->pcm_capture);
            continue;
        } else if (rc == -EAGAIN) {
            continue;
        } else if (rc < 0) {
            printf("ALSA capture error: %s\n", snd_strerror(rc));
            free(capture_buffer);
            return -1;
        }

        frames_read += (uint32_t)rc;
    }

    buffer->data = (float*)capture_buffer;
    buffer->size = frames_read * channels * sizeof(int16_t);
    buffer->channels = channels;
    buffer->timestamp_us = linux_get_timestamp_us();

    return 0;
}

static void linux_audio_cleanup(ethervox_audio_runtime_t* runtime) {
    linux_audio_data_t* audio_data = (linux_audio_data_t*)runtime->platform_data;
    
    if (audio_data) {
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
    runtime->driver.read_audio = linux_audio_read;
    runtime->driver.cleanup = linux_audio_cleanup;
    
    return 0;
}

#endif // ETHERVOX_PLATFORM_LINUX