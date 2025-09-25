#include "ethervox_sdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// Global SDK instance for single-instance usage
static ethervox_sdk_t* g_sdk_instance = NULL;

// Version string generation
static char version_string[32] = {0};

const char* ethervox_sdk_get_version_string(void) {
    if (version_string[0] == 0) {
        snprintf(version_string, sizeof(version_string), "%d.%d.%d",
                ETHERVOX_SDK_VERSION_MAJOR, 
                ETHERVOX_SDK_VERSION_MINOR, 
                ETHERVOX_SDK_VERSION_PATCH);
    }
    return version_string;
}

bool ethervox_sdk_is_compatible(uint32_t major, uint32_t minor) {
    // Major version must match, minor version must be <= current
    return (major == ETHERVOX_SDK_VERSION_MAJOR) && 
           (minor <= ETHERVOX_SDK_VERSION_MINOR);
}

// Initialize SDK
int ethervox_sdk_init(ethervox_sdk_t* sdk) {
    if (!sdk) return -1;
    
    memset(sdk, 0, sizeof(ethervox_sdk_t));
    
    sdk->version_major = ETHERVOX_SDK_VERSION_MAJOR;
    sdk->version_minor = ETHERVOX_SDK_VERSION_MINOR;
    sdk->version_patch = ETHERVOX_SDK_VERSION_PATCH;
    
    // Initialize diagnostics
    sdk->diagnostics = (ethervox_diagnostics_t*)calloc(1, sizeof(ethervox_diagnostics_t));
    if (!sdk->diagnostics) {
        strcpy(sdk->last_error, "Failed to allocate diagnostics");
        return -1;
    }
    
    sdk->diagnostics->min_log_level = ETHERVOX_LOG_INFO;
    sdk->diagnostics->enable_performance_tracking = true;
    sdk->diagnostics->enable_memory_profiling = false;
    sdk->diagnostics->metrics_update_interval_ms = 1000;  // 1 second
    
    // Initialize device profile
    sdk->device_profile = (ethervox_device_profile_t*)calloc(1, sizeof(ethervox_device_profile_t));
    if (!sdk->device_profile) {
        free(sdk->diagnostics);
        strcpy(sdk->last_error, "Failed to allocate device profile");
        return -1;
    }
    
    // Set default device profile
    strcpy(sdk->device_profile->name, "Default");
    strcpy(sdk->device_profile->platform, "Generic");
    sdk->device_profile->sample_rate = 16000;
    sdk->device_profile->bit_depth = 16;
    sdk->device_profile->mic_array_channels = 1;
    
    sdk->initialization_time = time(NULL);
    sdk->is_initialized = true;
    
    // Set global instance
    g_sdk_instance = sdk;
    
    printf("EtherVox SDK v%s initialized\n", ethervox_sdk_get_version_string());
    
    return 0;
}

// Cleanup SDK
void ethervox_sdk_cleanup(ethervox_sdk_t* sdk) {
    if (!sdk || !sdk->is_initialized) return;
    
    // Cleanup intent plugins
    for (uint32_t i = 0; i < sdk->intent_plugin_count; i++) {
        if (sdk->intent_plugins[i] && sdk->intent_plugins[i]->cleanup) {
            sdk->intent_plugins[i]->cleanup(sdk->intent_plugins[i]->user_data);
        }
    }
    
    // Cleanup diagnostics
    if (sdk->diagnostics) {
        free(sdk->diagnostics);
        sdk->diagnostics = NULL;
    }
    
    // Cleanup device profile
    if (sdk->device_profile) {
        free(sdk->device_profile);
        sdk->device_profile = NULL;
    }
    
    // Cleanup model router
    if (sdk->model_router) {
        free(sdk->model_router);
        sdk->model_router = NULL;
    }
    
    sdk->is_initialized = false;
    
    if (g_sdk_instance == sdk) {
        g_sdk_instance = NULL;
    }
    
    printf("EtherVox SDK cleaned up\n");
}

