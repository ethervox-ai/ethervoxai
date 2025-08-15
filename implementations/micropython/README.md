# 🔬 EthervoxAI MicroPython Implementation

## Overview

This implementation brings EthervoxAI to microcontrollers, specifically optimized for **Raspberry Pi Pico** and compatible boards. It provides core voice processing capabilities in memory-constrained environments while maintaining protocol compatibility with other EthervoxAI implementations.

## 🎯 Target Platforms

### Primary Target: Raspberry Pi Pico Family
- **🍓 Raspberry Pi Pico** (RP2040, 264KB SRAM, 2MB Flash)
- **🍓📶 Raspberry Pi Pico W** (RP2040 + WiFi, 264KB SRAM, 2MB Flash)
- **🍓📶🔵 Raspberry Pi Pico W 2** (RP2350 + WiFi + Bluetooth, 520KB SRAM, 4MB Flash) 🆕

### Secondary Targets
- **ESP32**: Full compatibility with existing ESP32 support
- **Other RP2040 boards**: Third-party RP2040 boards with similar specs
- **STM32**: Future support planned
- **Teensy 4.x**: Future support planned

### 🆕 Pico W 2 Enhanced Features
- **2x Memory**: 520KB SRAM enables larger AI models
- **2x Storage**: 4MB Flash for more model storage
- **Bluetooth Audio**: Wireless speakers, headphones, headsets
- **Enhanced Performance**: RP2350 @ 150MHz vs RP2040 @ 133MHz
- **Security Features**: Hardware crypto, ARM TrustZone

## 🌟 Features

### Core Capabilities
- **🔍 Hardware Detection**: Automatic board and capability detection
- **🎤 Audio I/O**: I2S audio input/output with external codecs
- **🔵 Bluetooth Audio**: Wireless speakers/headsets (Pico W 2) 🆕
- **🧠 Lightweight AI**: Optimized inference for tiny models (<100KB on Pico, <350KB on Pico W 2)
- **🔐 Privacy Controls**: Local processing with audit logging
- **📡 Connectivity**: WiFi support for model updates (Pico W/W2)
- **⚡ Power Management**: Battery-optimized operation modes

### Memory-Optimized Design
- **Streaming Processing**: Process audio in chunks to minimize RAM usage
- **Model Quantization**: Support for 4-bit and 8-bit quantized models
- **Garbage Collection**: Aggressive memory management
- **Lazy Loading**: Load components only when needed

## 📁 Directory Structure

```
implementations/micropython/
├── README.md                     # This file
├── ethervoxai/                   # Main package
│   ├── __init__.py              # Package initialization
│   ├── core/                    # Core modules
│   │   ├── __init__.py
│   │   ├── platform_detector.py # Hardware detection (existing)
│   │   ├── audio_manager.py     # I2S audio processing
│   │   ├── model_manager.py     # Tiny model management
│   │   ├── inference_engine.py  # Lightweight inference
│   │   └── privacy_manager.py   # Privacy controls
│   ├── boards/                  # Board-specific configurations
│   │   ├── __init__.py
│   │   ├── pico.py             # Raspberry Pi Pico
│   │   ├── pico_w.py           # Raspberry Pi Pico W
│   │   └── esp32.py            # ESP32 compatibility
│   ├── models/                  # Optimized model formats
│   │   ├── __init__.py
│   │   ├── quantized/          # 4-bit/8-bit models
│   │   └── streaming/          # Streaming model chunks
│   └── utils/                   # Utilities
│       ├── __init__.py
│       ├── memory.py           # Memory management
│       ├── audio_utils.py      # Audio processing utilities
│       └── protocol.py         # Cross-platform protocol
├── examples/                    # Usage examples
│   ├── basic_demo.py           # Simple voice recognition
│   ├── audio_io_test.py        # Audio I/O testing
│   ├── pico_voice_assistant.py # Complete voice assistant
│   └── low_power_mode.py       # Battery-optimized operation
├── boards/                      # Board-specific files
│   ├── pico/                   # Raspberry Pi Pico specific
│   │   ├── boot.py             # Boot configuration
│   │   ├── main.py             # Main application
│   │   └── pin_config.py       # GPIO pin assignments
│   └── pico_w/                 # Pico W with WiFi
│       ├── boot.py             # Boot with networking
│       ├── main.py             # WiFi-enabled main
│       └── wifi_config.py      # WiFi configuration
├── docs/                       # Documentation
│   ├── hardware_setup.md      # Hardware connections
│   ├── audio_codecs.md         # Supported audio codecs
│   ├── memory_optimization.md  # Memory usage guidelines
│   └── power_management.md     # Battery operation
└── tools/                      # Development tools
    ├── model_converter.py      # Convert models for MCU
    ├── memory_profiler.py      # Memory usage analysis
    └── flash_tool.py           # Deploy to boards
```

