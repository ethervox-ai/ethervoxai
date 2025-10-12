# EtherVox SDK Documentation

The EtherVox SDK provides a comprehensive framework for extending the EtherVox voice AI system with custom plugins,
model routers, device profiles, and diagnostics tools.

## Overview

The SDK enables developers to:

- **Create Intent Plugins**: Build custom natural language understanding modules for domain-specific voice commands
- **Develop Model Routers**: Implement intelligent routing between multiple LLM models based on request characteristics
 
- **Define Device Profiles**: Configure hardware-specific settings for different platforms and form factors
- **Integrate Diagnostics**: Add comprehensive logging, monitoring, and performance tracking

## Architecture

```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                    EtherVox SDK                         â”‚
â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¤
â”‚  Intent Plugins â”‚  Model Router   â”‚  Device Profiles    â”‚
â”‚  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€  â”‚  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€    â”‚
â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€     â”‚
â”‚  â€¢ Custom NLU   â”‚  â€¢ LLM Selectionâ”‚  â€¢ Hardware Config  â”‚
â”‚  â€¢ Entity Parse â”‚  â€¢ Load Balance â”‚  â€¢ GPIO Mapping     â”‚
â”‚  â€¢ Multi-lang   â”‚  â€¢ Fallback     â”‚  â€¢ Audio Settings   â”‚
â”‚  â€¢ Statistics   â”‚  â€¢ Performance  â”‚  â€¢ Power Management â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```

## Quick Start

### 1. Initialize the SDK

```c
#include "ethervox_sdk.h"

// Initialize SDK instance
ethervox_sdk_t sdk;
if (ethervox_sdk_init(&sdk) != 0) {
    printf("Failed to initialize SDK\n");
    return 1;
}

// Check version compatibility
if (!ethervox_sdk_is_compatible(1, 0)) {
    printf("SDK version incompatible\n");
    return 1;
}
```

### 2. Create an Intent Plugin

```c
// Define plugin data structure
typedef struct {
    uint32_t processed_commands;
} my_plugin_data_t;

// Implement intent parsing
static int my_parse_intent(const ethervox_stt_input_t* input, 
                          ethervox_intent_result_t* result, 
                          void* user_data) {
    my_plugin_data_t* data = (my_plugin_data_t*)user_data;
    
    // Your NLU logic here
    if (strstr(input->text, "my_command")) {
    result->type = ETHERVOX_INTENT_COMMAND;
    result->confidence = 0.9f;
    snprintf(result->entities, sizeof(result->entities), "%s", "{\"action\":\"my_command\"}");
        data->processed_commands++;
        return 0;  // Success
    }
    
    return -1;  // Not recognized
}

// Create and register plugin
ethervox_intent_plugin_t* plugin = calloc(1, sizeof(ethervox_intent_plugin_t));
    snprintf(plugin->name, sizeof(plugin->name), "%s", "MyPlugin");
    snprintf(plugin->version, sizeof(plugin->version), "%s", "1.0.0");
plugin->parse = my_parse_intent;
plugin->user_data = calloc(1, sizeof(my_plugin_data_t));

ethervox_sdk_register_intent_plugin(&sdk, plugin);
```

### 3. Set Up Model Routing

```c
// Configure model
ethervox_model_config_t config = {
    .type = ETHERVOX_MODEL_TYPE_OPENAI_GPT,
    .model_name = "gpt-3.5-turbo",
    .endpoint = "https://api.openai.com/v1/chat/completions",
    .max_tokens = 2048,
    .temperature = 0.8f,
    .timeout_ms = 10000
};
    snprintf(config.api_key, sizeof(config.api_key), "%s", "your-api-key");

ethervox_sdk_add_model_config(&sdk, &config);
```

### 4. Create Device Profile

