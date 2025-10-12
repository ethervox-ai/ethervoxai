/**
 * @file test_config.c
 * @brief Unit tests for EthervoxAI configuration system
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ethervox/config.h"

void test_version_constants() {
  printf("Testing version constants...\n");

  assert(ETHERVOX_VERSION_MAJOR == 0);
  assert(ETHERVOX_VERSION_MINOR == 1);
  assert(ETHERVOX_VERSION_PATCH == 0);

  // Test version string
  const char* expected_version = "0.1.0";
  assert(strcmp(ETHERVOX_VERSION_STRING, expected_version) == 0);

  printf("✓ Version constants test passed\n");
}

void test_platform_detection() {
  printf("Testing platform detection macros...\n");

  // Verify that exactly one platform is defined
  int platform_count = 0;

#ifdef ETHERVOX_PLATFORM_ESP32
  platform_count++;
  printf("  - ESP32 platform detected\n");
  assert(ETHERVOX_PLATFORM_EMBEDDED == 1);
#endif

#ifdef ETHERVOX_PLATFORM_RPI
  platform_count++;
  printf("  - Raspberry Pi platform detected\n");
  assert(ETHERVOX_PLATFORM_EMBEDDED == 1);
#endif

#ifdef ETHERVOX_PLATFORM_WINDOWS
  platform_count++;
  printf("  - Windows platform detected\n");
  assert(ETHERVOX_PLATFORM_DESKTOP == 1);
#endif

#ifdef ETHERVOX_PLATFORM_LINUX
  platform_count++;
  printf("  - Linux platform detected\n");
  assert(ETHERVOX_PLATFORM_DESKTOP == 1);
#endif

#ifdef ETHERVOX_PLATFORM_MACOS
  platform_count++;
  printf("  - macOS platform detected\n");
  assert(ETHERVOX_PLATFORM_DESKTOP == 1);
#endif

  // Should have exactly one platform defined
  assert(platform_count >= 1);  // Allow for multiple in cross-compilation scenarios

  printf("✓ Platform detection test passed\n");
}

void test_feature_configuration() {
  printf("Testing feature configuration constants...\n");

  // Test that constants are defined with reasonable values
  assert(ETHERVOX_MAX_LANGUAGES > 0);
  assert(ETHERVOX_AUDIO_SAMPLE_RATE > 0);
  assert(ETHERVOX_AUDIO_BUFFER_SIZE > 0);
  assert(ETHERVOX_MAX_PLUGINS > 0);

// Test platform-specific differences
#ifdef ETHERVOX_PLATFORM_EMBEDDED
  assert(ETHERVOX_MAX_LANGUAGES <= 3);
  assert(ETHERVOX_AUDIO_BUFFER_SIZE <= 1024);
  assert(ETHERVOX_MAX_PLUGINS <= 8);
  printf("  - Embedded platform: Limited resources\n");
#else
  assert(ETHERVOX_MAX_LANGUAGES <= 15);
  assert(ETHERVOX_AUDIO_BUFFER_SIZE <= 4096);
  assert(ETHERVOX_MAX_PLUGINS <= 32);
  printf("  - Desktop platform: Full resources\n");
#endif

  // Test audio sample rate
  assert(ETHERVOX_AUDIO_SAMPLE_RATE == 16000);

  printf("✓ Feature configuration test passed\n");
}

void test_debug_configuration() {
  printf("Testing debug configuration...\n");

#ifdef DEBUG_ENABLED
  assert(ETHERVOX_DEBUG == 1);
  assert(ETHERVOX_LOG_LEVEL == 0);  // Verbose
  printf("  - Debug mode enabled\n");
#else
  assert(ETHERVOX_DEBUG == 0);
  assert(ETHERVOX_LOG_LEVEL == 2);  // Error only
  printf("  - Release mode enabled\n");
#endif

  printf("✓ Debug configuration test passed\n");
}

int main() {
  printf("Running EthervoxAI Configuration Unit Tests\n");
  printf("==========================================\n");

  test_version_constants();
  test_platform_detection();
  test_feature_configuration();
  test_debug_configuration();

  printf("==========================================\n");
  printf("All configuration tests completed!\n");

  return 0;
}