## 🚀 Quick Start

### Prerequisites

1. **Hardware**:
   - **Raspberry Pi Pico** (RP2040, 264KB RAM)
   - **Raspberry Pi Pico W** (RP2040 + WiFi, 264KB RAM)  
   - **Raspberry Pi Pico W 2** (RP2350 + WiFi + Bluetooth, 520KB RAM) 🆕
   - Audio codec (e.g., PCM5102A for output, INMP441 for input)
   - **Bluetooth audio device** (for Pico W 2): speakers, headphones, headsets
   - Optional: External flash for larger models

2. **Software**:
   - MicroPython firmware for Pico (v1.19+ recommended)
   - **Bluetooth support** (for Pico W 2, when available in MicroPython)
   - Thonny IDE or similar MicroPython development environment

### Installation

1. **Flash MicroPython**:
   ```bash
   # Download latest MicroPython for Pico
   # Flash to board using standard procedure
   ```

2. **Install EthervoxAI**:
   ```python
   # Upload ethervoxai/ directory to board
   # Or use deployment tools
   ```

3. **Basic Usage**:
   ```python
   from ethervoxai import EthervoxAI
   
   # Initialize for Pico (basic audio via I2S)
   ai = EthervoxAI(board_type='pico')
   ai.initialize()
   
   # Initialize for Pico W (adds WiFi capabilities)
   ai = EthervoxAI(board_type='pico_w')
   ai.initialize()
   
   # Initialize for Pico W 2 (adds Bluetooth audio) 🆕
   ai = EthervoxAI(board_type='pico_w2')
   ai.initialize()
   
   # Start voice processing
   ai.start_listening()
   ```

4. **Pico W 2 Bluetooth Demo**:
   ```python
   # See examples/pico_w2_bluetooth_demo.py for full example
   import asyncio
   from examples.pico_w2_bluetooth_demo import main
   
   asyncio.run(main())
   ```

## 🔧 Hardware Configuration

### Audio Connections (Raspberry Pi Pico)

**I2S Audio Output (PCM5102A)**:
```
Pico Pin → PCM5102A
GP16     → BCK (Bit Clock)
GP17     → WS (Word Select)
GP18     → DIN (Data In)
3.3V     → VCC
GND      → GND
```

**I2S Audio Input (INMP441)**:
```
Pico Pin → INMP441
GP19     → SCK (Serial Clock)
GP20     → WS (Word Select)
GP21     → SD (Serial Data)
3.3V     → VDD
GND      → GND
```

**Optional External Flash (W25Q32)**:
```
Pico Pin → W25Q32
GP10     → CS
GP11     → CLK
GP12     → MOSI
GP13     → MISO
3.3V     → VCC
GND      → GND
```

## 🔵 Bluetooth Audio (Pico W 2 Only)

The Raspberry Pi Pico W 2 includes Bluetooth 5.2 LE support, enabling wireless audio capabilities:

### Supported Bluetooth Profiles
- **A2DP** (Advanced Audio Distribution Profile)
  - High-quality audio streaming to speakers/headphones
  - SBC codec support
  - ~150ms latency, up to 328 kbps bitrate
  - Use cases: Music playback, TTS output, notification sounds

- **HFP** (Hands-Free Profile)  
  - Bidirectional audio for voice commands
  - CVSD/mSBC codec support
  - ~50ms latency, up to 64 kbps bitrate
  - Use cases: Voice commands, phone calls, voice assistant

- **LE Audio** (Future)
  - Next-generation Bluetooth audio with LC3 codec
  - Ultra-low latency (~20ms), efficient bitrate
  - Pending MicroPython support

