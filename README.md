# EtherVoxAI

> **Privacy-First, Multilingual Voice AI for the Ambient Intelligence Era**

EtherVoxAI is an open-source voice AI platform designed for privacy-conscious users and developers who want to build intelligent voice interfaces without compromising personal data. Built for cross-platform deployment from microcontrollers to desktop systems.

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Build Status](https://img.shields.io/badge/build-passing-green.svg)]()
[![Platform Support](https://img.shields.io/badge/platforms-ESP32%20%7C%20RPi%20%7C%20Windows%20%7C%20Linux-blue.svg)]()
[![Language Support](https://img.shields.io/badge/languages-English%20%7C%20Spanish%20%7C%20Chinese-orange.svg)]()

## Key Features

### Privacy-First Design

- **Local-only processing** - Your voice data never leaves your device
- **Optional cloud integration** - Connect to external APIs only when you choose
- **Privacy dashboard** - Full visibility and control over data usage
- **Zero telemetry** - No tracking, analytics, or data collection

### Multilingual Support

- **Native support** for English, Spanish, and Chinese
- **Automatic language detection** and switching
- **Extensible language framework** for adding new languages
- **Cultural context awareness** for better understanding

### Cross-Platform Compatibility

- **Microcontrollers**: ESP32-S3, ESP32-C3
- **Single Board Computers**: Raspberry Pi Pico, Zero, 4, 5
- **Desktop Systems**: Windows 10/11, Linux distributions
- **Edge Devices**: Optimized for resource-constrained environments

### Intelligent Voice Processing

- **Advanced STT/TTS** with offline capabilities
- **Intent recognition** with extensible plugin system
- **Context-aware conversations** with memory management
- **Noise suppression** and echo cancellation

### Extensible Architecture

- **Plugin system** for custom intents and integrations
- **Model router** for intelligent LLM selection
- **Device profiles** for hardware-specific optimizations
- **Comprehensive SDK** for developers

## Quick Start

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

## System Requirements

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

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    EtherVoxAI Core Platform                     │
├─────────────────┬─────────────────┬─────────────────────────────┤
│ Audio Runtime   │   Dialogue      │  Platform   │    Dashboard  │
│                 │   Engine        │   Layer     │               │
│• STT/TTS        │• Intent Parse   │• GPIO/I2C   │• Web UI       │
│• Noise Sup.     │• LLM Route      │• Power Mgmt │• Monitoring   │
│• Multi-lang     │• Context        │• Hardware   │• Privacy      │
└─────────────────┴─────────────────┴─────────────┴───────────────┘
            │                                              
            ▼                                              
┌─────────────────────────────────────────────────────────────────┐
│                   Plugin System                                 │
├─────────────────┬─────────────────┬─────────────────┬───────────┤
│   Intent        │   Model         │   External      │    Custom │
│  Plugins        │  Routers        │ Integrations    │ Hardware  │
│• Smart Home     │• OpenAI GPT     │• HuggingFace    │• Device   │
│• IoT Control    │• Local LLM      │• Custom APIs    │ Profiles  │
│• Custom NLU     │• Fallbacks      │• RAG Systems    │• GPIO Maps│
└─────────────────┴─────────────────┴─────────────────┴───────────┘
```

## Usage Examples

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
const ethervox = require('@ethervox/dashboard-sdk');

// Configure model routing
const router = new ethervox.ModelRouter({
    primary: 'local-llm',    // Try local model first
    fallback: 'openai-gpt4', // Fallback to cloud if needed
    privacy: 'local-only'    // Override: never use cloud
});

// Route based on complexity
router.addRoute({
    condition: (intent) => intent.complexity < 0.7,
    model: 'local-llm'
});

router.addRoute({
    condition: (intent) => intent.requires_web_search,
    model: 'openai-gpt4'
});
```

## Privacy Features

### Local Data Processing

- **All audio processing** happens on-device
- **Intent recognition** uses local models by default  
- **Conversation history** stored locally with user consent
- **Optional cloud integration** clearly marked and user-controlled

### Privacy Dashboard

The web dashboard provides complete transparency:

- **Data Flow Visualization** - See exactly where your data goes
- **Permission Management** - Granular control over cloud services
- **Usage History** - Review all voice interactions
- **Export/Delete** - Full data portability and deletion

### Security Measures

- **Encryption at rest** for local voice data storage
- **HTTPS/TLS** for all network communications
- **API key management** with secure local storage
- **Regular security audits** and vulnerability assessments

## Development

### Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details on:

- Code of conduct and community guidelines
- Development environment setup
- Pull request process
- Issue reporting and feature requests

### Build System

EtherVoxAI uses CMake for cross-platform builds:

```bash
# Configure build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build with specific features
cmake -B build -DESP32_BUILD=ON -DRPI_BUILD=ON

# Run tests
ctest --test-dir build
```

### SDK Documentation

Comprehensive API documentation is available:

- **C/C++ API**: [SDK Documentation](sdk/README.md)
- **JavaScript API**: [Dashboard SDK](dashboard/src/sdk/README.md)
- **Python Bindings**: [Python SDK](python/README.md)

## Roadmap

### Phase 1 (Current)
- [x] Core audio processing engine
- [x] Basic intent recognition
- [x] ESP32 and Raspberry Pi support
- [x] Web dashboard MVP
- [ ] Plugin system implementation

### Phase 2
- [ ] Advanced multi-language support
- [ ] Cloud LLM integrations (OpenAI, HuggingFace)
- [ ] Mobile companion app
- [ ] Advanced privacy controls

### Phase 3
- [ ] Federated learning capabilities
- [ ] Custom wake word training
- [ ] Advanced context awareness
- [ ] Enterprise deployment tools

## License

This project is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License** (CC BY-NC-SA 4.0).

### You are free to:
- **Share** — copy and redistribute the material in any medium or format
- **Adapt** — remix, transform, and build upon the material

### Under the following terms:
- **Attribution** — You must give appropriate credit and indicate if changes were made
- **NonCommercial** — You may not use the material for commercial purposes
- **ShareAlike** — If you remix or adapt, you must distribute under the same license

For commercial licensing options, please contact us at licensing@ethervox-ai.org

See the [LICENSE](LICENSE) file for full terms.

## Support

- **Documentation**: [https://docs.ethervox-ai.org](https://docs.ethervox-ai.org)
- **Issues**: [GitHub Issues](https://github.com/ethervox-ai/ethervoxai/issues)
- **Discussions**: [GitHub Discussions](https://github.com/ethervox-ai/ethervoxai/discussions)
- **Email**: support@ethervox-ai.org

---

**EtherVoxAI** - Building the future of privacy-first voice AI 🎙️✨