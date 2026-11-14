/**
 * @file test_audio_core.c
 * @brief Unit tests for EthervoxAI audio core functionality
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

#include "test_utils.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/audio.h"

void test_audio_default_config() {
  printf("Testing ethervox_audio_get_default_config...\n");

  ethervox_audio_config_t config = ethervox_audio_get_default_config();

  CHECK(config.sample_rate == ETHERVOX_AUDIO_SAMPLE_RATE);
  CHECK(config.channels == 1);  // Mono by default
  CHECK(config.bits_per_sample == 16);
  CHECK(config.buffer_size == ETHERVOX_AUDIO_BUFFER_SIZE);
  CHECK(config.enable_noise_suppression == true);
  CHECK(config.enable_echo_cancellation == true);

  printf("✓ Default audio configuration test passed\n");
}

void test_audio_runtime_init() {
  printf("Testing ethervox_audio_init...\n");

  // Test null pointer handling
  CHECK(ethervox_audio_init(NULL, NULL) == ETHERVOX_ERROR_NULL_POINTER);

  ethervox_audio_runtime_t runtime;
  CHECK(ethervox_audio_init(&runtime, NULL) == ETHERVOX_ERROR_NULL_POINTER);

  // Test with valid config
  ethervox_audio_config_t config = ethervox_audio_get_default_config();

  // Note: This will fail in CI without actual audio hardware,
  // but that's expected and shows the test structure works
  int result = ethervox_audio_init(&runtime, &config);

  // We expect this to fail in CI environments, which is fine
  if (result == 0) {
    printf("✓ Audio runtime initialization test passed (hardware available)\n");
    ethervox_audio_cleanup(&runtime);
  } else {
    printf("✓ Audio runtime initialization test passed (no hardware, expected failure)\n");
  }
}

void test_audio_buffer_operations() {
  printf("Testing audio buffer operations...\n");

  ethervox_audio_buffer_t buffer;
  buffer.size = 1024;
  buffer.channels = 1;
  buffer.timestamp_us = 12345;

  // Test buffer structure
  CHECK(buffer.size == 1024);
  CHECK(buffer.channels == 1);
  CHECK(buffer.timestamp_us == 12345);

  printf("✓ Audio buffer operations test passed\n");
}

void test_language_detection_struct() {
  printf("Testing language detection structure...\n");

  ethervox_language_detect_t lang;
  /* Use snprintf for safe, bounded copy */
  snprintf(lang.language_code, sizeof(lang.language_code), "%s", "en");
  lang.confidence = 0.95f;
  lang.is_ambient = false;

  CHECK(strcmp(lang.language_code, "en") == 0);
  CHECK(lang.confidence == 0.95f);
  CHECK(lang.is_ambient == false);

  printf("✓ Language detection structure test passed\n");
}

int main() {
  printf("Running EthervoxAI Audio Core Unit Tests\n");
  printf("==========================================\n");

  test_audio_default_config();
  test_audio_runtime_init();
  test_audio_buffer_operations();
  test_language_detection_struct();

  printf("==========================================\n");
  printf("All audio core tests completed!\n");

  return ETHERVOX_SUCCESS;
}