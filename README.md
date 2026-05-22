# EtherVoxAI

> **Privacy-First, Multilingual Voice AI for the Ambient Intelligence Era**

EtherVoxAI is an open-source voice AI platform designed for privacy-conscious users and developers who want to build intelligent voice interfaces without compromising personal data. Built for cross-platform deployment from microcontrollers to desktop systems.

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Build Status](https://img.shields.io/badge/build-passing-green.svg)]()
[![Platform Support](https://img.shields.io/badge/platforms-ESP32%20%7C%20RPi%20%7C%20Windows%20%7C%20Linux-blue.svg)]()
[![Language Support](https://img.shields.io/badge/languages-English%20%7C%20Spanish%20%7C%20Chinese-orange.svg)]()

## Key Features

## Privacy-First Design

- **Local-only processing** - Your voice data never leaves your device
- **Optional cloud integration** - Connect to external APIs only when you choose
- **Privacy dashboard** - Full visibility and control over data usage
- **Zero telemetry** - No tracking, analytics, or data collection

## Multilingual Support

- **Native support** for English, Spanish, and Chinese
- **Automatic language detection** and switching
- **Extensible language framework** for adding new languages
- **Cultural context awareness** for better understanding

## Cross-Platform Compatibility

- **Microcontrollers**: ESP32-S3, ESP32-C3
- **Single Board Computers**: Raspberry Pi Pico, Zero, 4, 5
- **Desktop Systems**: Windows 10/11, Linux distributions
- **Edge Devices**: Optimized for resource-constrained environments

## Intelligent Voice Processing

- **Advanced STT/TTS** with offline capabilities
- **Intent recognition** with extensible plugin system
- **Context-aware conversations** with memory management
- **Noise suppression** and echo cancellation

## Extensible Architecture

- **Plugin system** for custom intents and integrations
- **Model router** for intelligent LLM selection
- **Device profiles** for hardware-specific optimizations
- **Comprehensive SDK** for developers

## Quick Start

## Prerequisites

**Desktop Development (Linux/Windows/macOS):**

- GCC/Clang or MinGW (for Windows cross-compilation)
- CMake 3.16+
- Node.js 18+ (for dashboard)

**Raspberry Pi Cross-Compilation:**

- ARM GCC cross-compiler (`gcc-arm-linux-gnueabihf`)
- bcm2835 library (auto-installed via setup script)
- OpenSSL and libcurl ARM libraries

**ESP32 Development:**

- ESP-IDF 5.0+
- Xtensa toolchain
- (Optional) USB redirection to WSL on Windows 11

## Installation

### Clone and Setup

```bash
# Clone the repository
git clone https://github.com/ethervox-ai/ethervoxai.git
cd ethervoxai

# Option A: Automatic dependency download (recommended for first-time users)
# llama.cpp and whisper.cpp will be downloaded automatically during cmake configure
mkdir build && cd build
cmake ..  # Downloads dependencies on first run (~2-5 minutes one-time)
make -j$(nproc)

# Option B: Manual submodule initialization (advanced users/offline development)
git submodule update --init --recursive  # Downloads ~80MB
mkdir build && cd build
cmake ..
make -j$(nproc)

# Download phonemizer dictionaries (required for TTS)
./scripts/download_phonemizer_data.sh

# Install system dependencies (optional - auto-installs on first build)
make install-deps
```

**Note**: After the first build, dependencies are cached in `external/` and never re-downloaded, even for clean rebuilds.

**Advanced Options:**
- Disable auto-download: `cmake -B build -DETHERVOX_AUTO_FETCH_DEPS=OFF`
- Use custom dependency paths: `cmake -B build -DLLAMA_CPP_CUSTOM_DIR=/path/to/llama.cpp`
- See [Dependency Management Guide](docs/DEPENDENCY_MANAGEMENT.md) for details

## Platform-Specific Builds

**Linux (Native Build):**

```bash

# Configure and build

make configure
make build

# Or use direct CMake

mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests

make test
```text

**Raspberry Pi (Cross-Compilation from Linux):**

```bash

# First-time setup: Install ARM toolchain and libraries

./scripts/setup-rpi-toolchain.sh

# Configure and build

make configure-rpi
make build-rpi

# The binary will be in build-rpi/ethervoxai

# Copy to your Raspberry Pi and run
```text

**Windows (Cross-Compilation from Linux):**

```bash

# Install Windows Cross Compilation Tool chain

cd /<username>/ethervoxai
./scripts/setup-windows-toolchain.sh

# Configure and build

make configure-windows
make build-windows

# The .exe will be in build-windows/ethervoxai.exe
```text

**ESP32 (Using ESP-IDF):**

```bash

# Activate ESP-IDF environment

. ~/esp/esp-idf/export.sh

# Navigate to your project

cd ~/ethervoxai

# Set the target chip

idf.py set-target esp32s3

# Configure the project (optional - opens menuconfig)

idf.py menuconfig

# Build the project

idf.py build

# Flash to device (replace with your port)

# In WSL, this might be /dev/ttyUSB0 or similar after USB passthrough

idf.py -p /dev/ttyUSB0 flash

# Flash and monitor

idf.py -p /dev/ttyUSB0 flash monitor
```text

!NOTE:
to flash in WSL directly you need to redirect the Windows USB port to WSL. To do so check the section

## Build Targets

The project uses a unified Makefile with platform-specific targets:

```bash

# Linux native build

make                    # Default: configure and build
make clean              # Clean build artifacts
make test               # Run unit tests

# Raspberry Pi cross-compilation

make build-rpi          # Configure and build for RPI
make clean-rpi          # Clean RPI build

# Windows cross-compilation

make build-windows      # Configure and build for Windows
make clean-windows      # Clean Windows build

# Clean all platforms

make clean-all          # Clean all build directories
```text

## Launch Dashboard

```bash

# Start the web dashboard

cd dashboard
npm install
npm run dev
```text

Navigate to `http://localhost:3000` to access the EtherVoxAI control panel.

## 5. Run the Application

```bash

# Linux

./build/ethervoxai

# With configuration file

./build/ethervoxai --config=configs/default.conf

# Raspberry Pi (on the device)

./ethervoxai

# Windows

ethervoxai.exe
```

## Desktop CLI Usage (macOS/Linux/Windows)

EtherVoxAI includes a powerful command-line interface with an LLM-powered Governor for intelligent tool orchestration and conversation memory.

### Build the Desktop CLI

```bash
# Install dependencies
npm install

# Build core and examples
npm run build

# Or build just the core
npm run build:core

# Skip GitHub token validation during development
SKIP_GITHUB_TOKEN_VALIDATION=1 make
```

**Note:** GitHub token validation can be skipped by setting `SKIP_GITHUB_TOKEN_VALIDATION=1` environment variable. This disables the bug reporter but allows development without configuring GitHub access.

### Running the Governor CLI

```bash
# Navigate to build directory
cd build

# Run with default settings
./ethervoxai

# Auto-load a model on startup
./ethervoxai --model path/to/model.gguf --auto-load

# Quiet mode (suppress model loading logs)
./ethervoxai --model path/to/model.gguf --auto-load --quiet
```

### CLI Commands

Once running, the Governor CLI supports the following commands:

**Conversation & Memory:**
- Just type naturally to chat with the AI
- `/paste` - Enter multi-line paste mode (end with `/end` or Ctrl+D)
- `/stats` - Show memory statistics (total entries, disk usage)
- `/search <query>` - Search conversation memory
- `/summary [window_size]` - Generate summary of recent conversation
- `/export <filepath>` - Export memory to file (markdown/json)

**Memory Management:**
- `/reminders` - List active reminders
- `/forget <memory_id>` - Delete a specific memory entry

**Model Control:**
- `/load <model_path>` - Load or switch LLM model
- `/help` - Show available commands
- `/quit` or `/exit` - Exit the application

**Model Management:**
- `/models` - List all available models with status and disk usage
- `/modelstatus <type>` - Check status of models (governor/whisper/vosk/piper)
- `/modeldownload <type> <name>` - Download a specific model
- `/modeldelete <type> <name>` - Delete a model to free disk space

### Model Management

EthervoxAI includes a comprehensive model management system for all AI components:

**Check Available Models:**
```bash
> /models
Total disk usage: 1847.23 MB

━━━ Governor LLM ━━━
✅ granite-3.0-2b-instruct-Q4_K_M.gguf [DEFAULT]
   Status: Found, Size: 1464.84 MB

━━━ Whisper STT ━━━
❌ ggml-base.en.bin [DEFAULT]
   Status: Not Found, Expected: 70.57 MB
```

**Download Models:**
```bash
# Download recommended Whisper model
> /modeldownload whisper ggml-base.en.bin

# Download Vosk for real-time conversation
> /modeldownload vosk vosk-model-small-en-us-0.15

# Download Piper TTS voice
> /modeldownload piper en_US-lessac-medium.onnx
```

**Supported Model Types:**
- **Governor LLM**: Language models for conversation and tool orchestration
  - granite-3.0-2b-instruct (1.5GB, recommended)
  - granite-3.0-8b-instruct (5GB, higher quality)
- **Whisper STT**: Speech-to-text for transcription
  - ggml-base.en.bin (74MB, fast and accurate)
  - ggml-small.en.bin (244MB, better accuracy)
- **Vosk STT**: Real-time speech recognition for conversations
  - vosk-model-small-en-us-0.15 (40MB, 10x faster than Whisper)
  - vosk-model-en-us-0.22 (1.8GB, best accuracy)
- **Piper TTS**: Text-to-speech for natural voice output
  - en_US-lessac-medium.onnx (17MB, natural voice)

**Phonemizer Note:**
EthervoxAI uses a custom GPL-free phonemizer for Piper TTS. The phonemizer dictionaries (CMU Dict for English, CC-CEDICT for Chinese) are downloaded via `scripts/download_phonemizer_data.sh` and are not included in the repository.

**Storage Location:**
All models are stored in `~/.ethervox/models/` with subdirectories by type.

**API Integration:**
```c
#include "ethervox/model_downloader.h"

// Check if model exists
ethervox_model_status_t status = ethervox_model_whisper_status("ggml-base.en.bin");
if (status == ETHERVOX_MODEL_STATUS_NOT_FOUND) {
    // Download the model
    ethervox_model_download(ETHERVOX_MODEL_TYPE_WHISPER, "ggml-base.en.bin", NULL, NULL);
}

// List all available models
ethervox_model_info_t* models = NULL;
uint32_t count = 0;
ethervox_model_list(ETHERVOX_MODEL_TYPE_GOVERNOR, &models, &count);
for (uint32_t i = 0; i < count; i++) {
    printf("%s - %s\n", models[i].name, models[i].description);
}
free(models);
```

See [docs/MODEL_MANAGEMENT.md](docs/MODEL_MANAGEMENT.md) for complete documentation.

### Available Tools

The Governor has access to the following tools for enhanced functionality:

**Memory Tools:**
- `memory_store` - Save facts, events, and reminders to conversation memory
- `memory_search` - Search through stored memories by query
- `memory_reminder_list` - List all active reminders
- `memory_complete_reminder` - Mark a reminder as done
- `memory_export` - Export conversation history to file
- `memory_forget` - Delete specific memories

**File Tools (Read-Write Enabled):**
- `file_list` - List files in a directory
- `file_read` - Read text file contents (.txt, .md, .org, .c, .cpp, .h, .sh)
- `file_search` - Search for text patterns in files
- `file_write` - Create or overwrite files with content (max 10MB)

**Compute Tools:**
- `calculator_compute` - Evaluate mathematical expressions
- `percentage_calculate` - Calculate percentages
- `time_get_current` - Get current time and date
- `timer_set` - Set countdown timers

### Example Usage

```bash
# Start the CLI
./build/ethervoxai --model qwen2.5-3b-instruct-q4_k_m.gguf --auto-load

# Chat naturally
> Hello, my name is Alice
Assistant: Nice to meet you, Alice! How can I help you today?

# Ask it to remember things
> Remember that I prefer dark mode for my IDE
Assistant: Got it, I'll remember that you prefer dark mode for your IDE.

# Search memories
> What do you remember about my preferences?
Assistant: You prefer dark mode for your IDE.

# File operations
> Write a markdown note about chickens to ./chickens.md
Assistant: Done! I've created chickens.md with information about chickens.

# Read files
> What's in ./chickens.md?
Assistant: The file contains information about chickens including...

# Calculations
> What's 17 divided by 12?
Assistant: 17 divided by 12 is approximately 1.42.

# Multi-line input
> /paste
(paste mode - type or paste multiple lines, then type /end or press Ctrl+D)
This is line 1
This is line 2
This is line 3
/end
Assistant: I received your multi-line message...
```

### Features

**Intelligent Tool Orchestration:**
- The Governor automatically selects and chains appropriate tools
- Supports parallel tool execution when possible
- Handles tool errors gracefully with helpful suggestions

**Conversation Memory:**
- Persistent memory across sessions (stored in `~/.ethervox/memory/`)
- Automatic importance scoring for memories
- Tag-based organization (personal, preferences, reminders, etc.)
- Search by relevance and recency

**File Access:**
- Sandboxed to specific directories (current directory, home, Documents)
- Extension filtering for security (.txt, .md, .org, .c, .cpp, .h, .sh)
- Path validation and permissions checking

**Performance:**
- Token-efficient tool calls
- Streaming responses for real-time feedback
- Configurable response length (default: 2048 tokens)
- Context window management


## 6. Flashing ESP32 build to device

To flash ESP32 directly from WSL 2.0 you need to redirect the Windows USB port to WSL to do so install usbipd-win from: (https://github.com/dorssel/usbipd-win/releases)[https://github.com/dorssel/usbipd-win/releases]

```powershell

# On Windows PowerShell (as Administrator):

# List USB devices

usbipd list

# Bind your ESP32 device (replace BUSID with your device's bus ID)

usbipd bind --busid <BUSID>

# Attach to WSL

usbipd attach --wsl --busid <BUSID>
```text

In WSL, after attaching USB device with usbipd:

```bash

# Check if device is visible

ls /dev/ttyUSB*

# If not, you may need to install USB drivers

sudo apt-get install linux-tools-virtual hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip \
    /usr/lib/linux-tools/*-generic/usbip 20
```text

or you can flash from Windows once the build is completed using the ESP-IDF Windows installation.
First build in WSL

```bash

# In WSL - build only

idf.py build

# Copy build output to Windows accessible location

cp -r build /mnt/c/Users/YourUsername/ethervoxai-build/
```text

then switch to Windows

```powershell

# In Windows PowerShell - flash

cd C:\Users\YourUsername\ethervoxai-build
esptool.py --chip esp32s3 --port <your COM port> write_flash @flash_args
```text

## System Requirements

## Minimum Hardware Requirements

| Platform | CPU | RAM | Storage | Audio |
|----------|-----|-----|---------|--------|
| ESP32-S3 | 240MHz Dual-Core | 512KB | 4MB Flash | I2S Microphone |
| Raspberry Pi 4 | 1.5GHz Quad-Core | 2GB | 8GB SD Card | USB/HAT Audio |
| Desktop | 1GHz x86_64 | 4GB | 1GB Free | Any Audio Device |

## Recommended Hardware

- **Raspberry Pi 4/5** with ReSpeaker 4-Mic Array HAT
- **ESP32-S3-DevKitC-1** with external I2S microphone
- **Desktop/Laptop** with quality USB microphone

## Architecture Overview

## Platform Abstraction Layer

EtherVoxAI uses a Hardware Abstraction Layer (HAL) to support multiple platforms:

```text
┌─────────────────────────────────────────────────────────────────┐
│                    EtherVoxAI Core Platform                     │
├─────────────────┬─────────────────┬─────────────────────────────┤
│ Audio Runtime   │   Dialogue      │  Platform HAL  │  Dashboard │
│                 │   Engine        │                │            │
│• STT/TTS        │• Intent Parse   │• GPIO/I2C/SPI  │• Web UI    │
│• Noise Sup.     │• LLM Route      │• Power Mgmt    │• Monitoring│
│• Multi-lang     │• Context Mgmt   │• Timers        │• Privacy   │
└─────────────────┴─────────────────┴────────────────┴────────────┘
                  │                                    │
                  ▼                                    ▼
┌─────────────────────────────────────┐  ┌──────────────────────┐
│     Platform-Specific HAL           │  │   Plugin System      │
├──────────┬──────────┬───────────────┤  ├──────────────────────┤
│   RPI    │ Desktop  │    ESP32      │  │• Intent Plugins      │
│          │          │               │  │• Model Routers       │
│• bcm2835 │• Stubs   │• ESP-IDF      │  │• External APIs       │
│• I2S     │• ALSA    │• FreeRTOS     │  │• Custom Hardware     │
│• GPIO    │• WinMM   │• I2S/GPIO     │  │• Device Profiles     │
└──────────┴──────────┴───────────────┘  └──────────────────────┘
```text

**Platform Detection:**

- Automatic platform detection at compile-time
- Platform-specific macros: `ETHERVOX_PLATFORM_RPI`, `ETHERVOX_PLATFORM_LINUX`, `ETHERVOX_PLATFORM_WINDOWS`, `ETHERVOX_PLATFORM_ESP32`
- Unified HAL interface for GPIO, I2C, SPI, timers, and power management
- Platform-specific optimizations for each target

**Cross-Compilation Support:**

- CMake toolchain files for each platform
- Sysroot-based cross-compilation for Raspberry Pi
- MinGW cross-compilation for Windows
- ESP-IDF integration for ESP32

## Usage Examples

## Basic Voice Interaction

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
```text

## Smart Home Integration

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
```text

## Multi-Model LLM Routing

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
```text

## Privacy Features

## Local Data Processing

- **All audio processing** happens on-device
- **Intent recognition** uses local models by default  
- **Conversation history** stored locally with user consent
- **Optional cloud integration** clearly marked and user-controlled

## Privacy Dashboard

The web dashboard provides complete transparency:

- **Data Flow Visualization** - See exactly where your data goes
- **Permission Management** - Granular control over cloud services
- **Usage History** - Review all voice interactions
- **Export/Delete** - Full data portability and deletion

## Security Measures

- **Encryption at rest** for local voice data storage
- **HTTPS/TLS** for all network communications
- **API key management** with secure local storage
- **Regular security audits** and vulnerability assessments

## Development

## Build System Architecture

EtherVoxAI uses a layered build system:

1. **Makefile** - High-level build orchestration with platform-specific targets
1. **CMake** - Cross-platform build configuration and dependency management
1. **Toolchain Files** - Platform-specific compiler and linker settings

**Directory Structure:**

```text
ethervoxai/
├── build/              # Linux native build
├── build-rpi/          # Raspberry Pi cross-compile build
├── build-windows/      # Windows cross-compile build
├── cmake/              # CMake configuration
│   ├── rpi-toolchain.cmake
│   ├── windows-toolchain.cmake
│   └── esp32-toolchain.cmake
├── scripts/            # Build and setup scripts
│   └── setup-rpi-toolchain.sh
├── sysroot/            # Cross-compilation sysroot
│   └── rpi/            # Raspberry Pi libraries and headers
├── src/
│   ├── audio/          # Audio processing
│   │   ├── platform_linux.c
│   │   ├── platform_rpi.c
│   │   ├── platform_windows.c
│   │   └── platform_esp32.c
│   ├── platform/       # Platform HAL implementations
│   │   ├── desktop_hal.c
│   │   ├── rpi_hal.c
│   │   └── esp32_hal.c
│   └── ...
└── tests/              # Unit and integration tests
```text

## Cross-Compilation Setup

**Raspberry Pi:**

```bash

# One-time setup

./scripts/setup-rpi-toolchain.sh

# This script:

# - Downloads ARM GCC cross-compiler

# - Downloads and builds bcm2835 library

# - Downloads ARM versions of OpenSSL and libcurl

# - Sets up sysroot for cross-compilation
```text

**Windows (from Linux):**

```bash

# Install MinGW

sudo apt-get install mingw-w64

# Build

make build-windows
```text

## Adding New Platforms

To add support for a new platform:

1. **Create platform-specific source files:**
   - `src/audio/platform_newplatform.c`
   - `src/platform/newplatform_hal.c`
1. **Add platform detection in CMakeLists.txt:**

```cmake
elseif(TARGET_PLATFORM STREQUAL "NEWPLATFORM")
    add_definitions(-DETHERVOX_PLATFORM_NEWPLATFORM=1)
    set(PLATFORM_SOURCES
        src/audio/platform_newplatform.c
        src/platform/newplatform_hal.c
    )
```text

1. **Create toolchain file** (if cross-compiling):
   - `cmake/newplatform-toolchain.cmake`
1. **Add Makefile targets:**

```makefile
configure-newplatform:
    cmake -B build-newplatform -DTARGET_PLATFORM=NEWPLATFORM

build-newplatform: configure-newplatform
    cmake --build build-newplatform
```text

## Testing

```bash

# Run all tests
make test

# Run tests for specific platform

cd build-rpi && ctest --verbose

# Run specific test

./build/tests/test_audio_core
./build/tests/test_plugin_manager
```text

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details on:

- Code of conduct and community guidelines
- Development environment setup
- Pull request process
- Platform-specific testing guidelines
- Issue reporting and feature requests

## SDK Documentation

Comprehensive API documentation is available:

- **C API**: [SDK Documentation](sdk/README.md)
- **Platform HAL Guide**: [HAL Documentation](docs/platform-hal.md)
- **JavaScript API**: [Dashboard SDK](dashboard/src/sdk/README.md)
- **Python Bindings**: [Python SDK](python/README.md)

## Development

## Build System Architecture

EtherVoxAI uses a layered build system:

1. **Makefile** - High-level build orchestration with platform-specific targets
1. **CMake** - Cross-platform build configuration and dependency management
1. **Toolchain Files** - Platform-specific compiler and linker settings

**Directory Structure:**

```text
ethervoxai/
├── build/              # Linux native build
├── build-rpi/          # Raspberry Pi cross-compile build
├── build-windows/      # Windows cross-compile build
├── cmake/              # CMake configuration
│   ├── rpi-toolchain.cmake
│   ├── windows-toolchain.cmake
│   └── esp32-toolchain.cmake
├── scripts/            # Build and setup scripts
│   └── setup-rpi-toolchain.sh
├── sysroot/            # Cross-compilation sysroot
│   └── rpi/            # Raspberry Pi libraries and headers
├── src/
│   ├── audio/          # Audio processing
│   │   ├── platform_linux.c
│   │   ├── platform_rpi.c
│   │   ├── platform_windows.c
│   │   └── platform_esp32.c
│   ├── platform/       # Platform HAL implementations
│   │   ├── desktop_hal.c
│   │   ├── rpi_hal.c
│   │   └── esp32_hal.c
│   └── ...
└── tests/              # Unit and integration tests
```text

## Cross-Compilation Setup

**Raspberry Pi:**

```bash

# One-time setup

./scripts/setup-rpi-toolchain.sh

# This script:

# - Downloads ARM GCC cross-compiler

# - Downloads and builds bcm2835 library

# - Downloads ARM versions of OpenSSL and libcurl
# - Sets up sysroot for cross-compilation
```text

**Windows (from Linux):**

```bash

# Install MinGW

sudo apt-get install mingw-w64

# Build

make build-windows
```text

## Adding New Platforms

To add support for a new platform:

1. **Create platform-specific source files:**
   - `src/audio/platform_newplatform.c`
   - `src/platform/newplatform_hal.c`
1. **Add platform detection in CMakeLists.txt:**

```cmake
elseif(TARGET_PLATFORM STREQUAL "NEWPLATFORM")
    add_definitions(-DETHERVOX_PLATFORM_NEWPLATFORM=1)
    set(PLATFORM_SOURCES
        src/audio/platform_newplatform.c
        src/platform/newplatform_hal.c
    )
```text

1. **Create toolchain file** (if cross-compiling):
   - `cmake/newplatform-toolchain.cmake`
1. **Add Makefile targets:**

```makefile
configure-newplatform:
    cmake -B build-newplatform -DTARGET_PLATFORM=NEWPLATFORM

build-newplatform: configure-newplatform
    cmake --build build-newplatform
```text

## Testing

```bash

# Run all tests

make test

# Run tests for specific platform

cd build-rpi && ctest --verbose

# Run specific test

./build/tests/test_audio_core
./build/tests/test_plugin_manager
```text

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details on:

- Code of conduct and community guidelines
- Development environment setup
- Pull request process
- Platform-specific testing guidelines
- Issue reporting and feature requests

## SDK Documentation

Comprehensive API documentation is available:

- **C API**: [SDK Documentation](sdk/README.md)
- **Platform HAL Guide**: [HAL Documentation](docs/platform-hal.md)
- **JavaScript API**: [Dashboard SDK](dashboard/src/sdk/README.md)
- **Python Bindings**: [Python SDK](python/README.md)

## SDK Documentation

Comprehensive API documentation is available:

- **C/C++ API**: [SDK Documentation](sdk/README.md)
- **Platform HAL Guide**: [HAL Documentation](docs/platform-hal.md)
- **JavaScript API**: [Dashboard SDK](dashboard/src/sdk/README.md)
- **Python Bindings**: [Python SDK](python/README.md)

## Roadmap

## Phase 1 (Current - v0.1.0)

- [x] Core audio processing engine
- [x] Basic intent recognition
- [x] Multi-platform build system (Linux, RPI, Windows)
- [x] Hardware Abstraction Layer (HAL)
- [x] Cross-compilation support
- [x] Web dashboard MVP
- [x] Unit test framework
- [ ] Plugin system implementation
- [ ] ESP32 platform support

## Phase 2 (v0.2.0)

- [ ] Advanced multi-language support
- [ ] Cloud LLM integrations (OpenAI, HuggingFace)
- [ ] Mobile companion app
- [ ] Advanced privacy controls
- [ ] Continuous integration/deployment

## Phase 3 (v0.3.0)

- [ ] Federated learning capabilities
- [ ] Custom wake word training
- [ ] Advanced context awareness
- [ ] Enterprise deployment tools
- [ ] Performance optimizations for embedded platforms

## License

This project is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License** (CC BY-NC-SA 4.0).

## You are free to:

- **Share** — copy and redistribute the material in any medium or format
- **Adapt** — remix, transform, and build upon the material

## Under the following terms:

- **Attribution** — You must give appropriate credit and indicate if changes were made
- **NonCommercial** — You may not use the material for commercial purposes
- **ShareAlike** — If you remix or adapt, you must distribute under the same license

For commercial licensing options, please contact us at licensing@ethervox-ai.org

See the [LICENSE](LICENSE) file for full terms.

### Third-Party Licenses

EthervoxAI uses several open-source libraries and data sources:
- **ONNX Runtime** (MIT) - Neural network inference
- **Speex DSP** (BSD-3-Clause) - Audio resampling
- **CMU Pronouncing Dictionary** (Public Domain) - English phonemization
- **CC-CEDICT** (CC BY-SA 4.0) - Chinese phonemization
- **llama.cpp** (MIT) - LLM inference
- **whisper.cpp** (MIT) - Speech recognition

For complete licensing information and attribution requirements, see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

## Support

- **Documentation**: [https://docs.ethervox-ai.org](https://docs.ethervox-ai.org)
- **Issues**: [GitHub Issues](https://github.com/ethervox-ai/ethervoxai/issues)
- **Discussions**: [GitHub Discussions](https://github.com/ethervox-ai/ethervoxai/discussions)
- **Email**: support@ethervox-ai.org


## Roadmap

### Long-term Vision (📋 Planned)
- 📋 Hardware device integration and manufacturing
- 📋 Distributed AI model sharing (privacy-preserving)
- 📋 Advanced voice synthesis and emotion detection
- 📋 Multi-modal AI (vision, audio, text) integration
- 📋 Commercial licensing and enterprise features

## License

See [LICENSE.md](LICENSE)

**EtherVoxAI** - Building the future of privacy-first voice AI 🎙️✨