```c
ethervox_device_profile_t profile = {0};
    snprintf(profile.name, sizeof(profile.name), "%s", "MyDevice");
    snprintf(profile.platform, sizeof(profile.platform), "%s", "Raspberry Pi");
profile.mic_array_channels = 4;
profile.sample_rate = 48000;
profile.gpio_pins.led_status = 12;
profile.has_wifi = true;
profile.supports_edge_inference = true;

*sdk.device_profile = profile;
```

## API Reference

### Core Functions

#### `int ethervox_sdk_init(ethervox_sdk_t* sdk)`

Initialize the SDK instance with default settings.
- **Parameters**: Pointer to SDK structure
- **Returns**: 0 on success, -1 on failure

#### `void ethervox_sdk_cleanup(ethervox_sdk_t* sdk)`  

Clean up SDK resources and registered plugins.
- **Parameters**: Pointer to initialized SDK structure

#### `const char* ethervox_sdk_get_version_string(void)`

Get SDK version as string (e.g., "1.0.0").
- **Returns**: Version string

#### `bool ethervox_sdk_is_compatible(uint32_t major, uint32_t minor)`

Check version compatibility with required version.
- **Parameters**: Required major and minor version numbers
- **Returns**: true if compatible

### Intent Plugin Management

#### `int ethervox_sdk_register_intent_plugin(ethervox_sdk_t* sdk, ethervox_intent_plugin_t* plugin)`

Register a new intent recognition plugin.
- **Parameters**: SDK instance, plugin structure
- **Returns**: 0 on success, -1 on failure

#### `ethervox_intent_plugin_t* ethervox_sdk_find_intent_plugin(ethervox_sdk_t* sdk, const char* name)`

Find registered plugin by name.
- **Parameters**: SDK instance, plugin name
- **Returns**: Plugin pointer or NULL if not found

