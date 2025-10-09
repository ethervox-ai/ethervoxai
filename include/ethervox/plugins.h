/**
 * @file plugins.h
 * @brief Plugin system interface definitions for EthervoxAI
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
#ifndef ETHERVOX_PLUGINS_H
#define ETHERVOX_PLUGINS_H

//#ifdef ETHERVOX_PLATFORM_RPI

#include "ethervox/config.h"
#include "ethervox/dialogue.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ETHERVOX_MAX_PLUGINS
#define ETHERVOX_MAX_PLUGINS 32
#endif

// Plugin types
typedef enum {
    ETHERVOX_PLUGIN_LLM = 0,
    ETHERVOX_PLUGIN_STT,
    ETHERVOX_PLUGIN_TTS,
    ETHERVOX_PLUGIN_INTENT,
    ETHERVOX_PLUGIN_ENTITY,
    ETHERVOX_PLUGIN_MIDDLEWARE,
    ETHERVOX_PLUGIN_COUNT
} ethervox_plugin_type_t;

// Plugin status
typedef enum {
    ETHERVOX_PLUGIN_STATUS_UNLOADED = 0,
    ETHERVOX_PLUGIN_STATUS_LOADED,
    ETHERVOX_PLUGIN_STATUS_ACTIVE,
    ETHERVOX_PLUGIN_STATUS_ERROR,
    ETHERVOX_PLUGIN_STATUS_DISABLED
} ethervox_plugin_status_t;

// Plugin metadata structure
typedef struct {
    char name[64];
    char version[16];
    char author[64];
    char description[256];
    ethervox_plugin_type_t type;
} ethervox_plugin_metadata_t;

// Plugin configuration
typedef struct {
    char* config_json;
    bool enabled;
    int priority;
    char* api_key;
    char* endpoint_url;
    uint32_t timeout_ms;
    uint32_t max_retries;
    void* custom_config;
} ethervox_plugin_config_t;

// Forward declaration
typedef struct ethervox_plugin ethervox_plugin_t;
typedef struct ethervox_plugin_manager ethervox_plugin_manager_t;

// Plugin interface structure
typedef struct {
    int (*init)(struct ethervox_plugin* plugin);
    int (*process)(struct ethervox_plugin* plugin, const void* input, void* output);
    void (*cleanup)(struct ethervox_plugin* plugin);
} ethervox_plugin_interface_t;

// Plugin structure
typedef struct ethervox_plugin {
    char name[64];
    char version[16];
    char description[256];
    ethervox_plugin_type_t type;
    ethervox_plugin_status_t status;
    void* handle;  // Dynamic library handle
    int (*execute)(const void* input, void* output);
    void* user_data;
    ethervox_plugin_metadata_t metadata;
    ethervox_plugin_interface_t interface;
    time_t load_time;
    time_t last_used;
    uint32_t usage_count;
} ethervox_plugin_t;


// External LLM plugin specifics
typedef struct {
    char* prompt;
    char* context;
    char language_code[8];
    float temperature;
    uint32_t max_tokens;
    char** stop_sequences;
    uint32_t stop_count;
} ethervox_llm_request_t;

// Plugin manager structure
typedef struct ethervox_plugin_manager {
    ethervox_plugin_t plugins[ETHERVOX_MAX_PLUGINS];
    uint32_t plugin_count;
    char plugin_directory[512];
    char config_file[512];
    bool initialized;
    uint32_t max_plugins;
    bool is_initialized;
    uint32_t loaded_plugins;
} ethervox_plugin_manager_t;

// Public API functions
int ethervox_plugin_manager_init(ethervox_plugin_manager_t* manager, const char* plugin_dir);
void ethervox_plugin_manager_cleanup(ethervox_plugin_manager_t* manager);

// Plugin management
int ethervox_plugin_load(ethervox_plugin_manager_t* manager, const char* plugin_name);
int ethervox_plugin_unload(ethervox_plugin_manager_t* manager, const char* plugin_name);
int ethervox_plugin_reload(ethervox_plugin_manager_t* manager, const char* plugin_name);
ethervox_plugin_t* ethervox_plugin_find(ethervox_plugin_manager_t* manager, const char* plugin_name);

// Plugin discovery
int ethervox_plugin_scan_directory(ethervox_plugin_manager_t* manager);
int ethervox_plugin_list_available(ethervox_plugin_manager_t* manager, char*** plugin_names, uint32_t* count);
int ethervox_plugin_list_loaded(ethervox_plugin_manager_t* manager, char*** plugin_names, uint32_t* count);

// Plugin execution
int ethervox_plugin_execute(ethervox_plugin_t* plugin, const void* input, void* output);
int ethervox_plugin_configure(ethervox_plugin_t* plugin, const char* config_json);

// External LLM integrations
int ethervox_llm_plugin_openai(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                               void* user_data);

int ethervox_llm_plugin_huggingface(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                                   void* user_data);

int ethervox_llm_plugin_local_rag(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                                 void* user_data);

// Built-in plugins
int ethervox_plugin_register_builtin_openai(ethervox_plugin_manager_t* manager);
int ethervox_plugin_register_builtin_huggingface(ethervox_plugin_manager_t* manager);  // Just declaration, no implementation
int ethervox_plugin_register_builtin_local_rag(ethervox_plugin_manager_t* manager);

// Utility functions
const char* ethervox_plugin_type_to_string(ethervox_plugin_type_t type);
const char* ethervox_plugin_status_to_string(ethervox_plugin_status_t status);
void ethervox_llm_request_free(ethervox_llm_request_t* request);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PLUGINS_H