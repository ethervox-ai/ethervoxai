/**
 * @file ethervox_sdk.h
 * @brief EthervoxAI SDK header definitions
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
#ifndef ETHERVOX_SDK_H
#define ETHERVOX_SDK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// SDK version information
#define ETHERVOX_SDK_VERSION_MAJOR 1
#define ETHERVOX_SDK_VERSION_MINOR 0
#define ETHERVOX_SDK_VERSION_PATCH 0

// Forward declarations
typedef struct ethervox_sdk_t ethervox_sdk_t;
typedef struct ethervox_intent_plugin_t ethervox_intent_plugin_t;
typedef struct ethervox_model_router_t ethervox_model_router_t;
typedef struct ethervox_diagnostics_t ethervox_diagnostics_t;
typedef struct ethervox_device_profile_t ethervox_device_profile_t;

// SDK Intent Plugin Interface
typedef enum {
    ETHERVOX_INTENT_UNKNOWN = 0,
    ETHERVOX_INTENT_GREETING,
    ETHERVOX_INTENT_QUESTION,
    ETHERVOX_INTENT_COMMAND,
    ETHERVOX_INTENT_CONFIRMATION,
    ETHERVOX_INTENT_FAREWELL,
    ETHERVOX_INTENT_HELP,
    ETHERVOX_INTENT_ERROR,
    ETHERVOX_INTENT_CUSTOM_BASE = 1000  // Custom intents start from 1000
} ethervox_intent_type_t;

typedef struct {
    ethervox_intent_type_t type;
    float confidence;
    char entities[512];  // JSON string of extracted entities
    char context[256];   // Additional context information
    uint64_t timestamp;
    char language[8];    // ISO language code
} ethervox_intent_result_t;

typedef struct {
    char text[1024];
    char language[8];
    float audio_confidence;
    uint32_t processing_time_ms;
} ethervox_stt_input_t;

// Intent plugin callbacks
typedef int (*ethervox_intent_parse_fn)(const ethervox_stt_input_t* input, 
                                       ethervox_intent_result_t* result, 
                                       void* user_data);
typedef void (*ethervox_intent_cleanup_fn)(void* user_data);

// Intent plugin structure
struct ethervox_intent_plugin_t {
    char name[64];
    char version[16];
    char description[256];
    uint32_t supported_languages_count;
    char supported_languages[16][8];  // Up to 16 language codes
    
    ethervox_intent_parse_fn parse;
    ethervox_intent_cleanup_fn cleanup;
    void* user_data;
    
    // Plugin metadata
    bool is_active;
    uint64_t total_requests;
    uint64_t successful_requests;
    float average_processing_time_ms;
};

// Model Router Interface
typedef enum {
    ETHERVOX_MODEL_TYPE_OPENAI_GPT = 0,
    ETHERVOX_MODEL_TYPE_HUGGINGFACE,
    ETHERVOX_MODEL_TYPE_LOCAL_LLM,
    ETHERVOX_MODEL_TYPE_CUSTOM
} ethervox_model_type_t;

typedef struct {
    ethervox_model_type_t type;
    char model_name[128];
    char endpoint[256];
    char api_key[256];
    bool is_local;
    uint32_t max_tokens;
    float temperature;
    uint32_t timeout_ms;
} ethervox_model_config_t;

typedef struct {
    char prompt[2048];
    char context[1024];
    char language[8];
    uint32_t max_response_length;
    float creativity_level;  // 0.0 to 1.0
    bool stream_response;
} ethervox_llm_request_t;

typedef struct {
    char response[4096];
    bool is_complete;
    float confidence;
    uint32_t processing_time_ms;
    uint32_t token_count;
    char model_used[128];
} ethervox_llm_response_t;

// Model router callbacks
typedef int (*ethervox_model_route_fn)(const ethervox_llm_request_t* request,
                                      ethervox_llm_response_t* response,
                                      const ethervox_model_config_t* config);

struct ethervox_model_router_t {
    char name[64];
    uint32_t model_count;
    ethervox_model_config_t models[16];  // Up to 16 models
    ethervox_model_route_fn route;
    
    // Routing statistics
    uint64_t total_requests;
    uint64_t successful_requests;
    float average_response_time_ms;
    uint32_t active_model_index;
};

// Diagnostics Interface
typedef enum {
    ETHERVOX_LOG_DEBUG = 0,
    ETHERVOX_LOG_INFO,
    ETHERVOX_LOG_WARNING,
    ETHERVOX_LOG_ERROR,
    ETHERVOX_LOG_CRITICAL
} ethervox_log_level_t;

typedef struct {
    uint64_t timestamp;
    ethervox_log_level_t level;
    char component[64];
    char message[512];
    char file[256];
    uint32_t line;
} ethervox_log_entry_t;

typedef struct {
    float cpu_usage_percent;
    uint32_t memory_used_mb;
    uint32_t memory_total_mb;
    float disk_usage_percent;
    float temperature_celsius;
    uint32_t network_rx_kbps;
    uint32_t network_tx_kbps;
    bool audio_input_active;
    bool audio_output_active;
    uint32_t active_connections;
} ethervox_system_metrics_t;

typedef void (*ethervox_log_callback_fn)(const ethervox_log_entry_t* entry, void* user_data);

struct ethervox_diagnostics_t {
    ethervox_log_callback_fn log_callback;
    void* log_user_data;
    
    ethervox_log_entry_t log_buffer[1000];  // Circular buffer
    uint32_t log_head;
    uint32_t log_count;
    
    ethervox_system_metrics_t current_metrics;
    uint64_t metrics_update_interval_ms;
    uint64_t last_metrics_update;
    
    // Diagnostics configuration
    ethervox_log_level_t min_log_level;
    bool enable_performance_tracking;
    bool enable_memory_profiling;
};

// Device Profile Interface
typedef struct {
    char name[64];
    char hardware_revision[32];
    char platform[32];
    
    // Audio configuration
    uint32_t mic_array_channels;
    uint32_t sample_rate;
    uint32_t bit_depth;
    float mic_sensitivity;
    bool has_echo_cancellation;
    bool has_noise_suppression;
    
    // GPIO pin assignments
    struct {
        uint32_t led_status;
        uint32_t led_recording;
        uint32_t button_mute;
        uint32_t button_wake;
        uint32_t i2c_sda;
        uint32_t i2c_scl;
        uint32_t spi_mosi;
        uint32_t spi_miso;
        uint32_t spi_sclk;
        uint32_t spi_cs;
    } gpio_pins;
    
    // Power management
    bool supports_low_power_mode;
    uint32_t sleep_timeout_ms;
    float battery_low_threshold_v;
    
    // Networking
    bool has_wifi;
    bool has_ethernet;
    bool has_bluetooth;
    char default_ssid[64];
    
    // Processing capabilities
    bool supports_edge_inference;
    uint32_t max_concurrent_streams;
    char preferred_model[64];
    
} ethervox_device_profile_t;

// SDK Main Interface
struct ethervox_sdk_t {
    // Version information
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    
    // Core components
    ethervox_intent_plugin_t* intent_plugins[16];
    uint32_t intent_plugin_count;
    
    ethervox_model_router_t* model_router;
    ethervox_diagnostics_t* diagnostics;
    ethervox_device_profile_t* device_profile;
    
    // SDK state
    bool is_initialized;
    char last_error[256];
    uint64_t initialization_time;
};

// SDK Core Functions
int ethervox_sdk_init(ethervox_sdk_t* sdk);
void ethervox_sdk_cleanup(ethervox_sdk_t* sdk);
const char* ethervox_sdk_get_version_string(void);
bool ethervox_sdk_is_compatible(uint32_t major, uint32_t minor);

// Intent Plugin Management
int ethervox_sdk_register_intent_plugin(ethervox_sdk_t* sdk, ethervox_intent_plugin_t* plugin);
int ethervox_sdk_unregister_intent_plugin(ethervox_sdk_t* sdk, const char* plugin_name);
ethervox_intent_plugin_t* ethervox_sdk_find_intent_plugin(ethervox_sdk_t* sdk, const char* name);
int ethervox_sdk_process_intent(ethervox_sdk_t* sdk, const ethervox_stt_input_t* input, 
                               ethervox_intent_result_t* result);

// Model Router Management
int ethervox_sdk_set_model_router(ethervox_sdk_t* sdk, ethervox_model_router_t* router);
int ethervox_sdk_add_model_config(ethervox_sdk_t* sdk, const ethervox_model_config_t* config);
int ethervox_sdk_route_llm_request(ethervox_sdk_t* sdk, const ethervox_llm_request_t* request,
                                  ethervox_llm_response_t* response);

// Diagnostics Management
int ethervox_sdk_set_log_callback(ethervox_sdk_t* sdk, ethervox_log_callback_fn callback, void* user_data);
void ethervox_sdk_log(ethervox_sdk_t* sdk, ethervox_log_level_t level, const char* component,
                     const char* format, ...);
int ethervox_sdk_get_system_metrics(ethervox_sdk_t* sdk, ethervox_system_metrics_t* metrics);
int ethervox_sdk_export_diagnostics(ethervox_sdk_t* sdk, const char* filepath);

// Device Profile Management
int ethervox_sdk_load_device_profile(ethervox_sdk_t* sdk, const char* profile_path);
int ethervox_sdk_save_device_profile(ethervox_sdk_t* sdk, const char* profile_path);
int ethervox_sdk_create_device_profile(ethervox_sdk_t* sdk, const char* profile_name,
                                      const ethervox_device_profile_t* template);

// Utility Functions
const char* ethervox_intent_type_to_string(ethervox_intent_type_t type);
const char* ethervox_model_type_to_string(ethervox_model_type_t type);
const char* ethervox_log_level_to_string(ethervox_log_level_t level);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_SDK_H