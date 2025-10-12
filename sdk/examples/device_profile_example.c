/**
 * @file device_profile_example.c
 * @brief Source file for EthervoxAI
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../ethervox_sdk.h"

// Example Device Profile: Custom Voice Assistant Device
// Demonstrates how to create and configure device profiles for different hardware

// Custom device configurations
typedef struct {
  const char* name;
  const char* platform;
  const char* description;
  void (*configure_device)(ethervox_device_profile_t* profile);
} device_template_t;

// Configure Raspberry Pi 4 with ReSpeaker 4-Mic Array
static void configure_rpi4_respeaker(ethervox_device_profile_t* profile) {
  strcpy(profile->name, "RaspberryPi-ReSpeaker");
  strcpy(profile->hardware_revision, "4.0");
  strcpy(profile->platform, "Raspberry Pi");

  // Audio configuration for 4-mic array
  profile->mic_array_channels = 4;
  profile->sample_rate = 48000;
  profile->bit_depth = 16;
  profile->mic_sensitivity = -26.0f;  // dBFS
  profile->has_echo_cancellation = true;
  profile->has_noise_suppression = true;

  // GPIO pin assignments for ReSpeaker HAT
  profile->gpio_pins.led_status = 12;     // Status LED
  profile->gpio_pins.led_recording = 13;  // Recording indicator
  profile->gpio_pins.button_mute = 17;    // Mute button
  profile->gpio_pins.button_wake = 18;    // Wake button
  profile->gpio_pins.i2c_sda = 2;         // I2C data
  profile->gpio_pins.i2c_scl = 3;         // I2C clock
  profile->gpio_pins.spi_mosi = 10;       // SPI MOSI
  profile->gpio_pins.spi_miso = 9;        // SPI MISO
  profile->gpio_pins.spi_sclk = 11;       // SPI clock
  profile->gpio_pins.spi_cs = 8;          // SPI chip select

  // Power management
  profile->supports_low_power_mode = true;
  profile->sleep_timeout_ms = 300000;  // 5 minutes
  profile->battery_low_threshold_v = 3.3f;

  // Network capabilities
  profile->has_wifi = true;
  profile->has_ethernet = true;
  profile->has_bluetooth = true;
  strcpy(profile->default_ssid, "EtherVox-RPI");

  // Processing capabilities
  profile->supports_edge_inference = true;
  profile->max_concurrent_streams = 4;
  strcpy(profile->preferred_model, "whisper-base");
}

// Configure ESP32-S3 with built-in microphone
static void configure_esp32_s3_builtin(ethervox_device_profile_t* profile) {
  strcpy(profile->name, "ESP32-S3-Builtin");
  strcpy(profile->hardware_revision, "S3");
  strcpy(profile->platform, "ESP32");

  // Audio configuration for single microphone
  profile->mic_array_channels = 1;
  profile->sample_rate = 16000;
  profile->bit_depth = 16;
  profile->mic_sensitivity = -30.0f;       // dBFS
  profile->has_echo_cancellation = false;  // Limited processing power
  profile->has_noise_suppression = false;

  // GPIO pin assignments for ESP32-S3
  profile->gpio_pins.led_status = 2;     // Built-in LED
  profile->gpio_pins.led_recording = 4;  // External recording LED
  profile->gpio_pins.button_mute = 0;    // Boot button as mute
  profile->gpio_pins.button_wake = 9;    // External wake button
  profile->gpio_pins.i2c_sda = 21;       // I2C data
  profile->gpio_pins.i2c_scl = 22;       // I2C clock
  profile->gpio_pins.spi_mosi = 23;      // SPI MOSI
  profile->gpio_pins.spi_miso = 19;      // SPI MISO
  profile->gpio_pins.spi_sclk = 18;      // SPI clock
  profile->gpio_pins.spi_cs = 5;         // SPI chip select

  // Power management
  profile->supports_low_power_mode = true;
  profile->sleep_timeout_ms = 60000;  // 1 minute (battery powered)
  profile->battery_low_threshold_v = 3.0f;

  // Network capabilities
  profile->has_wifi = true;
  profile->has_ethernet = false;
  profile->has_bluetooth = true;
  strcpy(profile->default_ssid, "EtherVox-ESP32");

  // Processing capabilities
  profile->supports_edge_inference = false;  // Limited memory
  profile->max_concurrent_streams = 1;
  strcpy(profile->preferred_model, "whisper-tiny");
}

// Configure desktop development setup
static void configure_desktop_dev(ethervox_device_profile_t* profile) {
  strcpy(profile->name, "Desktop-Development");
  strcpy(profile->hardware_revision, "1.0");
  strcpy(profile->platform, "Desktop");

  // Audio configuration for USB microphone
  profile->mic_array_channels = 2;  // Stereo USB mic
  profile->sample_rate = 48000;
  profile->bit_depth = 16;
  profile->mic_sensitivity = -20.0f;  // Good quality USB mic
  profile->has_echo_cancellation = true;
  profile->has_noise_suppression = true;

  // No GPIO pins available on desktop
  memset(&profile->gpio_pins, 0, sizeof(profile->gpio_pins));

  // Power management not applicable
  profile->supports_low_power_mode = false;
  profile->sleep_timeout_ms = 0;
  profile->battery_low_threshold_v = 0.0f;

  // Network capabilities
  profile->has_wifi = true;
  profile->has_ethernet = true;
  profile->has_bluetooth = true;
  strcpy(profile->default_ssid, "EtherVox-Desktop");

  // Processing capabilities
  profile->supports_edge_inference = true;
  profile->max_concurrent_streams = 8;
  strcpy(profile->preferred_model, "whisper-large");
}

// Available device templates
static device_template_t device_templates[] = {
    {"RaspberryPi-ReSpeaker", "Raspberry Pi", "Raspberry Pi 4 with ReSpeaker 4-Mic Array HAT",
     configure_rpi4_respeaker},
    {"ESP32-S3-Builtin", "ESP32", "ESP32-S3 with built-in microphone and basic peripherals",
     configure_esp32_s3_builtin},
    {"Desktop-Development", "Desktop", "Desktop computer for development and testing",
     configure_desktop_dev}};

// Print device profile information
static void print_device_profile(const ethervox_device_profile_t* profile) {
  printf("Device Profile: %s\n", profile->name);
  printf("  Platform: %s (%s)\n", profile->platform, profile->hardware_revision);
  printf("  Audio: %d channels @ %d Hz, %d-bit\n", profile->mic_array_channels,
         profile->sample_rate, profile->bit_depth);
  printf("  Mic Sensitivity: %.1f dBFS\n", profile->mic_sensitivity);
  printf("  Echo Cancellation: %s\n", profile->has_echo_cancellation ? "Yes" : "No");
  printf("  Noise Suppression: %s\n", profile->has_noise_suppression ? "Yes" : "No");

  printf("  GPIO Pins:\n");
  printf("    Status LED: %d, Recording LED: %d\n", profile->gpio_pins.led_status,
         profile->gpio_pins.led_recording);
  printf("    Mute Button: %d, Wake Button: %d\n", profile->gpio_pins.button_mute,
         profile->gpio_pins.button_wake);
  printf("    I2C: SDA=%d, SCL=%d\n", profile->gpio_pins.i2c_sda, profile->gpio_pins.i2c_scl);
  printf("    SPI: MOSI=%d, MISO=%d, SCLK=%d, CS=%d\n", profile->gpio_pins.spi_mosi,
         profile->gpio_pins.spi_miso, profile->gpio_pins.spi_sclk, profile->gpio_pins.spi_cs);

  printf("  Power Management:\n");
  printf("    Low Power Mode: %s\n", profile->supports_low_power_mode ? "Yes" : "No");
  printf("    Sleep Timeout: %d ms\n", profile->sleep_timeout_ms);
  printf("    Battery Threshold: %.1f V\n", profile->battery_low_threshold_v);

  printf("  Network:\n");
  printf("    WiFi: %s, Ethernet: %s, Bluetooth: %s\n", profile->has_wifi ? "Yes" : "No",
         profile->has_ethernet ? "Yes" : "No", profile->has_bluetooth ? "Yes" : "No");
  printf("    Default SSID: %s\n", profile->default_ssid);

  printf("  Processing:\n");
  printf("    Edge Inference: %s\n", profile->supports_edge_inference ? "Yes" : "No");
  printf("    Max Concurrent Streams: %d\n", profile->max_concurrent_streams);
  printf("    Preferred Model: %s\n", profile->preferred_model);

  printf("\n");
}

// Save device profile to file (simple text format)
static int save_device_profile_to_file(const ethervox_device_profile_t* profile,
                                       const char* filename) {
  FILE* file = fopen(filename, "w");
  if (!file) {
    printf("Failed to open file for writing: %s\n", filename);
    return -1;
  }

  fprintf(file, "# EtherVox Device Profile\n");
  fprintf(file, "# Generated on: %s\n", ctime(&(time_t){time(NULL)}));
  fprintf(file, "\n[device]\n");
  fprintf(file, "name = %s\n", profile->name);
  fprintf(file, "hardware_revision = %s\n", profile->hardware_revision);
  fprintf(file, "platform = %s\n", profile->platform);

  fprintf(file, "\n[audio]\n");
  fprintf(file, "mic_array_channels = %d\n", profile->mic_array_channels);
  fprintf(file, "sample_rate = %d\n", profile->sample_rate);
  fprintf(file, "bit_depth = %d\n", profile->bit_depth);
  fprintf(file, "mic_sensitivity = %.1f\n", profile->mic_sensitivity);
  fprintf(file, "echo_cancellation = %s\n", profile->has_echo_cancellation ? "true" : "false");
  fprintf(file, "noise_suppression = %s\n", profile->has_noise_suppression ? "true" : "false");

  fprintf(file, "\n[gpio]\n");
  fprintf(file, "led_status = %d\n", profile->gpio_pins.led_status);
  fprintf(file, "led_recording = %d\n", profile->gpio_pins.led_recording);
  fprintf(file, "button_mute = %d\n", profile->gpio_pins.button_mute);
  fprintf(file, "button_wake = %d\n", profile->gpio_pins.button_wake);
  fprintf(file, "i2c_sda = %d\n", profile->gpio_pins.i2c_sda);
  fprintf(file, "i2c_scl = %d\n", profile->gpio_pins.i2c_scl);
  fprintf(file, "spi_mosi = %d\n", profile->gpio_pins.spi_mosi);
  fprintf(file, "spi_miso = %d\n", profile->gpio_pins.spi_miso);
  fprintf(file, "spi_sclk = %d\n", profile->gpio_pins.spi_sclk);
  fprintf(file, "spi_cs = %d\n", profile->gpio_pins.spi_cs);

  fprintf(file, "\n[power]\n");
  fprintf(file, "low_power_mode = %s\n", profile->supports_low_power_mode ? "true" : "false");
  fprintf(file, "sleep_timeout_ms = %d\n", profile->sleep_timeout_ms);
  fprintf(file, "battery_threshold_v = %.1f\n", profile->battery_low_threshold_v);

  fprintf(file, "\n[network]\n");
  fprintf(file, "wifi = %s\n", profile->has_wifi ? "true" : "false");
  fprintf(file, "ethernet = %s\n", profile->has_ethernet ? "true" : "false");
  fprintf(file, "bluetooth = %s\n", profile->has_bluetooth ? "true" : "false");
  fprintf(file, "default_ssid = %s\n", profile->default_ssid);

  fprintf(file, "\n[processing]\n");
  fprintf(file, "edge_inference = %s\n", profile->supports_edge_inference ? "true" : "false");
  fprintf(file, "max_concurrent_streams = %d\n", profile->max_concurrent_streams);
  fprintf(file, "preferred_model = %s\n", profile->preferred_model);

  fclose(file);

  printf("Device profile saved to: %s\n", filename);
  return 0;
}

// Example usage
int main() {
  printf("=== EtherVox SDK Device Profile Example ===\n\n");

  // Initialize SDK
  ethervox_sdk_t sdk;
  if (ethervox_sdk_init(&sdk) != 0) {
    printf("Failed to initialize SDK\n");
    return 1;
  }

  // Show available device templates
  printf("Available device templates:\n");
  for (int i = 0; i < 3; i++) {
    printf("  %d. %s (%s)\n     %s\n", i + 1, device_templates[i].name,
           device_templates[i].platform, device_templates[i].description);
  }
  printf("\n");

  // Create and configure different device profiles
  for (int i = 0; i < 3; i++) {
    printf("=== Configuring %s ===\n", device_templates[i].name);

    // Create device profile
    ethervox_device_profile_t profile;
    memset(&profile, 0, sizeof(profile));

    // Configure using template
    device_templates[i].configure_device(&profile);

    // Set the profile in SDK
    if (sdk.device_profile) {
      *sdk.device_profile = profile;
    }

    // Print profile information
    print_device_profile(&profile);

    // Save profile to file
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_profile.conf", device_templates[i].name);
    save_device_profile_to_file(&profile, filename);

    printf("---\n\n");
  }

  // Demonstrate profile comparison
  printf("=== Device Profile Comparison ===\n");
  printf("| Feature              | RPI-ReSpeaker | ESP32-S3 | Desktop     |\n");
  printf("|---------------------|---------------|----------|-------------|\n");

  ethervox_device_profile_t profiles[3];
  for (int i = 0; i < 3; i++) {
    memset(&profiles[i], 0, sizeof(profiles[i]));
    device_templates[i].configure_device(&profiles[i]);
  }

  printf("| Mic Channels         | %-13d | %-8d | %-11d |\n", profiles[0].mic_array_channels,
         profiles[1].mic_array_channels, profiles[2].mic_array_channels);
  printf("| Sample Rate          | %-13d | %-8d | %-11d |\n", profiles[0].sample_rate,
         profiles[1].sample_rate, profiles[2].sample_rate);
  printf("| Echo Cancellation    | %-13s | %-8s | %-11s |\n",
         profiles[0].has_echo_cancellation ? "Yes" : "No",
         profiles[1].has_echo_cancellation ? "Yes" : "No",
         profiles[2].has_echo_cancellation ? "Yes" : "No");
  printf("| Max Concurrent       | %-13d | %-8d | %-11d |\n", profiles[0].max_concurrent_streams,
         profiles[1].max_concurrent_streams, profiles[2].max_concurrent_streams);
  printf("| Edge Inference       | %-13s | %-8s | %-11s |\n",
         profiles[0].supports_edge_inference ? "Yes" : "No",
         profiles[1].supports_edge_inference ? "Yes" : "No",
         profiles[2].supports_edge_inference ? "Yes" : "No");

  printf(
      "\nDevice profiles demonstrate how EtherVox can be adapted to different hardware "
      "configurations\n");
  printf("while maintaining consistent API and functionality across platforms.\n");

  // Cleanup
  ethervox_sdk_cleanup(&sdk);

  return 0;
}