#### `int ethervox_sdk_process_intent(ethervox_sdk_t* sdk, const ethervox_stt_input_t* input,
ethervox_intent_result_t* result)`
Process input text through all registered plugins.
- **Parameters**: SDK instance, input text structure, result structure
- **Returns**: 0 on success, -1 if no plugin recognized intent

### Model Router Management

#### `int ethervox_sdk_add_model_config(ethervox_sdk_t* sdk, const ethervox_model_config_t* config)`

Add model configuration to router.
- **Parameters**: SDK instance, model configuration
- **Returns**: 0 on success, -1 on failure

#### `int ethervox_sdk_route_llm_request(ethervox_sdk_t* sdk, const ethervox_llm_request_t* request,
ethervox_llm_response_t* response)`
Route LLM request to appropriate model.
- **Parameters**: SDK instance, request structure, response structure  
- **Returns**: 0 on success, -1 on failure

### Diagnostics

#### `void ethervox_sdk_log(ethervox_sdk_t* sdk, ethervox_log_level_t level, const char* component, const char* format,
...)`
Log formatted message with specified level.
- **Parameters**: SDK instance, log level, component name, format string and args

#### `int ethervox_sdk_set_log_callback(ethervox_sdk_t* sdk, ethervox_log_callback_fn callback, void* user_data)`

Set custom log callback function.
- **Parameters**: SDK instance, callback function, user data pointer
- **Returns**: 0 on success

## Data Structures

### Intent Plugin Structure

```c
typedef struct ethervox_intent_plugin_t {
    char name[64];                              // Plugin name
    char version[16];                           // Plugin version
    char description[256];                      // Description
    uint32_t supported_languages_count;        // Number of supported languages
    char supported_languages[16][8];           // Language codes (e.g., "en", "es")
    
    ethervox_intent_parse_fn parse;             // Parsing function
    ethervox_intent_cleanup_fn cleanup;         // Cleanup function
    void* user_data;                            // Plugin-specific data
    
    // Runtime statistics
    bool is_active;                             // Plugin active state
    uint64_t total_requests;                    // Total processed requests
    uint64_t successful_requests;               // Successfully processed requests
    float average_processing_time_ms;           // Average processing time
} ethervox_intent_plugin_t;
```

### Model Configuration Structure

```c
typedef struct {
    ethervox_model_type_t type;                 // Model type (OpenAI, HuggingFace, etc.)
    char model_name[128];                       // Model name
    char endpoint[256];                         // API endpoint URL
    char api_key[256];                          // Authentication key
    bool is_local;                              // Local model flag
    uint32_t max_tokens;                        // Maximum response tokens
    float temperature;                          // Creativity parameter (0.0-1.0)
    uint32_t timeout_ms;                        // Request timeout
} ethervox_model_config_t;
```

### Device Profile Structure  

```c
typedef struct {
    char name[64];                              // Device name
    char hardware_revision[32];                 // Hardware revision
    char platform[32];                          // Platform name
    
    // Audio configuration
    uint32_t mic_array_channels;                // Number of microphone channels
    uint32_t sample_rate;                       // Audio sample rate (Hz)
    uint32_t bit_depth;                         // Audio bit depth
    float mic_sensitivity;                      // Microphone sensitivity (dBFS)
    bool has_echo_cancellation;                 // Echo cancellation support
    bool has_noise_suppression;                 // Noise suppression support
    
    // GPIO pin assignments
    struct {
        uint32_t led_status;                    // Status LED pin
        uint32_t led_recording;                 // Recording indicator pin
        uint32_t button_mute;                   // Mute button pin
        uint32_t button_wake;                   // Wake button pin
        uint32_t i2c_sda;                       // I2C data pin
        uint32_t i2c_scl;                       // I2C clock pin
        uint32_t spi_mosi;                      // SPI MOSI pin
        uint32_t spi_miso;                      // SPI MISO pin  
        uint32_t spi_sclk;                      // SPI clock pin
        uint32_t spi_cs;                        // SPI chip select pin
    } gpio_pins;
    
    // Network capabilities
    bool has_wifi;                              // WiFi support
    bool has_ethernet;                          // Ethernet support
    bool has_bluetooth;                         // Bluetooth support
    char default_ssid[64];                      // Default WiFi SSID
    
    // Processing capabilities
    bool supports_edge_inference;               // Edge AI processing support
    uint32_t max_concurrent_streams;            // Maximum concurrent audio streams
    char preferred_model[64];                   // Preferred AI model name
} ethervox_device_profile_t;
```

## Examples

The SDK includes comprehensive examples demonstrating various use cases:

### Intent Plugin Example

- Smart home voice command recognition
- Entity extraction (device, action, room, value)
- Multi-language support
- Performance statistics

### Model Router Example  

- Intelligent routing between GPT-3.5, GPT-4, and local models
- Request complexity analysis
- Fallback and retry mechanisms
- Performance optimization

### Device Profile Example

- Pre-configured profiles for Raspberry Pi, ESP32, Desktop
- Hardware-specific GPIO mappings
- Audio configuration optimization
- Configuration file generation

## Best Practices

### Plugin Development

1. **Implement robust error handling** - Always validate inputs and handle edge cases
2. **Optimize for performance** - Intent parsing should complete in < 100ms
3. **Support multiple languages** - Use standardized language codes (ISO 639-1)
4. **Provide detailed statistics** - Track success rates and processing times
5. **Use consistent naming** - Follow naming conventions for consistency

### Model Routing

1. **Implement intelligent selection** - Route based on complexity, latency, cost
2. **Add fallback mechanisms** - Handle model failures gracefully  
3. **Monitor performance** - Track response times and success rates
4. **Cache responses** - Implement caching for repeated requests
5. **Respect rate limits** - Handle API rate limiting appropriately

### Device Profiles  

1. **Match hardware capabilities** - Configure settings appropriate for platform
2. **Optimize for power** - Use appropriate sleep timeouts and power management
3. **Document pin assignments** - Clearly specify GPIO usage
4. **Test thoroughly** - Validate configuration on target hardware
5. **Version configurations** - Track profile versions for compatibility

## Troubleshooting

### Common Issues

#### Plugin Not Recognized

- Check plugin is properly registered with `ethervox_sdk_register_intent_plugin()`
- Verify plugin name is unique
- Ensure parse function returns 0 on success
- Check supported language matches input language

#### Model Routing Failures

- Verify API keys are valid and not expired
- Check network connectivity to model endpoints  
- Ensure model configuration parameters are correct
- Monitor timeout settings for slow models

#### Device Profile Issues

- Validate GPIO pin assignments don't conflict
- Check hardware permissions (GPIO access may require root)
- Verify audio device configuration matches hardware
- Test I2C/SPI bus availability

### Debugging

Enable detailed logging to diagnose issues:

```c
// Set minimum log level  
sdk.diagnostics->min_log_level = ETHERVOX_LOG_DEBUG;