// Intent Plugin Management
int ethervox_sdk_register_intent_plugin(ethervox_sdk_t* sdk, ethervox_intent_plugin_t* plugin) {
    if (!sdk || !plugin || sdk->intent_plugin_count >= 16) return -1;
    
    // Check for duplicate plugin names
    for (uint32_t i = 0; i < sdk->intent_plugin_count; i++) {
        if (strcmp(sdk->intent_plugins[i]->name, plugin->name) == 0) {
            snprintf(sdk->last_error, sizeof(sdk->last_error),
                    "Plugin '%s' already registered", plugin->name);
            return -1;
        }
    }
    
    sdk->intent_plugins[sdk->intent_plugin_count] = plugin;
    sdk->intent_plugin_count++;
    
    plugin->is_active = true;
    plugin->total_requests = 0;
    plugin->successful_requests = 0;
    plugin->average_processing_time_ms = 0.0f;
    
    printf("Registered intent plugin: %s v%s\n", plugin->name, plugin->version);
    
    return 0;
}

int ethervox_sdk_unregister_intent_plugin(ethervox_sdk_t* sdk, const char* plugin_name) {
    if (!sdk || !plugin_name) return -1;
    
    for (uint32_t i = 0; i < sdk->intent_plugin_count; i++) {
        if (strcmp(sdk->intent_plugins[i]->name, plugin_name) == 0) {
            // Cleanup plugin
            if (sdk->intent_plugins[i]->cleanup) {
                sdk->intent_plugins[i]->cleanup(sdk->intent_plugins[i]->user_data);
            }
            
            // Shift remaining plugins
            for (uint32_t j = i; j < sdk->intent_plugin_count - 1; j++) {
                sdk->intent_plugins[j] = sdk->intent_plugins[j + 1];
            }
            sdk->intent_plugin_count--;
            
            printf("Unregistered intent plugin: %s\n", plugin_name);
            return 0;
        }
    }
    
    snprintf(sdk->last_error, sizeof(sdk->last_error),
            "Plugin '%s' not found", plugin_name);
    return -1;
}

ethervox_intent_plugin_t* ethervox_sdk_find_intent_plugin(ethervox_sdk_t* sdk, const char* name) {
    if (!sdk || !name) return NULL;
    
    for (uint32_t i = 0; i < sdk->intent_plugin_count; i++) {
        if (strcmp(sdk->intent_plugins[i]->name, name) == 0) {
            return sdk->intent_plugins[i];
        }
    }
    
    return NULL;
}

int ethervox_sdk_process_intent(ethervox_sdk_t* sdk, const ethervox_stt_input_t* input, 
                               ethervox_intent_result_t* result) {
    if (!sdk || !input || !result) return -1;
    
    // Initialize result
    memset(result, 0, sizeof(ethervox_intent_result_t));
    result->type = ETHERVOX_INTENT_UNKNOWN;
    result->confidence = 0.0f;
    result->timestamp = time(NULL);
    strcpy(result->language, input->language);
    
    // Try each intent plugin until one succeeds
    for (uint32_t i = 0; i < sdk->intent_plugin_count; i++) {
        ethervox_intent_plugin_t* plugin = sdk->intent_plugins[i];
        if (!plugin->is_active || !plugin->parse) continue;
        
        // Check language support
        bool language_supported = false;
        for (uint32_t j = 0; j < plugin->supported_languages_count; j++) {
            if (strcmp(plugin->supported_languages[j], input->language) == 0) {
                language_supported = true;
                break;
            }
        }
        
        if (!language_supported) continue;
        
        clock_t start = clock();
        plugin->total_requests++;
        
        int ret = plugin->parse(input, result, plugin->user_data);
        
        clock_t end = clock();
        float processing_time = ((float)(end - start)) / CLOCKS_PER_SEC * 1000.0f;
        
        // Update plugin statistics
        plugin->average_processing_time_ms = 
            (plugin->average_processing_time_ms * (plugin->successful_requests) + processing_time) / 
            (plugin->successful_requests + 1);
        
        if (ret == 0 && result->type != ETHERVOX_INTENT_UNKNOWN) {
            plugin->successful_requests++;
            return 0;  // Successfully parsed intent
        }
    }
    
    // No plugin could parse the intent
    snprintf(sdk->last_error, sizeof(sdk->last_error),
            "No plugin could parse intent for language '%s'", input->language);
    return -1;
}

// Model Router Management
int ethervox_sdk_set_model_router(ethervox_sdk_t* sdk, ethervox_model_router_t* router) {
    if (!sdk || !router) return -1;
    
    if (sdk->model_router) {
        free(sdk->model_router);
    }
    
    sdk->model_router = router;
    
    printf("Set model router: %s (%d models)\n", router->name, router->model_count);
    
    return 0;
}

