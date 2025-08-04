# Audio Input/Output Test Suite

Comprehensive testing application for EthervoxAI audio capabilities on all platforms and architectures.

## Overview

This test suite provides an interactive console application to test:
- 🎤 **Audio Input**: Microphone recording with different devices and modules
- 🔊 **Audio Output**: Playback through various audio systems
- 🎵 **Cross-Platform Audio**: Test all 5 audio output methods
- 📊 **Device Discovery**: Enumerate available audio devices
- ⚙️ **Configuration**: Adjust channels, sample rates, modules

## Quick Start

### Windows (Recommended)
```bash
# Run the automated test launcher
.\run-audio-test.bat
```

### Manual Launch
```bash
# Build the project
npm run build

# Launch the test console
node tests/audio-input-output/launch-audio-test.js
```

## Features

### 🎤 Audio Input Testing
- **Record from microphone** with configurable duration
- **Select input devices** and channels
- **Choose recording modules**: Native, WAV, Cross-platform
- **Save recordings** in WAV format
- **Real-time audio capture** with different sample rates

### 🔊 Audio Output Testing  
- **Playback recorded audio** through different output methods
- **Test text-to-speech** with various TTS engines
- **Select output devices** and channels
- **Cross-platform compatibility** testing

### 📊 Device Management
- **Discover audio devices** automatically
- **List input/output devices** with specifications
- **Windows device enumeration** via PowerShell
- **Default device detection**

### 🔧 Module Testing
Test all available audio modules:

1. **Cross-Platform Audio Manager** - EthervoxAI unified system
2. **Native Audio** - Direct mic/speaker packages  
3. **WAV Audio** - File-based recording/playback
4. **Text-to-Speech** - TTS-only output

## Interactive Commands

### Device Commands
- `devices` - List all available audio input/output devices
- `modules` - Show available audio processing modules

### Configuration
- `config` - Show current configuration
- `config <property> <value>` - Update settings
  - `inputDevice` - Select input device ID
  - `outputDevice` - Select output device ID  
  - `inputChannels` - Number of input channels (1-2)
  - `outputChannels` - Number of output channels (1-2)
  - `sampleRate` - Sample rate in Hz (8000, 16000, 44100, etc.)
  - `bitDepth` - Bit depth (8, 16, 24, 32)
  - `recordingDuration` - Recording length in seconds
  - `selectedModule` - Audio module to use

### Recording & Playback
- `record` - Start recording from microphone
- `stop` - Stop current recording
- `recordings` - List all saved recordings
- `play <recording-id>` - Play back a specific recording

### Testing
- `tts <text>` - Test text-to-speech with specified text
- `status` - Show detailed system status

### General
- `help` - Show all available commands
- `quit` - Exit the application

## Example Session

```
AudioTest> devices
🎤 Available Audio Devices:
===========================

📥 Input Devices:
  1. Default Microphone (default)
     ID: default-input, Channels: 1, Sample Rate: 16000Hz

📤 Output Devices:
  1. Default Speakers (default)
     ID: default-output, Channels: 2, Sample Rate: 44100Hz

AudioTest> modules
🔧 Available Audio Modules:
===========================
  1. Cross-Platform Audio Manager
     EthervoxAI unified audio system
     Input: ❌, Output: ✅
     ID: cross-platform

  2. Native Audio (mic + speaker)
     Direct mic/speaker packages
     Input: ✅, Output: ✅
     ID: native

AudioTest> config selectedModule native
✅ selectedModule set to: native

AudioTest> record
🎤 Recording started. Type "stop" to end recording.

AudioTest> stop
🛑 Recording stopped: recording-1691234567890 (5s)
💾 Recording saved to: recordings/recording-1691234567890.wav

AudioTest> play recording-1691234567890
🔊 Playing back recording with Native Audio (mic + speaker)...
✅ Playback completed: recording-1691234567890

AudioTest> tts Hello EthervoxAI, audio testing is working perfectly!
🗣️ Testing TTS: "Hello EthervoxAI, audio testing is working perfectly!"
✅ TTS test completed
```

## Architecture

```
┌─────────────────────────────────────────────────┐
│            Audio Test Console (CLI)            │
├─────────────────────────────────────────────────┤
│           AudioInputOutputTester                │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │
│  │   Device    │  │   Module    │  │Recording │ │
│  │ Discovery   │  │  Manager    │  │ Manager  │ │
│  └─────────────┘  └─────────────┘  └──────────┘ │
├─────────────────────────────────────────────────┤
│         Cross-Platform Audio Manager           │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │
│  │ Native TTS  │  │ WAV Player  │  │ Say TTS  │ │
│  │   (Win32)   │  │(Pure JS)    │  │(X-Platform)│ │
│  └─────────────┘  └─────────────┘  └──────────┘ │
├─────────────────────────────────────────────────┤
│              Native Audio Modules              │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │
│  │     mic     │  │   speaker   │  │   wav    │ │
│  │ (Optional)  │  │ (Optional)  │  │(Optional)│ │
│  └─────────────┘  └─────────────┘  └──────────┘ │
└─────────────────────────────────────────────────┘
```

## File Structure

```
tests/audio-input-output/
├── AudioInputOutputTester.ts    # Core testing engine
├── AudioTestConsole.ts          # Interactive CLI interface  
├── launch-audio-test.js         # Simple Node.js launcher
├── recordings/                  # Saved audio recordings (auto-created)
│   └── *.wav                   # WAV format recordings
└── README.md                   # This file
```

## Troubleshooting

### "Cannot find module" errors
```bash
# Build the project first
npm run build

# Ensure all dependencies are installed
npm install
```

### "No audio modules available"
```bash
# Install cross-platform alternatives (recommended)
npm install say node-wav-player play-sound node-powershell

# Install native audio (optional, requires build tools)
npm install mic speaker wav
```

### "Recording failed" 
- Check microphone permissions in Windows Privacy settings
- Ensure microphone is not being used by another application
- Try switching to a different audio module: `config selectedModule cross-platform`

### "Playback failed"
- Verify speakers/headphones are connected and working
- Test with TTS first: `tts Hello World`
- Try different output modules if available

### ARM64 Windows Issues
The test suite is specifically designed to work on ARM64 Windows:
- ✅ Cross-platform audio manager works on all architectures
- ✅ Text-to-speech always available
- ✅ WAV file processing supported
- ⚠️ Native `speaker` package may not work (automatic fallback available)

## Advanced Usage

### Custom Recording Duration
```
config recordingDuration 10    # 10 second recordings
```

### High-Quality Audio
```
config sampleRate 44100        # CD quality
config bitDepth 16            # 16-bit depth
config outputChannels 2       # Stereo output
```

### Device-Specific Testing
```
config inputDevice specific-mic-id
config outputDevice specific-speaker-id
```

This test suite provides comprehensive validation of EthervoxAI's audio capabilities across all supported platforms and architectures!
