#include "ethervox/audio.h"
#include <stdio.h>

#ifdef ETHERVOX_PLATFORM_ESP32
#include "driver/i2s.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ESP32_AUDIO";

typedef struct {
    i2s_port_t i2s_port;
    i2s_config_t i2s_config;
    i2s_pin_config_t pin_config;
    uint8_t* i2s_buffer;
    size_t buffer_size;
    bool is_recording;
    bool is_playing;
    TaskHandle_t audio_task_handle;
} esp32_audio_data_t;

static void esp32_audio_task(void* parameter) {
    ethervox_audio_runtime_t* runtime = (ethervox_audio_runtime_t*)parameter;
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    
    size_t bytes_read;
    ethervox_audio_buffer_t buffer;
    
    while (audio_data->is_recording) {
        // Read audio data from I2S
        esp_err_t result = i2s_read(audio_data->i2s_port, audio_data->i2s_buffer, 
                                   audio_data->buffer_size, &bytes_read, portMAX_DELAY);
        
        if (result == ESP_OK && bytes_read > 0) {
            // Convert to float and call callback
            buffer.data = (float*)malloc(bytes_read / 2 * sizeof(float));
            buffer.size = bytes_read / 2;
            buffer.channels = runtime->config.channels;
            buffer.timestamp_us = esp_timer_get_time();
            
            // Convert 16-bit PCM to float
            int16_t* pcm_data = (int16_t*)audio_data->i2s_buffer;
            for (size_t i = 0; i < buffer.size; i++) {
                buffer.data[i] = pcm_data[i] / 32768.0f;
            }
            
            if (runtime->on_audio_data) {
                runtime->on_audio_data(&buffer, runtime->user_data);
            }
            
            ethervox_audio_buffer_free(&buffer);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(NULL);
}

static int esp32_audio_init(ethervox_audio_runtime_t* runtime, const ethervox_audio_config_t* config) {
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)malloc(sizeof(esp32_audio_data_t));
    if (!audio_data) {
        return -1;
    }
    
    memset(audio_data, 0, sizeof(esp32_audio_data_t));
    runtime->platform_data = audio_data;
    
    audio_data->i2s_port = I2S_NUM_0;
    audio_data->buffer_size = config->buffer_size * 2; // 16-bit samples
    audio_data->i2s_buffer = (uint8_t*)malloc(audio_data->buffer_size);
    
    if (!audio_data->i2s_buffer) {
        free(audio_data);
        return -1;
    }
    
    // I2S configuration
    audio_data->i2s_config = (i2s_config_t) {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX,
        .sample_rate = config->sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = (config->channels == 1) ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 2,
        .dma_buf_len = config->buffer_size / 2,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    // I2S pin configuration (adjust for your hardware)
    audio_data->pin_config = (i2s_pin_config_t) {
        .bck_io_num = 26,     // Bit clock
        .ws_io_num = 25,      // Word select
        .data_out_num = 22,   // Data out (for playback)
        .data_in_num = 23     // Data in (for recording)
    };
    
    ESP_LOGI(TAG, "ESP32 audio driver initialized");
    return 0;
}

static int esp32_audio_start_capture(ethervox_audio_runtime_t* runtime) {
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    
    // Install and start I2S driver
    esp_err_t result = i2s_driver_install(audio_data->i2s_port, &audio_data->i2s_config, 0, NULL);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(result));
        return -1;
    }
    
    result = i2s_set_pin(audio_data->i2s_port, &audio_data->pin_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S pins: %s", esp_err_to_name(result));
        i2s_driver_uninstall(audio_data->i2s_port);
        return -1;
    }
    
    // Start audio processing task
    audio_data->is_recording = true;
    xTaskCreate(esp32_audio_task, "audio_task", 4096, runtime, 5, &audio_data->audio_task_handle);
    
    ESP_LOGI(TAG, "ESP32 audio capture started");
    return 0;
}

static int esp32_audio_stop_capture(ethervox_audio_runtime_t* runtime) {
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    
    audio_data->is_recording = false;
    
    if (audio_data->audio_task_handle) {
        vTaskDelete(audio_data->audio_task_handle);
        audio_data->audio_task_handle = NULL;
    }
    
    i2s_driver_uninstall(audio_data->i2s_port);
    
    ESP_LOGI(TAG, "ESP32 audio capture stopped");
    return 0;
}

static int esp32_audio_start_playback(ethervox_audio_runtime_t* runtime) {
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    audio_data->is_playing = true;
    ESP_LOGI(TAG, "ESP32 audio playback started");
    return 0;
}

static int esp32_audio_stop_playback(ethervox_audio_runtime_t* runtime) {
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    audio_data->is_playing = false;
    ESP_LOGI(TAG, "ESP32 audio playback stopped");
    return 0;
}

static void esp32_audio_cleanup(ethervox_audio_runtime_t* runtime) {
    esp32_audio_data_t* audio_data = (esp32_audio_data_t*)runtime->platform_data;
    
    if (audio_data) {
        if (audio_data->i2s_buffer) {
            free(audio_data->i2s_buffer);
        }
        free(audio_data);
        runtime->platform_data = NULL;
    }
    ESP_LOGI(TAG, "ESP32 audio driver cleaned up");
}

int ethervox_audio_register_platform_driver(ethervox_audio_runtime_t* runtime) {
    runtime->driver.init = esp32_audio_init;
    runtime->driver.start_capture = esp32_audio_start_capture;
    runtime->driver.stop_capture = esp32_audio_stop_capture;
    runtime->driver.start_playback = esp32_audio_start_playback;
    runtime->driver.stop_playback = esp32_audio_stop_playback;
    runtime->driver.cleanup = esp32_audio_cleanup;
    
    return 0;
}

#endif // ETHERVOX_PLATFORM_ESP32