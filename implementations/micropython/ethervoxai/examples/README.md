# 🎙️ EthervoxAI MicroPython Examples

This directory contains example implementations and demos for EthervoxAI on microcontrollers.

## 📋 Available Examples

### 1. 🎤 `basic_demo.py` - Basic Voice Assistant
A simple voice assistant demonstrating core EthervoxAI functionality.

**Features:**
- Auto board detection
- Wake word activation ("Hey Computer")
- Basic voice commands (light control, time, status)
- LED feedback
- Power management

**Usage:**
```python
import asyncio
from examples.basic_demo import main
asyncio.run(main())
```

### 2. 🔊 `audio_io_test.py` - Audio Hardware Test
Comprehensive test suite for validating audio hardware setup.

**Features:**
- Pin configuration validation
- I2S hardware testing
- Microphone input testing
- Speaker output testing
- Audio quality analysis
- Troubleshooting guidance

**Usage:**
```python
from examples.audio_io_test import run_interactive_test
run_interactive_test()
```

### 3. 🍓 `pico_voice_assistant.py` - Raspberry Pi Pico Voice Assistant
Optimized voice assistant specifically for Raspberry Pi Pico constraints.

**Features:**
- Memory-optimized for 264KB SRAM
- Aggressive power management
- Pico-specific optimizations
- Real-time memory monitoring
- Sleep mode for battery operation

**Usage:**
```python
import asyncio
from examples.pico_voice_assistant import main
asyncio.run(main())
```

## 🔌 Hardware Setup

### Required Components

#### For Raspberry Pi Pico / Pico W:
- **Microphone**: INMP441 I2S MEMS microphone
- **DAC**: PCM5102A I2S DAC module
- **Amplifier**: MAX98357A I2S amplifier (optional)
- **Speaker/Headphones**: 4-8Ω speaker or headphones
- **Power**: USB or 3.7V Li-Po battery

#### For ESP32:
- **Microphone**: INMP441 or built-in ADC with analog mic
- **DAC**: PCM5102A or built-in DAC
- **Amplifier**: MAX98357A or built-in amplifier
- **Speaker/Headphones**: 4-8Ω speaker or headphones
- **Power**: USB, 5V, or 3.7V Li-Po battery

### 📐 Wiring Diagrams

#### Raspberry Pi Pico Wiring

```
┌─────────────────┐    ┌─────────────────┐
│   Raspberry Pi  │    │    INMP441      │
│      Pico       │    │   Microphone    │
├─────────────────┤    ├─────────────────┤
│ GP19 (I2S_SCK)  ├────┤ SCK             │
│ GP20 (I2S_WS)   ├────┤ WS              │
│ GP21 (I2S_SD)   ├────┤ SD              │
│ 3V3             ├────┤ VDD             │
│ GND             ├────┤ GND             │
│                 │    │ L/R ── GND      │
└─────────────────┘    └─────────────────┘

┌─────────────────┐    ┌─────────────────┐
│   Raspberry Pi  │    │    PCM5102A     │
│      Pico       │    │      DAC        │
├─────────────────┤    ├─────────────────┤
│ GP16 (I2S_SCK)  ├────┤ BCK             │
│ GP17 (I2S_WS)   ├────┤ LRCK            │
│ GP18 (I2S_SD)   ├────┤ DIN             │
│ 3V3             ├────┤ VCC             │
│ GND             ├────┤ GND             │
│ GP22 (Enable)   ├────┤ XSMT            │
└─────────────────┘    └─────────────────┘
```

#### ESP32 Wiring

```
┌─────────────────┐    ┌─────────────────┐
│      ESP32      │    │    INMP441      │
│                 │    │   Microphone    │
├─────────────────┤    ├─────────────────┤
│ GPIO32 (I2S_SCK)├────┤ SCK             │
│ GPIO33 (I2S_WS) ├────┤ WS              │
│ GPIO34 (I2S_SD) ├────┤ SD              │
│ 3V3             ├────┤ VDD             │
│ GND             ├────┤ GND             │
│                 │    │ L/R ── GND      │
└─────────────────┘    └─────────────────┘

┌─────────────────┐    ┌─────────────────┐
│      ESP32      │    │    PCM5102A     │
│                 │    │      DAC        │
├─────────────────┤    ├─────────────────┤
│ GPIO26 (I2S_SCK)├────┤ BCK             │
│ GPIO25 (I2S_WS) ├────┤ LRCK            │
│ GPIO22 (I2S_SD) ├────┤ DIN             │
│ 3V3             ├────┤ VCC             │
│ GND             ├────┤ GND             │
│ GPIO27 (Enable) ├────┤ XSMT            │
└─────────────────┘    └─────────────────┘
```

## 🚀 Getting Started

### 1. Flash MicroPython
Ensure your microcontroller has MicroPython with I2S support:
- **Pico/Pico W**: Use official MicroPython v1.19+ with I2S
- **ESP32**: Use MicroPython v1.19+ with I2S support

