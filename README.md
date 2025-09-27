# 🎙️ EtherVo## ✨ Key Fe### 🌍 ### 🔧 **Cross### 🔌 **Extensible Architecture**Platform Compatibility***Multilingual Support**tures

### 🔒 **Privacy-First Design**

> **Privacy-First, Multilingual Voice AI for the Ambient Intelligence Era**

EtherVoxAI is an open-source voice AI platform designed for privacy-conscious users and developers who want to build 
intelligent voice interfaces without compromising personal data. Built for cross-platform deployment from 
microcontrollers to desktop systems.

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Build Status](https://img.shields.io/badge/build-passing-green.svg)]()
[![Platform Support](https://img.shields.io/badge/platforms-ESP32%20%7C%20RPi%20%7C%20Windows%20%7C%20Linux-blue.svg)]()
[![Language Support](https://img.shields.io/badge/languages-English%20%7C%20Spanish%20%7C%20Chinese-orange.svg)]()

## âœ¨ Key Features

### ðŸ”’ **Privacy-First Design**

- **Local-only processing** - Your voice data never leaves your device
- **Optional cloud integration** - Connect to external APIs only when you choose
- **Privacy dashboard** - Full visibility and control over data usage
- **Zero telemetry** - No tracking, analytics, or data collection

### ðŸŒ **Multilingual Support**

- **Native support** for English, Spanish, and Chinese
- **Automatic language detection** and switching
- **Extensible language framework** for adding new languages
- **Cultural context awareness** for better understanding

### ðŸ”§ **Cross-Platform Compatibility**

- **Microcontrollers**: ESP32-S3, ESP32-C3
- **Single Board Computers**: Raspberry Pi Pico, Zero, 4, 5
- **Desktop Systems**: Windows 10/11, Linux distributions
- **Edge Devices**: Optimized for resource-constrained environments

### 🎯 **Intelligent Voice Processing**

- **Advanced STT/TTS** with offline capabilities
- **Intent recognition** with extensible plugin system
- **Context-aware conversations** with memory management
- **Noise suppression** and echo cancellation

### ðŸ”Œ **Extensible Architecture**

- **Plugin system** for custom intents and integrations
- **Model router** for intelligent LLM selection
- **Device profiles** for hardware-specific optimizations
- **Comprehensive SDK** for developers

## 🚀 Quick Start

### Prerequisites

- **For Desktop Development**: GCC/Clang, CMake 3.20+, Node.js 18+
- **For ESP32**: ESP-IDF 5.0+, Xtensa toolchain
- **For Raspberry Pi**: ARM GCC toolchain, WiringPi library

### 1. Clone and Build

```bash
# Clone the repository

git clone https://github.com/ethervox-ai/ethervoxai.git
cd ethervoxai

# Build for your platform

mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 2. Cross-Platform Builds

```bash
# ESP32 build

cmake -DCMAKE_TOOLCHAIN_FILE=cmake/esp32.cmake ..
make

# Raspberry Pi build  

cmake -DCMAKE_TOOLCHAIN_FILE=cmake/rpi.cmake ..
make

# Windows cross-compile from Linux

cmake -DCMAKE_TOOLCHAIN_FILE=cmake/windows.cmake ..
make
```

### 3. Launch Dashboard

```bash
# Start the web dashboard

cd dashboard
npm install
npm run dev
```

Navigate to `http://localhost:3000` to access the EtherVoxAI control panel.

### 4. Run Examples

```bash
# Test the core system

./ethervoxai --config=configs/default.conf

# Try SDK examples

cd sdk/examples
make
./intent_plugin_example
./model_router_example
./device_profile_example
```

## ðŸ“‹ System Requirements

### Minimum Hardware Requirements

| Platform | CPU | RAM | Storage | Audio |
|----------|-----|-----|---------|--------|
| ESP32-S3 | 240MHz Dual-Core | 512KB | 4MB Flash | I2S Microphone |
| Raspberry Pi 4 | 1.5GHz Quad-Core | 2GB | 8GB SD Card | USB/HAT Audio |
| Desktop | 1GHz x86_64 | 4GB | 1GB Free | Any Audio Device |

### Recommended Hardware

- **Raspberry Pi 4/5** with ReSpeaker 4-Mic Array HAT
- **ESP32-S3-DevKitC-1** with external I2S microphone
- **Desktop/Laptop** with quality USB microphone

## ðŸ—ï¸ Architecture Overview

```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                    EtherVoxAI Core                      â”‚
â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¤
â”‚Audio Runtimeâ”‚   Dialogue  â”‚   Platform  â”‚    Dashboard  â”‚
â”‚             â”‚   Engine    â”‚   Layer     â”‚               â”‚
â”‚â€¢ STT/TTS    â”‚â€¢ Intent     â”‚â€¢ GPIO/I2C   â”‚â€¢ Web UI       â”‚
â”‚â€¢ Noise Sup. â”‚â€¢ LLM Route  â”‚â€¢ Power Mgmt â”‚â€¢ Monitoring   â”‚
â”‚â€¢ Multi-lang â”‚â€¢ Context    â”‚â€¢ Hardware   â”‚â€¢ Privacy      â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
            â”‚                                              
            â–¼                                              
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                   Plugin System                         â”‚
â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¤
â”‚   Intent    â”‚   Model     â”‚   External  â”‚    Custom     â”‚
â”‚  Plugins    â”‚  Routers    â”‚ Integrationsâ”‚   Hardware    â”‚
â”‚â€¢ Smart Home â”‚â€¢ OpenAI GPT â”‚â€¢ HuggingFaceâ”‚â€¢ Device       â”‚
â”‚â€¢ IoT Controlâ”‚â€¢ Local LLM  â”‚â€¢ Custom APIsâ”‚ Profiles      â”‚
â”‚â€¢ Custom NLU â”‚â€¢ Fallbacks  â”‚â€¢ RAG Systemsâ”‚â€¢ GPIO Maps    â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```

## ðŸ’» Usage Examples

### Basic Voice Interaction

```c
#include "ethervox/audio.h"
#include "ethervox/dialogue.h"

// Initialize the system
ethervox_audio_runtime_t audio;
ethervox_audio_init(&audio, ETHERVOX_LANG_ENGLISH);

ethervox_dialogue_engine_t dialogue;  
ethervox_dialogue_init(&dialogue);

// Process voice input
void process_voice_input(const char* audio_data, size_t length) {
    char transcript[1024];
    if (ethervox_audio_process(&audio, audio_data, length, transcript) == 0) {
        ethervox_intent_result_t intent;
        if (ethervox_dialogue_parse_intent(&dialogue, transcript, &intent) == 0) {
            printf("Intent: %s (confidence: %.2f)\n", 
                   intent.type_name, intent.confidence);
        }
    }
}
```

### Smart Home Integration

```c
#include "ethervox_sdk.h"

// Create smart home plugin
ethervox_intent_plugin_t* smart_home = create_smart_home_plugin();
ethervox_sdk_register_intent_plugin(&sdk, smart_home);

// Process commands like "turn on the living room lights"
ethervox_stt_input_t input = {
    .text = "turn on the living room lights",
    .language = "en"
};

ethervox_intent_result_t result;
ethervox_sdk_process_intent(&sdk, &input, &result);
// Result contains parsed entities: device, action, room
```

### Multi-Model LLM Routing

```javascript
// Dashboard configuration
const modelConfig = {
  models: [
    { name: "gpt-3.5-turbo", type: "openai", priority: 1 },
    { name: "llama-2-7b", type: "local", priority: 2 },
    { name: "claude-3", type: "anthropic", priority: 3 }
  ],
  routing: {
    complexity_threshold: 0.7,
    prefer_local: true,
    fallback_enabled: true
  }
};
```

## ðŸ› ï¸ Development

### Project Structure

```
ethervoxai/
â”œâ”€â”€ CMakeLists.txt              # Root build configuration
â”œâ”€â”€ package.json                # Node.js dependencies
â”œâ”€â”€ src/                        # Core C/C++ source code
â”‚   â”œâ”€â”€ main.cpp               # Application entry point
â”‚   â”œâ”€â”€ audio/                 # Audio processing system
â”‚   â”œâ”€â”€ dialogue/              # Intent and LLM integration  
â”‚   â”œâ”€â”€ platform/              # Hardware abstraction layer
â”‚   â””â”€â”€ plugins/               # Plugin management system
â”œâ”€â”€ include/ethervox/          # Public API headers
â”œâ”€â”€ dashboard/                 # Vue.js web interface
â”‚   â”œâ”€â”€ src/components/        # Vue components
â”‚   â”œâ”€â”€ src/stores/           # Pinia state management
â”‚   â””â”€â”€ src/views/            # Page components
â”œâ”€â”€ sdk/                       # Developer SDK
â”‚   â”œâ”€â”€ ethervox_sdk.h        # SDK API header
â”‚   â”œâ”€â”€ ethervox_sdk.c        # SDK implementation
â”‚   â””â”€â”€ examples/             # Usage examples
â”œâ”€â”€ cmake/                     # Build system configuration
â”œâ”€â”€ configs/                   # Device and runtime configs
â””â”€â”€ docs/                      # Documentation
```

### Building Components

```bash
# Build core system only

cmake -DBUILD_DASHBOARD=OFF ..
make ethervoxai-core

# Build with all features

cmake -DBUILD_ALL=ON ..
make

# Build specific examples

cd sdk/examples
make intent_plugin_example
```

### Running Tests

```bash
# Unit tests

make test

# Integration tests

./scripts/test_integration.sh

# Cross-platform tests

./scripts/test_platforms.sh
```

## ðŸ”Œ Plugin Development

### Creating Intent Plugins

```c
// Define custom intent plugin
static int my_parse_intent(const ethervox_stt_input_t* input, 
                          ethervox_intent_result_t* result, 
                          void* user_data) {
    if (strstr(input->text, "weather")) {
        result->type = ETHERVOX_INTENT_QUESTION;
        result->confidence = 0.9f;
        strcpy(result->entities, "{\"query\":\"weather\"}");
        return 0;
    }
    return -1;
}

// Register plugin
ethervox_intent_plugin_t weather_plugin = {
    .name = "WeatherPlugin",
    .version = "1.0.0", 
    .parse = my_parse_intent
};
ethervox_sdk_register_intent_plugin(&sdk, &weather_plugin);
```

### Device Profiles

```c
// Configure for Raspberry Pi with ReSpeaker HAT
ethervox_device_profile_t rpi_profile = {
    .name = "RaspberryPi-ReSpeaker",
    .platform = "Raspberry Pi",
    .mic_array_channels = 4,
    .sample_rate = 48000,
    .gpio_pins = {
        .led_status = 12,
        .button_mute = 17,
        .i2c_sda = 2,
        .i2c_scl = 3
    },
    .has_wifi = true,
    .supports_edge_inference = true
};
```

## ðŸŒ Privacy & Security

### Data Handling Principles

1. **Local-First Processing**: All voice data processed on-device by default
2. **Explicit Consent**: External API calls require user permission
3. **Data Minimization**: Only collect data necessary for functionality  
4. **Transparency**: Full visibility into data flow and storage
5. **User Control**: Complete control over data retention and deletion

### Security Features

- **End-to-end encryption** for optional cloud communications
- **Secure key storage** for API credentials
- **Regular security audits** of dependencies
- **Sandboxed plugin execution** for third-party extensions

## ðŸ“š Documentation

- **[API Reference](docs/api-reference.md)** - Complete API documentation
- **[SDK Guide](sdk/README.md)** - Developer SDK documentation  
- **[Hardware Guide](docs/hardware.md)** - Supported hardware and setup
- **[Plugin Development](docs/plugins.md)** - Creating custom plugins
- **[Deployment Guide](docs/deployment.md)** - Production deployment
- **[MVP Specification](docs/mvp.md)** - Product requirements and goals

## ðŸ¤ Contributing

We welcome contributions from the community! Please see our [Contributing Guide](CONTRIBUTING.md) for details on:

- Development environment setup
- Code style and conventions  
- Testing requirements
- Pull request process
- Community guidelines

### Quick Contribution Steps

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

## ðŸ“„ License

This project is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License**.

- âœ… **Personal use, modification, and sharing** are encouraged
- âœ… **Educational and research use** is fully supported
- âŒ **Commercial use** requires separate licensing agreement
- ðŸ“ **Attribution** required for all derivative works

See the [LICENSE](LICENSE) file for full details.

## ðŸ™ Acknowledgments

- **Whisper AI** - For advancing open-source speech recognition
- **Vue.js Community** - For the excellent web framework
- **ESP-IDF Team** - For robust embedded development platform
- **Raspberry Pi Foundation** - For accessible computing hardware
- **Open Source Community** - For inspiration and collaboration

## ðŸ“§ Contact & Support

- **GitHub Issues**: [Report bugs and request features](https://github.com/ethervox-ai/ethervoxai/issues)
- **Discussions**: [Community discussions and questions](https://github.com/ethervox-ai/ethervoxai/discussions)  
- **Documentation**: [docs.ethervox.ai](https://docs.ethervox.ai)
- **Email**: support@ethervox.ai

---

<div align="center">

**ðŸŒŸ Star this repository if EtherVoxAI helps you build privacy-first voice applications! ðŸŒŸ**

[â­ Star](https://github.com/ethervox-ai/ethervoxai) â€¢ [ðŸ´ Fork](https://github.com/ethervox-ai/ethervoxai/fork) â€¢ [ðŸ› Report Bug](https://github.com/ethervox-ai/ethervoxai/issues) â€¢ [ðŸ’¡ Request Feature](https://github.com/ethervox-ai/ethervoxai/issues)

</div>
