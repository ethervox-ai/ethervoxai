/*
 * EthervoxAI - Open Source Voice Assistant Platform
 */

#include <stdio.h>

#include "esp_system.h"
#include "ethervox/audio.h"
#include "ethervox/platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  printf("EthervoxAI starting on ESP32...\n");

  // Initialize platform
  ethervox_platform_t platform;
  if (ethervox_platform_init(&platform) != 0) {
    printf("Failed to initialize platform\n");
    return;
  }

  printf("Platform: %s\n", ethervox_platform_get_name());

  // Initialize audio with config
  ethervox_audio_runtime_t audio;
  ethervox_audio_config_t audio_config = {0};  // Use default config
  if (ethervox_audio_init(&audio, &audio_config) != 0) {
    printf("Failed to initialize audio\n");
    return;
  }

  printf("EthervoxAI initialized successfully\n");

  // Main loop
  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}