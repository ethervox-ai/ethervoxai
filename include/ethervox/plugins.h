#ifndef ETHERVOX_PLUGINS_H
#define ETHERVOX_PLUGINS_H

#include "ethervox/config.h"
#include "ethervox/dialogue.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
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

// Plugin metadata
typedef struct {
    char name[64];
    char version[16];
    char author[64];
    char description[256];
    ethervox_plugin_type_t type;
    uint32_t api_version;
    char dependencies[512];
    bool requires_network;
    bool requires_gpu;
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

// Plugin interface functions
typedef struct {
    int (*init)(ethervox_plugin_t* plugin, const ethervox_plugin_config_t* config);
    void (*cleanup)(ethervox_plugin_t* plugin);
    int (*process)(ethervox_plugin_t* plugin, const void* input, void* output);
    int (*configure)(ethervox_plugin_t* plugin, const char* key, const char* value);
    const char* (*get_status)(ethervox_plugin_t* plugin);
} ethervox_plugin_interface_t;

// Plugin structure
struct ethervox_plugin {
    ethervox_plugin_metadata_t metadata;
    ethervox_plugin_config_t config;
    ethervox_plugin_status_t status;
    ethervox_plugin_interface_t interface;
    void* handle;  // Dynamic library handle
    void* private_data;
    uint64_t load_time;
    uint64_t last_used;
    uint32_t usage_count;
    char last_error[256];
};

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

typedef struct {
    char* text;
    char* model_name;
    uint32_t token_count;
    uint32_t processing_time_ms;
    float confidence;
    bool truncated;
    char* finish_reason;
} ethervox_llm_response_t;

// Plugin manager
struct ethervox_plugin_manager {
    ethervox_plugin_t* plugins;
    uint32_t max_plugins;
    uint32_t loaded_plugins;
    char plugin_directory[512];
    char config_file[512];
    
    // Plugin callbacks
    void (*on_plugin_loaded)(const ethervox_plugin_t* plugin, void* user_data);
    void (*on_plugin_error)(const ethervox_plugin_t* plugin, const char* error, void* user_data);
    void* callback_user_data;
    
    bool is_initialized;
};

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
                               const char* api_key);
int ethervox_llm_plugin_huggingface(const ethervox_llm_request_t* request, ethervox_llm_response_t* response, 
                                   const char* model_name, const char* api_key);
int ethervox_llm_plugin_local_rag(const ethervox_llm_request_t* request, ethervox_llm_response_t* response,
                                 const char* index_path);

// Built-in plugins
int ethervox_plugin_register_builtin_openai(ethervox_plugin_manager_t* manager);
int ethervox_plugin_register_builtin_huggingface(ethervox_plugin_manager_t* manager);
int ethervox_plugin_register_builtin_local_rag(ethervox_plugin_manager_t* manager);

// Utility functions
const char* ethervox_plugin_type_to_string(ethervox_plugin_type_t type);
const char* ethervox_plugin_status_to_string(ethervox_plugin_status_t status);
void ethervox_llm_request_free(ethervox_llm_request_t* request);
void ethervox_llm_response_free(ethervox_llm_response_t* response);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PLUGINS_H