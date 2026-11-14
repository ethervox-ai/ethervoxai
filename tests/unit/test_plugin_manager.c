/**
 * @file test_plugin_manager.c
 * @brief Unit tests for EthervoxAI plugin management system
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

#include "ethervox/plugins.h"

void test_plugin_type_to_string() {
  printf("Testing ethervox_plugin_type_to_string...\n");

  CHECK(strcmp(ethervox_plugin_type_to_string(ETHERVOX_PLUGIN_LLM), "llm") == 0);
  CHECK(strcmp(ethervox_plugin_type_to_string(ETHERVOX_PLUGIN_STT), "stt") == 0);
  CHECK(strcmp(ethervox_plugin_type_to_string(ETHERVOX_PLUGIN_TTS), "tts") == 0);
  CHECK(strcmp(ethervox_plugin_type_to_string(ETHERVOX_PLUGIN_INTENT), "intent") == 0);
  CHECK(strcmp(ethervox_plugin_type_to_string(ETHERVOX_PLUGIN_ENTITY), "entity") == 0);
  CHECK(strcmp(ethervox_plugin_type_to_string(ETHERVOX_PLUGIN_MIDDLEWARE), "middleware") == 0);

  // Test unknown type
  CHECK(strcmp(ethervox_plugin_type_to_string((ethervox_plugin_type_t)999), "unknown") == 0);

  printf("✓ Plugin type to string conversion test passed\n");
}

void test_plugin_status_to_string() {
  printf("Testing ethervox_plugin_status_to_string...\n");

  CHECK(strcmp(ethervox_plugin_status_to_string(ETHERVOX_PLUGIN_STATUS_UNLOADED), "unloaded") ==
         0);
  CHECK(strcmp(ethervox_plugin_status_to_string(ETHERVOX_PLUGIN_STATUS_LOADED), "loaded") == 0);
  CHECK(strcmp(ethervox_plugin_status_to_string(ETHERVOX_PLUGIN_STATUS_ACTIVE), "active") == 0);
  CHECK(strcmp(ethervox_plugin_status_to_string(ETHERVOX_PLUGIN_STATUS_ERROR), "error") == 0);
  CHECK(strcmp(ethervox_plugin_status_to_string(ETHERVOX_PLUGIN_STATUS_DISABLED), "disabled") ==
         0);

  printf("✓ Plugin status to string conversion test passed\n");
}

void test_plugin_manager_init() {
  printf("Testing plugin manager initialization...\n");

  ethervox_plugin_manager_t manager;

  // Initialize with default plugin directory
  CHECK(ethervox_plugin_manager_init(&manager, NULL) == 0);
  CHECK(manager.plugin_count == ETHERVOX_BUILTIN_PLUGIN_COUNT);
  CHECK(manager.max_plugins == ETHERVOX_MAX_PLUGINS);

  // Cleanup
  ethervox_plugin_manager_cleanup(&manager);

  // Initialize with custom plugin directory
  CHECK(ethervox_plugin_manager_init(&manager, "./test_plugins") == 0);
  ethervox_plugin_manager_cleanup(&manager);

  printf("✓ Plugin manager initialization test passed\n");
}

void test_plugin_manager_null_handling() {
  printf("Testing plugin manager null pointer handling...\n");

  // Test NULL manager
  CHECK(ethervox_plugin_manager_init(NULL, NULL) == ETHERVOX_ERROR_INVALID_ARGUMENT);
  CHECK(ethervox_plugin_manager_init(NULL, "./plugins") == ETHERVOX_ERROR_INVALID_ARGUMENT);

  // Test NULL plugin directory (should be allowed - uses default)
  ethervox_plugin_manager_t manager;
  CHECK(ethervox_plugin_manager_init(&manager, NULL) == 0);
  ethervox_plugin_manager_cleanup(&manager);

  printf("✓ Plugin manager null pointer handling test passed\n");
}

void test_plugin_enums_ranges() {
  printf("Testing plugin enum value ranges...\n");

  // Test that enum values are in expected ranges
  CHECK(ETHERVOX_PLUGIN_LLM >= 0);
  CHECK(ETHERVOX_PLUGIN_MIDDLEWARE < 10);  // Reasonable upper bound

  CHECK(ETHERVOX_PLUGIN_STATUS_UNLOADED >= 0);
  CHECK(ETHERVOX_PLUGIN_STATUS_DISABLED < 10);  // Reasonable upper bound

  printf("✓ Plugin enum ranges test passed\n");
}

int main() {
  printf("Running EthervoxAI Plugin Manager Unit Tests\n");
  printf("==========================================\n");

  test_plugin_type_to_string();
  test_plugin_status_to_string();
  test_plugin_manager_init();
  test_plugin_manager_null_handling();
  test_plugin_enums_ranges();

  printf("==========================================\n");
  printf("All plugin manager tests completed!\n");

  return ETHERVOX_SUCCESS;
}