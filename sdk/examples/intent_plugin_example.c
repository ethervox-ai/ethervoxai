/**
 * @file intent_plugin_example.c
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

#include "../ethervox_sdk.h"

// Example Intent Plugin: Smart Home Controller
// This plugin recognizes smart home commands like "turn on lights", "set temperature", etc.

typedef struct {
  char device_name[64];
  char action[32];
  float value;
  char room[32];
} smart_home_command_t;

// Plugin user data
typedef struct {
  uint32_t command_count;
  smart_home_command_t last_command;
} smart_home_plugin_data_t;

// Helper function to extract entities from text
static int extract_smart_home_entities(const char* text, smart_home_command_t* command) {
  memset(command, 0, sizeof(smart_home_command_t));

  // Simple keyword matching (in production, use NLP libraries)
  char text_lower[1024];
  strncpy(text_lower, text, sizeof(text_lower) - 1);

  // Convert to lowercase for easier matching
  for (int i = 0; text_lower[i]; i++) {
    if (text_lower[i] >= 'A' && text_lower[i] <= 'Z') {
      text_lower[i] += 32;
    }
  }

  // Device detection
  if (strstr(text_lower, "light") || strstr(text_lower, "lamp")) {
    strcpy(command->device_name, "light");
  } else if (strstr(text_lower, "thermostat") || strstr(text_lower, "temperature")) {
    strcpy(command->device_name, "thermostat");
  } else if (strstr(text_lower, "fan")) {
    strcpy(command->device_name, "fan");
  } else if (strstr(text_lower, "tv") || strstr(text_lower, "television")) {
    strcpy(command->device_name, "tv");
  } else {
    return -1;  // Unknown device
  }

  // Action detection
  if (strstr(text_lower, "turn on") || strstr(text_lower, "switch on") ||
      strstr(text_lower, "enable")) {
    strcpy(command->action, "turn_on");
  } else if (strstr(text_lower, "turn off") || strstr(text_lower, "switch off") ||
             strstr(text_lower, "disable")) {
    strcpy(command->action, "turn_off");
  } else if (strstr(text_lower, "dim") || strstr(text_lower, "lower")) {
    strcpy(command->action, "dim");
  } else if (strstr(text_lower, "brighten") || strstr(text_lower, "increase")) {
    strcpy(command->action, "brighten");
  } else if (strstr(text_lower, "set")) {
    strcpy(command->action, "set");
  } else {
    return -1;  // Unknown action
  }

  // Room detection
  if (strstr(text_lower, "living room")) {
    strcpy(command->room, "living_room");
  } else if (strstr(text_lower, "bedroom")) {
    strcpy(command->room, "bedroom");
  } else if (strstr(text_lower, "kitchen")) {
    strcpy(command->room, "kitchen");
  } else if (strstr(text_lower, "bathroom")) {
    strcpy(command->room, "bathroom");
  } else {
    strcpy(command->room, "all");
  }

  // Value extraction for temperature/brightness
  char* ptr = strstr(text_lower, "to ");
  if (ptr) {
    ptr += 3;  // Skip "to "
    command->value = strtof(ptr, NULL);
  }

  return 0;
}

// Intent parsing function
static int smart_home_parse_intent(const ethervox_stt_input_t* input,
                                   ethervox_intent_result_t* result, void* user_data) {
  smart_home_plugin_data_t* data = (smart_home_plugin_data_t*)user_data;

  // Extract smart home command
  smart_home_command_t command;
  if (extract_smart_home_entities(input->text, &command) != 0) {
    return -1;  // Not a smart home command
  }

  // Set intent result
  result->type = ETHERVOX_INTENT_COMMAND;
  result->confidence = 0.85f;  // Fixed confidence for this example

  // Create JSON entities string
  snprintf(result->entities, sizeof(result->entities),
           "{\"device\":\"%s\",\"action\":\"%s\",\"room\":\"%s\",\"value\":%.1f}",
           command.device_name, command.action, command.room, command.value);

  snprintf(result->context, sizeof(result->context), "Smart home control command");

  // Update plugin data
  data->command_count++;
  data->last_command = command;

  printf("Smart Home Command: %s %s in %s (value: %.1f)\n", command.action, command.device_name,
         command.room, command.value);

  return 0;  // Successfully parsed
}

// Plugin cleanup function
static void smart_home_cleanup(void* user_data) {
  if (user_data) {
    smart_home_plugin_data_t* data = (smart_home_plugin_data_t*)user_data;
    printf("Smart Home Plugin processed %d commands\n", data->command_count);
    free(data);
  }
}

// Create smart home intent plugin
ethervox_intent_plugin_t* create_smart_home_plugin(void) {
  ethervox_intent_plugin_t* plugin =
      (ethervox_intent_plugin_t*)calloc(1, sizeof(ethervox_intent_plugin_t));
  if (!plugin)
    return NULL;

  // Plugin metadata
  strcpy(plugin->name, "SmartHomeController");
  strcpy(plugin->version, "1.0.0");
  strcpy(plugin->description, "Recognizes smart home device control commands");

  // Supported languages
  plugin->supported_languages_count = 2;
  strcpy(plugin->supported_languages[0], "en");
  strcpy(plugin->supported_languages[1], "es");

  // Plugin functions
  plugin->parse = smart_home_parse_intent;
  plugin->cleanup = smart_home_cleanup;

  // Create user data
  smart_home_plugin_data_t* data =
      (smart_home_plugin_data_t*)calloc(1, sizeof(smart_home_plugin_data_t));
  plugin->user_data = data;

  return plugin;
}

// Example usage
int main() {
  printf("=== EtherVox SDK Intent Plugin Example ===\n\n");

  // Initialize SDK
  ethervox_sdk_t sdk;
  if (ethervox_sdk_init(&sdk) != 0) {
    printf("Failed to initialize SDK\n");
    return 1;
  }

  // Create and register smart home plugin
  ethervox_intent_plugin_t* smart_home_plugin = create_smart_home_plugin();
  if (!smart_home_plugin) {
    printf("Failed to create smart home plugin\n");
    ethervox_sdk_cleanup(&sdk);
    return 1;
  }

  if (ethervox_sdk_register_intent_plugin(&sdk, smart_home_plugin) != 0) {
    printf("Failed to register smart home plugin\n");
    free(smart_home_plugin);
    ethervox_sdk_cleanup(&sdk);
    return 1;
  }

  // Test intent parsing
  printf("Testing intent parsing...\n\n");

  const char* test_phrases[] = {"turn on the lights in the living room",
                                "set the thermostat to 72 degrees",
                                "dim the bedroom lights",
                                "turn off the kitchen fan",
                                "what's the weather like today",  // Should not be recognized
                                "switch on the tv"};

  for (int i = 0; i < 6; i++) {
    printf("Input: \"%s\"\n", test_phrases[i]);

    ethervox_stt_input_t input = {0};
    strncpy(input.text, test_phrases[i], sizeof(input.text) - 1);
    strcpy(input.language, "en");
    input.audio_confidence = 0.95f;
    input.processing_time_ms = 150;

    ethervox_intent_result_t result;
    int ret = ethervox_sdk_process_intent(&sdk, &input, &result);

    if (ret == 0) {
      printf("  Intent: %s (confidence: %.2f)\n", ethervox_intent_type_to_string(result.type),
             result.confidence);
      printf("  Entities: %s\n", result.entities);
      printf("  Context: %s\n", result.context);
    } else {
      printf("  No intent recognized\n");
    }
    printf("\n");
  }

  // Print plugin statistics
  printf("Plugin Statistics:\n");
  printf("  Total requests: %llu\n", smart_home_plugin->total_requests);
  printf("  Successful requests: %llu\n", smart_home_plugin->successful_requests);
  printf("  Success rate: %.1f%%\n",
         smart_home_plugin->total_requests > 0
             ? (100.0f * smart_home_plugin->successful_requests / smart_home_plugin->total_requests)
             : 0.0f);
  printf("  Average processing time: %.2f ms\n", smart_home_plugin->average_processing_time_ms);

  // Cleanup
  ethervox_sdk_cleanup(&sdk);
  free(smart_home_plugin);

  return 0;
}