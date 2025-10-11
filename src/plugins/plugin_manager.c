/**
 * @file plugin_manager.c
 * @brief Plugin management system for EthervoxAI
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
#include "ethervox/plugins.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Dynamic library loading not supported on ESP32
#if !defined(ETHERVOX_PLATFORM_ESP32) && !defined(PLATFORM_EMBEDDED)
    #ifdef _WIN32
        #include <windows.h>
    #else
        #include <dlfcn.h>
    #endif
#endif

// Plugin type to string mapping
const char* ethervox_plugin_type_to_string(ethervox_plugin_type_t type) {
    switch (type) {
        case ETHERVOX_PLUGIN_LLM: return "llm";
        case ETHERVOX_PLUGIN_STT: return "stt";
        case ETHERVOX_PLUGIN_TTS: return "tts";
        case ETHERVOX_PLUGIN_INTENT: return "intent";
        case ETHERVOX_PLUGIN_ENTITY: return "entity";
        case ETHERVOX_PLUGIN_MIDDLEWARE: return "middleware";
        default: return "unknown";
    }
}

// Plugin status to string mapping
const char* ethervox_plugin_status_to_string(ethervox_plugin_status_t status) {
    switch (status) {
        case ETHERVOX_PLUGIN_STATUS_UNLOADED: return "unloaded";
        case ETHERVOX_PLUGIN_STATUS_LOADED: return "loaded";
        case ETHERVOX_PLUGIN_STATUS_ACTIVE: return "active";
        case ETHERVOX_PLUGIN_STATUS_ERROR: return "error";
        case ETHERVOX_PLUGIN_STATUS_DISABLED: return "disabled";
        default: return "unknown";
    }
}

// Initialize plugin manager
int ethervox_plugin_manager_init(ethervox_plugin_manager_t* manager, const char* plugin_dir) {
    if (!manager) return -1;

    memset(manager, 0, sizeof(ethervox_plugin_manager_t));

    const char* resolved_plugin_dir = plugin_dir ? plugin_dir : "./plugins";

    // Set plugin directory
    strncpy(manager->plugin_directory, resolved_plugin_dir, sizeof(manager->plugin_directory) - 1);
    manager->plugin_directory[sizeof(manager->plugin_directory) - 1] = '\0';
    
    // Build config file path with safety check
    int written = snprintf(manager->config_file, sizeof(manager->config_file),
                          "%s/plugins.conf", manager->plugin_directory);
    
    // Check if path was truncated
    if (written < 0 || written >= (int)sizeof(manager->config_file)) {
        fprintf(stderr, "Error: Plugin config path too long or invalid (needs %d bytes, have %zu)\n",
                written, sizeof(manager->config_file));
        return -1;
    }
    
    // Set max plugins
    manager->max_plugins = ETHERVOX_MAX_PLUGINS;
    manager->is_initialized = true;
    
    printf("Plugin manager initialized with directory: %s\n", manager->plugin_directory);
    
    // Register built-in plugins
    ethervox_plugin_register_builtin_openai(manager);
    ethervox_plugin_register_builtin_huggingface(manager);
    ethervox_plugin_register_builtin_local_rag(manager);
    
    return 0;
}

// Cleanup plugin manager
void ethervox_plugin_manager_cleanup(ethervox_plugin_manager_t* manager) {
    if (!manager) return;
    
    // Unload all plugins
    for (size_t i = 0; i < manager->plugin_count; i++) {
        ethervox_plugin_t* plugin = &manager->plugins[i];
        if (plugin->handle) {
#if !defined(ETHERVOX_PLATFORM_ESP32) && !defined(PLATFORM_EMBEDDED)
    #ifdef _WIN32
            FreeLibrary((HMODULE)plugin->handle);
    #else
            dlclose(plugin->handle);
    #endif
#endif
            plugin->handle = NULL;
        }
    }
    
    manager->plugin_count = 0;
    manager->is_initialized = false;
}

// Find plugin by name
ethervox_plugin_t* ethervox_plugin_find(ethervox_plugin_manager_t* manager, const char* plugin_name) {
    if (!manager || !plugin_name) return NULL;
    
    for (uint32_t i = 0; i < manager->max_plugins; i++) {
        ethervox_plugin_t* plugin = &manager->plugins[i];
        if (plugin->status != ETHERVOX_PLUGIN_STATUS_UNLOADED &&
            strcmp(plugin->metadata.name, plugin_name) == 0) {
            return plugin;
        }
    }
    
    return NULL;
}

// Load plugin (placeholder implementation)
int ethervox_plugin_load(ethervox_plugin_manager_t* manager, const char* plugin_name) {
    if (!manager || !plugin_name) return -1;
    
    // Find empty slot
    for (uint32_t i = 0; i < manager->max_plugins; i++) {
        ethervox_plugin_t* plugin = &manager->plugins[i];
        if (plugin->status == ETHERVOX_PLUGIN_STATUS_UNLOADED) {
            
            // Initialize plugin metadata (simplified)
            strncpy(plugin->metadata.name, plugin_name, sizeof(plugin->metadata.name) - 1);
            strcpy(plugin->metadata.version, "1.0.0");
            strcpy(plugin->metadata.author, "EthervoxAI Team");
            snprintf(plugin->metadata.description, sizeof(plugin->metadata.description),
                    "Built-in %s plugin", plugin_name);
            
            // Set plugin type based on name
            if (strstr(plugin_name, "openai") || strstr(plugin_name, "huggingface") || strstr(plugin_name, "rag")) {
                plugin->metadata.type = ETHERVOX_PLUGIN_LLM;
            } else {
                plugin->metadata.type = ETHERVOX_PLUGIN_MIDDLEWARE;
            }
            
            plugin->status = ETHERVOX_PLUGIN_STATUS_LOADED;
            plugin->load_time = time(NULL);
            manager->loaded_plugins++;
            
            printf("Plugin loaded: %s\n", plugin_name);
            return 0;
        }
    }
    
    return -1;  // No available slots
}

// Execute plugin

// Wrapper functions for type-safe plugin execution
static int openai_execute_wrapper(const void* input, void* output) {
    return ethervox_llm_plugin_openai(
        (const ethervox_llm_request_t*)input,
        (ethervox_llm_response_t*)output,
        NULL
    );
}

static int huggingface_execute_wrapper(const void* input, void* output) {
    return ethervox_llm_plugin_huggingface(
        (const ethervox_llm_request_t*)input,
        (ethervox_llm_response_t*)output,
        NULL
    );
}

static int local_rag_execute_wrapper(const void* input, void* output) {
    return ethervox_llm_plugin_local_rag(
        (const ethervox_llm_request_t*)input,
        (ethervox_llm_response_t*)output,
        NULL
    );
}

int ethervox_plugin_execute(ethervox_plugin_t* plugin, const void* input, void* output) {
    if (!plugin || plugin->status != ETHERVOX_PLUGIN_STATUS_LOADED) {
        return -1;
    }
    
    plugin->last_used = time(NULL);
    plugin->usage_count++;

    if (plugin->plugin_interface.process) {
        return plugin->plugin_interface.process(plugin, input, output);
    }

    if (plugin->execute) {
        return plugin->execute(input, output);
    }

    return -1;
}

// OpenAI plugin implementation (keep only ONE definition)
int ethervox_llm_plugin_openai(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                               void* user_data) {
    if (!request || !response) return -1;
    
    // Simulate OpenAI API call
    response->text = (char*)malloc(1024);
    snprintf(response->text, 1024, "OpenAI response to: %s", request->prompt);
    strncpy(response->language_code, request->language_code, 7);
    response->confidence = 0.95f;
    response->processing_time_ms = 100;
    response->token_count = 50;
    response->tokens_used = 50;
    response->model_name = strdup("gpt-3.5-turbo");
    response->requires_external_llm = false;
    response->external_llm_prompt = NULL;
    response->truncated = false;
    response->finish_reason = strdup("stop");
    
    return 0;
}

// HuggingFace plugin implementation
int ethervox_llm_plugin_huggingface(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                                   void* user_data) {
    if (!request || !response) return -1;
    
    // user_data can contain model name if needed
    const char* model_name = (user_data && strlen((char*)user_data) > 0) ? (char*)user_data : "gpt2";
    
    // Simulate HuggingFace inference
    response->text = (char*)malloc(1024);
    snprintf(response->text, 1024, "HuggingFace (%s) response to: %s", model_name, request->prompt);
    strncpy(response->language_code, request->language_code, 7);
    response->confidence = 0.90f;
    response->processing_time_ms = 150;
    response->token_count = 45;
    response->tokens_used = 45;
    response->model_name = strdup(model_name);
    response->requires_external_llm = false;
    response->external_llm_prompt = NULL;
    response->truncated = false;
    response->finish_reason = strdup("stop");
    
    return 0;
}

// Local RAG plugin implementation
int ethervox_llm_plugin_local_rag(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                                 void* user_data) {
    if (!request || !response) return -1;
    
    // Simulate local RAG processing
    response->text = (char*)malloc(1024);
    snprintf(response->text, 1024, "Local RAG response to: %s", request->prompt);
    strncpy(response->language_code, request->language_code, 7);
    response->confidence = 0.85f;
    response->processing_time_ms = 80;
    response->token_count = 60;
    response->tokens_used = 60;
    response->model_name = strdup("local-rag");
    response->requires_external_llm = false;
    response->external_llm_prompt = NULL;
    response->truncated = false;
    response->finish_reason = strdup("rag_complete");
    
    return 0;
}

// Register built-in OpenAI plugin
int ethervox_plugin_register_builtin_openai(ethervox_plugin_manager_t* manager) {
    if (!manager || manager->plugin_count >= ETHERVOX_MAX_PLUGINS) return -1;
    
    ethervox_plugin_t* plugin = &manager->plugins[manager->plugin_count];
    
    strncpy(plugin->name, "openai", sizeof(plugin->name) - 1);
    strncpy(plugin->version, "1.0.0", sizeof(plugin->version) - 1);
    plugin->type = ETHERVOX_PLUGIN_LLM;
    plugin->status = ETHERVOX_PLUGIN_STATUS_LOADED;
    plugin->execute = openai_execute_wrapper;  // Use wrapper instead of direct assignment
    plugin->user_data = NULL;
    
    manager->plugin_count++;
    return 0;
}

// Register built-in HuggingFace plugin
int ethervox_plugin_register_builtin_huggingface(ethervox_plugin_manager_t* manager) {
    if (!manager || manager->plugin_count >= ETHERVOX_MAX_PLUGINS) return -1;
    
    ethervox_plugin_t* plugin = &manager->plugins[manager->plugin_count];
    
    strncpy(plugin->name, "huggingface", sizeof(plugin->name) - 1);
    strncpy(plugin->version, "1.0.0", sizeof(plugin->version) - 1);
    plugin->type = ETHERVOX_PLUGIN_LLM;
    plugin->status = ETHERVOX_PLUGIN_STATUS_LOADED;
    plugin->execute = huggingface_execute_wrapper;  // Use wrapper
    plugin->user_data = NULL;
    
    manager->plugin_count++;
    return 0;
}

// Register built-in Local RAG plugin
int ethervox_plugin_register_builtin_local_rag(ethervox_plugin_manager_t* manager) {
    if (!manager || manager->plugin_count >= ETHERVOX_MAX_PLUGINS) return -1;
    
    ethervox_plugin_t* plugin = &manager->plugins[manager->plugin_count];
    
    strncpy(plugin->name, "local_rag", sizeof(plugin->name) - 1);
    strncpy(plugin->version, "1.0.0", sizeof(plugin->version) - 1);
    plugin->type = ETHERVOX_PLUGIN_LLM;
    plugin->status = ETHERVOX_PLUGIN_STATUS_LOADED;
    plugin->execute = local_rag_execute_wrapper;  // Use wrapper
    plugin->user_data = NULL;
    
    manager->plugin_count++;
    return 0;
}

// Free LLM request
void ethervox_llm_request_free(ethervox_llm_request_t* request) {
    if (!request) return;
    
    if (request->prompt) {
        free(request->prompt);
        request->prompt = NULL;
    }
    if (request->context) {
        free(request->context);
        request->context = NULL;
    }
    if (request->stop_sequences) {
        for (uint32_t i = 0; i < request->stop_count; i++) {
            if (request->stop_sequences[i]) {
                free(request->stop_sequences[i]);
            }
        }
        free(request->stop_sequences);
        request->stop_sequences = NULL;
    }
}

