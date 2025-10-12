# EtherVox SDK Examples

This directory contains comprehensive examples demonstrating how to extend EtherVox using the SDK.

## Examples Overview

### 1. Intent Plugin Example (`intent_plugin_example.c`)

Demonstrates how to create custom intent recognition plugins for domain-specific commands.

**Features:**
- Smart Home Controller plugin implementation
- Intent parsing with entity extraction
- Multi-language support
- Plugin statistics and performance tracking
- JSON entity serialization

**What you'll learn:**
- How to implement intent parsing callbacks
- Entity extraction techniques
- Plugin registration and lifecycle management
- Performance monitoring

**Run example:**
```bash
gcc -o intent_plugin_example intent_plugin_example.c ../ethervox_sdk.c
./intent_plugin_example
```

### 2. Model Router Example (`model_router_example.c`)

Shows how to create intelligent routing between multiple LLM models based on request complexity.

**Features:**
- Multi-model router with smart selection
- Complexity analysis of user requests
- Fallback and retry mechanisms
- Performance statistics and model health monitoring
- Support for OpenAI, HuggingFace, and local models

**What you'll learn:**
- Model routing strategies and algorithms
- Request complexity analysis
- Failover and load balancing
- Model performance optimization

**Run example:**
```bash
gcc -o model_router_example model_router_example.c ../ethervox_sdk.c
./model_router_example
```

### 3. Device Profile Example (`device_profile_example.c`)

Demonstrates device profile creation and hardware abstraction for different platforms.

**Features:**
- Pre-configured profiles for Raspberry Pi, ESP32, and Desktop
- Hardware-specific GPIO pin assignments
- Audio configuration optimization
- Power management settings
- Network capability detection
- Profile serialization to configuration files

**What you'll learn:**
- Hardware abstraction techniques
- Platform-specific optimizations
- Configuration management
- Cross-platform compatibility

**Run example:**
```bash
gcc -o device_profile_example device_profile_example.c ../ethervox_sdk.c
./device_profile_example
```

## Building All Examples

Use the provided Makefile to build all examples at once:

```bash
cd sdk/examples
make
```

This will create executables for all three examples.

## Example Output

### Intent Plugin Example Output:

```
=== EtherVox SDK Intent Plugin Example ===

EtherVox SDK v1.0.0 initialized
Registered intent plugin: SmartHomeController v1.0.0
Testing intent parsing...

Input: "turn on the lights in the living room"
Smart Home Command: turn_on light in living_room (value: 0.0)
  Intent: Command (confidence: 0.85)
  Entities: {"device":"light","action":"turn_on","room":"living_room","value":0.0}
  Context: Smart home control command

Plugin Statistics:
  Total requests: 6
  Successful requests: 5
  Success rate: 83.3%
  Average processing time: 0.12 ms
```

### Model Router Example Output:

```
=== EtherVox SDK Model Router Example ===

EtherVox SDK v1.0.0 initialized
Added model: llama-2-7b (Local LLM)
Added model: gpt-3.5-turbo (OpenAI GPT)
Added model: gpt-4 (OpenAI GPT)
Set model router: Multi-Model Smart Router (3 models)

Testing model routing...

Request 1: "Hello, how are you?"
Request complexity: 0.21
Trying model: llama-2-7b (attempt 1)
  Model used: llama-2-7b
  Processing time: 338 ms
  Confidence: 0.89
  Response: Using local processing, here's my response: Hello, how are you? [Simulated response from llama-2-7b]
```

### Device Profile Example Output:

```
=== EtherVox SDK Device Profile Example ===

Available device templates:
  1. RaspberryPi-ReSpeaker (Raspberry Pi)
     Raspberry Pi 4 with ReSpeaker 4-Mic Array HAT
  2. ESP32-S3-Builtin (ESP32)
     ESP32-S3 with built-in microphone and basic peripherals

Device Profile: RaspberryPi-ReSpeaker
  Platform: Raspberry Pi (4.0)
  Audio: 4 channels @ 48000 Hz, 16-bit
  Mic Sensitivity: -26.0 dBFS
  Echo Cancellation: Yes
  Processing: Edge Inference: Yes, Max Streams: 4
```

## Integration Guide

### Adding Custom Intent Plugins

1. **Define your plugin structure:**
```c
typedef struct {
    // Your custom data
    uint32_t custom_field;
} my_plugin_data_t;
```

2. **Implement the parsing function:**
```c
static int my_intent_parser(const ethervox_stt_input_t* input, 
                           ethervox_intent_result_t* result, 
                           void* user_data) {
    // Your intent recognition logic
    // Return 0 on success, -1 on failure
}
```

3. **Register your plugin:**
```c
ethervox_intent_plugin_t* plugin = create_my_plugin();
ethervox_sdk_register_intent_plugin(&sdk, plugin);
```

### Creating Custom Model Routers

1. **Implement routing logic:**
```c
static int my_model_route(const ethervox_llm_request_t* request,
                         ethervox_llm_response_t* response,
                         const ethervox_model_config_t* config) {
    // Your model routing and inference logic
    return 0;
}
```

2. **Configure your router:**
```c
ethervox_model_router_t* router = create_my_router();
router->route = my_model_route;
ethervox_sdk_set_model_router(&sdk, router);
```

### Defining Device Profiles

1. **Create configuration function:**
```c
static void configure_my_device(ethervox_device_profile_t* profile) {
  snprintf(profile->name, sizeof(profile->name), "%s", "MyDevice");
    profile->mic_array_channels = 2;
    // Set other hardware-specific parameters
}
```

2. **Apply the profile:**
```c
ethervox_device_profile_t profile;
configure_my_device(&profile);
ethervox_sdk_create_device_profile(&sdk, "MyDevice", &profile);
```

## Advanced Usage

### Diagnostics and Logging

Enable comprehensive logging and system monitoring:

```c
// Set up logging callback
ethervox_sdk_set_log_callback(&sdk, my_log_handler, NULL);

// Log messages
ethervox_sdk_log(&sdk, ETHERVOX_LOG_INFO, "MyComponent", 
                 "Processing request with confidence %.2f", confidence);

// Get system metrics
ethervox_system_metrics_t metrics;
ethervox_sdk_get_system_metrics(&sdk, &metrics);
```

### Performance Optimization

- Use local models for low-latency responses
- Implement request caching in your model router
- Configure device profiles to match your hardware capabilities
- Monitor plugin performance and optimize parsing algorithms

## Contributing

Feel free to submit additional examples or improvements to existing ones. Examples should be:

1. **Self-contained** - Include all necessary code and dependencies
2. **Well-documented** - Clear comments explaining the concepts
3. **Practical** - Demonstrate real-world use cases
4. **Educational** - Help developers understand SDK concepts

## Support

For questions about the SDK examples or integration help:

1. Check the main EtherVox documentation
2. Review the SDK header files for API reference
3. Study the example code for implementation patterns
4. Submit issues or questions to the project repository