int ethervox_sdk_add_model_config(ethervox_sdk_t* sdk, const ethervox_model_config_t* config) {
    if (!sdk || !config) return -1;
    
    if (!sdk->model_router) {
        // Create default model router
        sdk->model_router = (ethervox_model_router_t*)calloc(1, sizeof(ethervox_model_router_t));
        if (!sdk->model_router) {
            strcpy(sdk->last_error, "Failed to allocate model router");
            return -1;
        }
        strcpy(sdk->model_router->name, "Default Router");
    }
    
    if (sdk->model_router->model_count >= 16) {
        strcpy(sdk->last_error, "Maximum number of models reached");
        return -1;
    }
    
    sdk->model_router->models[sdk->model_router->model_count] = *config;
    sdk->model_router->model_count++;
    
    printf("Added model configuration: %s (%s)\n", 
           config->model_name, ethervox_model_type_to_string(config->type));
    
    return 0;
}

// Diagnostics Management
int ethervox_sdk_set_log_callback(ethervox_sdk_t* sdk, ethervox_log_callback_fn callback, void* user_data) {
    if (!sdk || !sdk->diagnostics) return -1;
    
    sdk->diagnostics->log_callback = callback;
    sdk->diagnostics->log_user_data = user_data;
    
    return 0;
}

void ethervox_sdk_log(ethervox_sdk_t* sdk, ethervox_log_level_t level, const char* component,
                     const char* format, ...) {
    if (!sdk || !sdk->diagnostics || !component || !format) return;
    
    if (level < sdk->diagnostics->min_log_level) return;
    
    // Create log entry
    ethervox_log_entry_t entry = {0};
    entry.timestamp = time(NULL);
    entry.level = level;
    strncpy(entry.component, component, sizeof(entry.component) - 1);
    
    // Format message
    va_list args;
    va_start(args, format);
    vsnprintf(entry.message, sizeof(entry.message), format, args);
    va_end(args);
    
    // Add to circular buffer
    ethervox_diagnostics_t* diag = sdk->diagnostics;
    diag->log_buffer[diag->log_head] = entry;
    diag->log_head = (diag->log_head + 1) % 1000;
    if (diag->log_count < 1000) {
        diag->log_count++;
    }
    
    // Call user callback
    if (diag->log_callback) {
        diag->log_callback(&entry, diag->log_user_data);
    }
    
    // Also print to console for development
    printf("[%s] %s: %s\n", 
           ethervox_log_level_to_string(level), 
           component, 
           entry.message);
}

// Utility Functions
const char* ethervox_intent_type_to_string(ethervox_intent_type_t type) {
    switch (type) {
        case ETHERVOX_INTENT_UNKNOWN: return "Unknown";
        case ETHERVOX_INTENT_GREETING: return "Greeting";
        case ETHERVOX_INTENT_QUESTION: return "Question";
        case ETHERVOX_INTENT_COMMAND: return "Command";
        case ETHERVOX_INTENT_CONFIRMATION: return "Confirmation";
        case ETHERVOX_INTENT_FAREWELL: return "Farewell";
        case ETHERVOX_INTENT_HELP: return "Help";
        case ETHERVOX_INTENT_ERROR: return "Error";
        default: return "Custom";
    }
}

const char* ethervox_model_type_to_string(ethervox_model_type_t type) {
    switch (type) {
        case ETHERVOX_MODEL_TYPE_OPENAI_GPT: return "OpenAI GPT";
        case ETHERVOX_MODEL_TYPE_HUGGINGFACE: return "HuggingFace";
        case ETHERVOX_MODEL_TYPE_LOCAL_LLM: return "Local LLM";
        case ETHERVOX_MODEL_TYPE_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* ethervox_log_level_to_string(ethervox_log_level_t level) {
    switch (level) {
        case ETHERVOX_LOG_DEBUG: return "DEBUG";
        case ETHERVOX_LOG_INFO: return "INFO";
        case ETHERVOX_LOG_WARNING: return "WARN";
        case ETHERVOX_LOG_ERROR: return "ERROR";
        case ETHERVOX_LOG_CRITICAL: return "CRIT";
        default: return "UNKNOWN";
    }
}