// Set up log callback for custom handling
ethervox_sdk_set_log_callback(&sdk, my_log_handler, NULL);

// Log debug information
ethervox_sdk_log(&sdk, ETHERVOX_LOG_DEBUG, "MyComponent",
                 "Processing input: %s", input_text);
```

## Integration Examples

### Web API Integration

```c
// REST endpoint for intent processing
int process_voice_command(const char* audio_data, char* response) {
    ethervox_stt_input_t input;
    // Convert audio to text (using audio runtime)
    snprintf(input.text, sizeof(input.text), "%s", transcribed_text);
    
    ethervox_intent_result_t result;
    if (ethervox_sdk_process_intent(&sdk, &input, &result) == 0) {
        // Process recognized intent
        execute_intent_action(&result, response);
        return 0;
    }
    return -1;
}
```

### IoT Device Integration

```c
// GPIO-based status indication
void update_device_status(ethervox_sdk_t* sdk, bool recording) {
    if (recording) {
        gpio_write(sdk->device_profile->gpio_pins.led_recording, true);
        gpio_write(sdk->device_profile->gpio_pins.led_status, false);
    } else {
        gpio_write(sdk->device_profile->gpio_pins.led_recording, false);  
        gpio_write(sdk->device_profile->gpio_pins.led_status, true);
    }
}
```

### Multi-tenant Applications

```c
// Separate SDK instances per tenant
typedef struct {
    ethervox_sdk_t sdk;
    char tenant_id[64];
    ethervox_intent_plugin_t* custom_plugins[8];
} tenant_context_t;

tenant_context_t* create_tenant_context(const char* tenant_id) {
    tenant_context_t* ctx = calloc(1, sizeof(tenant_context_t));
    snprintf(ctx->tenant_id, sizeof(ctx->tenant_id), "%s", tenant_id);
    ethervox_sdk_init(&ctx->sdk);
    // Load tenant-specific plugins and configuration
    return ctx;
}
```

## Performance Considerations

### Memory Usage

- SDK base overhead: ~50KB
- Each intent plugin: ~5-10KB  
- Model configurations: ~1KB each
- Log buffer: ~100KB (1000 entries)
- Device profile: ~2KB

### Processing Performance

- Intent parsing: 10-100ms typical
- Model routing decision: <10ms
- LLM inference: 500-5000ms depending on model
- Device profile loading: <1ms

### Optimization Tips

1. **Limit active plugins** - Only register needed plugins
2. **Use local models** - For low-latency requirements
3. **Implement request caching** - Cache frequent responses  
4. **Optimize parsing algorithms** - Use efficient pattern matching
5. **Monitor resource usage** - Track CPU and memory consumption

## License

The EtherVox SDK is distributed under the same license as the main EtherVox project. See the LICENSE file for details.

## Contributing

We welcome contributions to the SDK! Please:

1. Follow existing code style and conventions
2. Add comprehensive examples for new features  
3. Update documentation for API changes
4. Test across all supported platforms
5. Submit pull requests with clear descriptions

## Support

For SDK support and questions:

- Check the examples directory for implementation patterns
- Review API documentation in header files  
- Submit issues to the main project repository
- Join community discussions for usage questions