/**
 * @file platform_rpi.c
 * @brief Raspberry Pi-specific audio platform implementation for EthervoxAI
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

#ifdef ETHERVOX_PLATFORM_RPI
#include <alsa/asoundlib.h>
#include <bcm2835.h>

typedef struct {
    snd_pcm_t* pcm_capture;
    snd_pcm_t* pcm_playback;
    snd_pcm_hw_params_t* hw_params;
    char* audio_buffer;
    uint32_t buffer_frames;
    bool is_recording;
    bool is_playing;
    
    // GPIO for mic array control
    int mic_array_enable_pin;
    int mic_array_sel_pins[3];  // For 8-channel mic array selection
} rpi_audio_data_t;

static int rpi_gpio_init(rpi_audio_data_t* audio_data) {
    if (!bcm2835_init()) {
        printf("Failed to initialize BCM2835 library\n");
        return -1;
    }
    
    // Configure GPIO pins for mic array
    audio_data->mic_array_enable_pin = RPI_GPIO_P1_18;
    audio_data->mic_array_sel_pins[0] = RPI_GPIO_P1_22;
    audio_data->mic_array_sel_pins[1] = RPI_GPIO_P1_24;
    audio_data->mic_array_sel_pins[2] = RPI_GPIO_P1_26;
    
    bcm2835_gpio_fsel(audio_data->mic_array_enable_pin, BCM2835_GPIO_FSEL_OUTP);
    for (int i = 0; i < 3; i++) {
        bcm2835_gpio_fsel(audio_data->mic_array_sel_pins[i], BCM2835_GPIO_FSEL_OUTP);
    }
    
    // Enable mic array
    bcm2835_gpio_write(audio_data->mic_array_enable_pin, HIGH);
    
    printf("Raspberry Pi GPIO initialized for mic array\n");
    return 0;
}

static int rpi_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
    rpi_audio_data_t* audio_data = (rpi_audio_data_t*)malloc(sizeof(rpi_audio_data_t));
    if (!audio_data) {
        return -1;
    }
    
    memset(audio_data, 0, sizeof(rpi_audio_data_t));
    runtime->platform_data = audio_data;
    
    audio_data->buffer_frames = config->buffer_size;
    audio_data->audio_buffer = (char*)malloc(audio_data->buffer_frames * config->channels * sizeof(int16_t));
    
    // Initialize GPIO for mic array control
    if (rpi_gpio_init(audio_data) != 0) {
        printf("Warning: GPIO initialization failed, continuing without mic array control\n");
    }
    
    printf("Raspberry Pi audio driver initialized\n");
    return 0;
}

static int rpi_select_microphone(rpi_audio_data_t* audio_data, int mic_index) {
    if (mic_index < 0 || mic_index > 7) {
        return -1;
    }
    
    // Set selection pins based on mic index (3-bit binary)
    for (int i = 0; i < 3; i++) {
        bcm2835_gpio_write(audio_data->mic_array_sel_pins[i], 
                          (mic_index & (1 << i)) ? HIGH : LOW);
    }
    
    // Small delay for switching
    bcm2835_delay(1);
    
    return 0;
}

static int rpi_audio_start_capture(ethervox_audio_runtime_t* runtime) {
    rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;
    int err;
    
    // Select primary microphone (mic 0)
    rpi_select_microphone(audio_data, 0);
    
    // Open PCM device for recording (use hw:1,0 for USB audio or I2S HAT)
    err = snd_pcm_open(&audio_data->pcm_capture, "hw:1,0", SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        // Fallback to default device
        err = snd_pcm_open(&audio_data->pcm_capture, "default", SND_PCM_STREAM_CAPTURE, 0);
        if (err < 0) {
            printf("Cannot open audio device for capture: %s\n", snd_strerror(err));
            return -1;
        }
    }
    
    // Allocate hardware parameters object
    snd_pcm_hw_params_alloca(&audio_data->hw_params);
    
    // Configure hardware parameters for high-quality recording
    snd_pcm_hw_params_any(audio_data->pcm_capture, audio_data->hw_params);
    snd_pcm_hw_params_set_access(audio_data->pcm_capture, audio_data->hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(audio_data->pcm_capture, audio_data->hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(audio_data->pcm_capture, audio_data->hw_params, runtime->config.channels);
    
    unsigned int sample_rate = runtime->config.sample_rate;
    snd_pcm_hw_params_set_rate_near(audio_data->pcm_capture, audio_data->hw_params, &sample_rate, 0);
    
    // Optimize for low latency
    snd_pcm_hw_params_set_periods(audio_data->pcm_capture, audio_data->hw_params, 4, 0);
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
    printf("Raspberry Pi audio capture started with mic array support\n");
    return 0;
}

static int rpi_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
    rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;
    
    if (audio_data->pcm_capture) {
        snd_pcm_close(audio_data->pcm_capture);
        audio_data->pcm_capture = NULL;
    }
    
    // Disable mic array
    bcm2835_gpio_write(audio_data->mic_array_enable_pin, LOW);
    
    audio_data->is_recording = false;
    printf("Raspberry Pi audio capture stopped\n");
    return 0;
}

static int rpi_audio_start_playback(ethervox_audio_runtime_t* runtime) {
    rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;
    int err;
    
    // Open PCM device for playback
    err = snd_pcm_open(&audio_data->pcm_playback, "hw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        err = snd_pcm_open(&audio_data->pcm_playback, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            printf("Cannot open audio device for playback: %s\n", snd_strerror(err));
            return -1;
        }
    }
    
    audio_data->is_playing = true;
    printf("Raspberry Pi audio playback started\n");
    return 0;
}

static int rpi_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
    rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;
    
    if (audio_data->pcm_playback) {
        snd_pcm_close(audio_data->pcm_playback);
        audio_data->pcm_playback = NULL;
    }
    
    audio_data->is_playing = false;
    printf("Raspberry Pi audio playback stopped\n");
    return 0;
}

static void rpi_audio_cleanup(ethervox_audio_runtime_t* runtime) {
    rpi_audio_data_t* audio_data = (rpi_audio_data_t*)runtime->platform_data;
    
    if (audio_data) {
        // Cleanup GPIO
        bcm2835_close();
        
        if (audio_data->audio_buffer) {
            free(audio_data->audio_buffer);
        }
        free(audio_data);
        runtime->platform_data = NULL;
    }
    printf("Raspberry Pi audio driver cleaned up\n");
}

int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
    runtime->driver.init = rpi_audio_init;
    runtime->driver.start_capture = rpi_audio_start_capture;
    runtime->driver.stop_capture = rpi_audio_stop_capture;
    runtime->driver.start_playback = rpi_audio_start_playback;
    runtime->driver.stop_playback = rpi_audio_stop_playback;
    runtime->driver.cleanup = rpi_audio_cleanup;
    
    return 0;
}

#endif // ETHERVOX_PLATFORM_RPI