#include "ethervox/plugins.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef ETHERVOX_PLATFORM_WINDOWS
    #include <windows.h>
    #define PLUGIN_EXTENSION ".dll"
#else
    #include <dlfcn.h>
    #define PLUGIN_EXTENSION ".so"
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
    
    // Set plugin directory
    if (plugin_dir) {
        strncpy(manager->plugin_directory, plugin_dir, sizeof(manager->plugin_directory) - 1);
    } else {
        #ifdef ETHERVOX_PLATFORM_WINDOWS
            strcpy(manager->plugin_directory, ".\\plugins");
        #else
            strcpy(manager->plugin_directory, "./plugins");
        #endif
    }
    
    // Set config file path
    snprintf(manager->config_file, sizeof(manager->config_file), 
             "%s/plugins.conf", manager->plugin_directory);
    
    // Allocate plugin array
    manager->max_plugins = ETHERVOX_MAX_PLUGINS;
    manager->plugins = (ethervox_plugin_t*)calloc(manager->max_plugins, sizeof(ethervox_plugin_t));
    
    if (!manager->plugins) {
        return -1;
    }
    
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
    if (!manager || !manager->is_initialized) return;
    
    // Unload all plugins
    for (uint32_t i = 0; i < manager->max_plugins; i++) {
        ethervox_plugin_t* plugin = &manager->plugins[i];
        if (plugin->status != ETHERVOX_PLUGIN_STATUS_UNLOADED) {
            if (plugin->interface.cleanup) {
                plugin->interface.cleanup(plugin);
            }
            
            // Close dynamic library
            if (plugin->handle) {
                #ifdef ETHERVOX_PLATFORM_WINDOWS
                    FreeLibrary((HMODULE)plugin->handle);
                #else
                    dlclose(plugin->handle);
                #endif
            }
        }
    }
    
    if (manager->plugins) {
        free(manager->plugins);
        manager->plugins = NULL;
    }
    
    manager->is_initialized = false;
    printf("Plugin manager cleaned up\n");
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
int ethervox_plugin_execute(ethervox_plugin_t* plugin, const void* input, void* output) {
    if (!plugin || plugin->status != ETHERVOX_PLUGIN_STATUS_LOADED) {
        return -1;
    }
    
    plugin->last_used = time(NULL);
    plugin->usage_count++;
    
    if (plugin->interface.process) {
        return plugin->interface.process(plugin, input, output);
    }
    
    return -1;
}

// OpenAI plugin implementation
int ethervox_llm_plugin_openai(const ethervox_llm_request_t* request, ethervox_llm_response_t* response, 
                               const char* api_key) {
    if (!request || !response || !api_key) {
        return -1;
    }
    
    // Placeholder implementation
    // In production, this would make HTTP requests to OpenAI API
    memset(response, 0, sizeof(ethervox_llm_response_t));
    
    response->text = strdup("This is a placeholder response from OpenAI plugin. "
                           "To use this plugin, provide a valid API key and implement HTTP client.");
    response->model_name = strdup("gpt-3.5-turbo");
    response->token_count = 20;
    response->processing_time_ms = 500;
    response->confidence = 0.95f;
    response->truncated = false;
    response->finish_reason = strdup("stop");
    
    printf("OpenAI plugin called (placeholder)\n");
    return 0;
}

// HuggingFace plugin implementation
int ethervox_llm_plugin_huggingface(const ethervox_llm_request_t* request, ethervox_llm_response_t* response, 
                                   const char* model_name, const char* api_key) {
    if (!request || !response || !model_name) {
        return -1;
    }
    
    // Placeholder implementation
    memset(response, 0, sizeof(ethervox_llm_response_t));
    
    response->text = strdup("This is a placeholder response from HuggingFace plugin. "
                           "Implement HTTP client to connect to HuggingFace Inference API.");
    response->model_name = strdup(model_name);
    response->token_count = 18;
    response->processing_time_ms = 750;
    response->confidence = 0.88f;
    response->truncated = false;
    response->finish_reason = strdup("stop");
    
    printf("HuggingFace plugin called with model: %s (placeholder)\n", model_name);
    return 0;
}

// Local RAG plugin implementation
int ethervox_llm_plugin_local_rag(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                                 const char* index_path) {
    if (!request || !response || !index_path) {
        return -1;
    }
    
    // Placeholder implementation
    memset(response, 0, sizeof(ethervox_llm_response_t));
    
    response->text = strdup("This is a placeholder response from Local RAG plugin. "
                           "Implement vector database integration for local knowledge retrieval.");
    response->model_name = strdup("local-rag");
    response->token_count = 22;
    response->processing_time_ms = 200;  // Faster as it's local
    response->confidence = 0.92f;
    response->truncated = false;
    response->finish_reason = strdup("rag_complete");
    
    printf("Local RAG plugin called with index: %s (placeholder)\n", index_path);
    return 0;
}

// Register built-in OpenAI plugin
int ethervox_plugin_register_builtin_openai(ethervox_plugin_manager_t* manager) {
    return ethervox_plugin_load(manager, "builtin_openai");
}

// Register built-in HuggingFace plugin
int ethervox_plugin_register_builtin_huggingface(ethervox_plugin_manager_t* manager) {
    return ethervox_plugin_load(manager, "builtin_huggingface");
}

// Register built-in Local RAG plugin
int ethervox_plugin_register_builtin_local_rag(ethervox_plugin_manager_t* manager) {
    return ethervox_plugin_load(manager, "builtin_local_rag");
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

// Free LLM response
void ethervox_llm_response_free(ethervox_llm_response_t* response) {
    if (!response) return;
    
    if (response->text) {
        free(response->text);
        response->text = NULL;
    }
    if (response->model_name) {
        free(response->model_name);
        response->model_name = NULL;
    }
    if (response->finish_reason) {
        free(response->finish_reason);
        response->finish_reason = NULL;
    }
}