### 2. Upload EthervoxAI
Copy the entire `ethervoxai` directory to your microcontroller:
```bash
# Using mpremote (recommended)
mpremote cp -r ethervoxai/ :

# Or using Thonny IDE
# File -> Open -> Select microcontroller
# Upload ethervoxai folder
```

### 3. Test Audio Hardware
Run the audio test first to verify your setup:
```python
from ethervoxai.examples.audio_io_test import run_interactive_test
run_interactive_test()
```

### 4. Run Basic Demo
Start with the basic demo:
```python
import asyncio
from ethervoxai.examples.basic_demo import main
asyncio.run(main())
```

## 🎯 Voice Commands

### Basic Commands (all examples)
- **"Hey Computer"** - Wake word to activate listening
- **"light on"** / **"turn on light"** - Turn on LED
- **"light off"** / **"turn off light"** - Turn off LED  
- **"what time is it"** - Get current time
- **"how are you"** / **"status"** - System status
- **"help"** - Show available commands
- **"sleep"** / **"sleep mode"** - Enter power saving mode

### Pico-Specific Commands
- **"memory"** - Detailed memory information
- **"system info"** - Hardware information

## ⚡ Power Management

### Power Consumption Estimates

| Mode | Raspberry Pi Pico | ESP32 | Notes |
|------|------------------|-------|-------|
| Active Listening | ~50mA | ~120mA | Continuous audio processing |
| Standby | ~20mA | ~80mA | Wake word detection only |
| Sleep Mode | ~5mA | ~35mA | Button wake only |
| Deep Sleep | ~1mA | ~10mA | Timer wake only |

### Battery Life Estimates (1000mAh Li-Po)

| Usage Pattern | Pico | ESP32 |
|---------------|------|-------|
| Continuous Use | ~20 hours | ~8 hours |
| Normal Use (50% sleep) | ~30 hours | ~12 hours |
| Light Use (80% sleep) | ~40 hours | ~16 hours |

## 🔧 Troubleshooting

### Common Issues

#### 1. No Audio Input
- Check microphone wiring (SCK, WS, SD pins)
- Verify 3.3V power to INMP441
- Ensure L/R pin is connected to GND
- Test with `audio_io_test.py`

#### 2. No Audio Output
- Check DAC wiring (BCK, LRCK, DIN pins)
- Verify enable pin connection (XSMT)
- Check speaker/headphone connection
- Test with `audio_io_test.py`

#### 3. Memory Errors (Pico)
- Use `pico_voice_assistant.py` for optimization
- Reduce audio buffer size in configuration
- Enable more frequent garbage collection
- Monitor memory with built-in commands

#### 4. Wake Word Not Detected
- Speak clearly and at normal volume
- Check microphone sensitivity
- Verify audio input with test script
- Try different wake words or phrases

#### 5. High Power Consumption
- Enable sleep mode when inactive
- Reduce CPU frequency in power config
- Use wake-on-button instead of continuous listening
- Check for audio processing loops

### Debug Commands

```python
# Test individual components
from ethervoxai import EthervoxAI
ai = EthervoxAI()

# Check board detection
print(ai.get_board_info())

# Test audio setup
ai.test_audio_hardware()

# Memory monitoring (Pico)
import gc
print(f"Free memory: {gc.mem_free()} bytes")
```

## 📚 API Reference

### EthervoxAI Main Class

```python
from ethervoxai import EthervoxAI

# Initialize with auto-detection
ai = EthervoxAI()

# Initialize with specific board
ai = EthervoxAI(board_type='pico')

# Main methods
await ai.listen_for_wake_word(timeout_ms=1000)
await ai.listen_for_command(timeout_ms=5000)
await ai.speak(text)
ai.is_button_pressed()
ai.set_led(state)
ai.get_board_info()
```

### Board Configuration

```python
from ethervoxai.boards import get_board_config, detect_board

# Auto-detect board
board = detect_board()
config = get_board_config()

# Manual board selection
config = get_board_config('pico')
config = get_board_config('pico_w')
config = get_board_config('esp32')
```

## 🔮 Future Examples

Planned examples for future releases:
- **WiFi Voice Assistant** - Cloud-connected voice processing
- **Bluetooth Audio** - Wireless audio streaming
- **Multi-Language Support** - Voice commands in different languages
- **Custom Wake Words** - Train your own wake word models
- **Home Automation** - Control IoT devices with voice
- **Voice Recorder** - High-quality audio recording and playback

## 🤝 Contributing

To contribute new examples:

1. Follow the existing code structure
2. Include comprehensive documentation
3. Add hardware requirements and wiring diagrams
4. Test on multiple board types
5. Include troubleshooting guidance

## 📄 License

These examples are part of the EthervoxAI project and follow the same license terms.