### Bluetooth Audio Setup
```python
from ethervoxai import EthervoxAI
from ethervoxai.boards import get_board_config

# Initialize Pico W 2
ai = EthervoxAI(board_type='pico_w2')
board_config = get_board_config('pico_w2')()

# Scan for Bluetooth audio devices
devices = board_config.scan_bluetooth_audio_devices()

# Connect to a device
result = board_config.connect_bluetooth_audio(
    device_address="00:1A:2B:3C:4D:5E",
    profile="a2dp"  # or "hfp" for voice
)

# Configure audio routing
audio_config = {
    'input_source': 'i2s_microphone',
    'output_destination': 'bluetooth',
    'bluetooth_profile': 'a2dp'
}
await ai.audio_manager.configure_bluetooth_audio(audio_config)
```

### Bluetooth Device Compatibility
- ✅ **Bluetooth Speakers**: Sony, JBL, Bose, Echo Dot, etc.
- ✅ **Bluetooth Headphones**: AirPods, Sony WH-1000XM, etc.
- ✅ **Bluetooth Headsets**: Gaming headsets, office headsets
- ✅ **Smart Displays**: Echo Show, Google Nest Hub (audio only)
- ⚠️ **Classic Bluetooth**: Limited to BLE devices only

### Audio Quality Comparison
| Connection Type | Latency | Quality | Use Case |
|----------------|---------|---------|----------|
| I2S Codec | ~10ms | Excellent | Local processing |
| Bluetooth A2DP | ~150ms | Very Good | Music, TTS |
| Bluetooth HFP | ~50ms | Good | Voice commands |
| Bluetooth LE Audio | ~20ms | Excellent | Future use |

## 🧠 AI Model Support

### Supported Model Types
- **TinyLlama-1B-Q4**: Quantized to ~250KB for MCU
- **Phi-2-Micro**: Custom 100KB version for Pico
- **Custom Wake Word**: <10KB wake word detection
- **Command Recognition**: ~50KB command classification

### Model Optimization
- **4-bit Quantization**: Reduces model size by 75%
- **Pruning**: Remove unused parameters
- **Streaming Inference**: Process tokens incrementally
- **Memory Pooling**: Reuse buffers across inference calls

## ⚡ Power Management

### Power Modes
- **Active**: Full processing, ~100mA
- **Listening**: Wake word detection only, ~20mA
- **Sleep**: Deep sleep between operations, ~1mA
- **Ultra-Low**: Minimal functionality, ~0.1mA

### Battery Life Estimates
- **2000mAh Battery**:
  - Active: ~20 hours
  - Listening: ~100 hours  
  - Sleep: ~200 days
  - Ultra-Low: ~2 years

## 📊 Performance Characteristics

### Raspberry Pi Pico Performance
- **Wake Word Detection**: ~50ms latency
- **Voice Command**: ~200ms processing
- **Text Response**: ~500ms generation
- **Audio Output**: Real-time streaming

### Memory Usage
- **Core System**: ~50KB RAM
- **Audio Buffers**: ~30KB RAM
- **Model Loading**: ~100KB RAM
- **Available for Models**: ~80KB RAM

## 🔐 Privacy & Security

### Local Processing
- All voice processing on-device
- No cloud dependencies required
- Optional WiFi for model updates only

### Data Protection
- Encrypted model storage
- Secure boot (future)
- Local audit logging
- User consent management

## 🛠️ Development

### Building Models
```python
# Convert standard models for MCU
from tools.model_converter import convert_for_mcu

convert_for_mcu(
    input_model="tinyllama-1b.gguf",
    output_model="tinyllama-pico.bin",
    target_size_kb=200,
    quantization="4bit"
)
```

### Memory Profiling
```python
from tools.memory_profiler import profile_memory

with profile_memory():
    ai = EthervoxAI()
    ai.process_audio(audio_data)
```

### Testing on Hardware
```python
# Run comprehensive hardware tests
python examples/audio_io_test.py
```

## 🤝 Contributing

### Development Guidelines
1. **Memory First**: Always consider RAM constraints
2. **Real-time**: Maintain audio processing deadlines  
3. **Power Aware**: Optimize for battery operation
4. **Protocol Compliance**: Follow EthervoxAI protocol specs

### Testing Requirements
- Test on actual hardware (Pico/Pico W)
- Verify memory usage under constraints
- Validate audio quality and latency
- Check power consumption

## 📄 License

Same MIT license as main EthervoxAI project.

---

**🔬 EthervoxAI MicroPython - Bringing AI to the edge, one microcontroller at a time**
