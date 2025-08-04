# EthervoxAI Windows Desktop Demo

A sample Windows desktop application demonstrating real-time audio input/output integration with EthervoxAI's multilingual voice processing capabilities.

## Features

- **Real-time Audio Capture**: Capture microphone input using Windows audio APIs
- **Speech-to-Text Processing**: Convert spoken input to text using EthervoxAI multilingual runtime
- **AI Response Generation**: Process voice commands through local LLM stack
- **Text-to-Speech Output**: Generate spoken responses with privacy controls
- **Privacy Dashboard**: Real-time privacy controls and audit logging
- **Multiple Language Support**: Automatic language detection and switching

## Prerequisites

### System Requirements
- **Operating System**: Windows 10/11 (64-bit)
- **Node.js**: Version 18+ (with npm)
- **Audio Hardware**: Microphone and speakers/headphones
- **Memory**: 4GB+ RAM recommended for local LLM processing

### Windows Audio Dependencies
**IMPORTANT**: The old `windows-build-tools` package is deprecated and broken. Use modern alternatives:

```powershell
# Modern approach - Install Visual Studio Build Tools
# Download from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
# OR install Visual Studio Community with C++ workload

# Alternative: Use Chocolatey package manager
# Install Chocolatey first: https://chocolatey.org/install
choco install visualstudio2022buildtools
choco install visualstudio2022-workload-vctools
```

### Audio Library Installation (Optional)
```bash
# Basic TTS (always works)
npm install say

# Advanced audio (requires build tools above)
npm install mic speaker wav node-portaudio
```

## Installation

### 1. Install Core Dependencies
```bash
# Navigate to the EthervoxAI project root
cd path/to/ethervoxai

# Install core EthervoxAI modules
npm install

# Install basic TTS support (recommended)
npm install say

# Install advanced audio (optional, requires build tools)
npm install mic speaker wav
```

### 2. Audio Setup Options
```bash
# Option A: Quick Start (Basic TTS)
npm install say

# Option B: Advanced Audio (requires Visual Studio Build Tools)
npm install mic speaker wav

# Option C: Use the setup script
setup-audio-advanced.bat
```

## Usage

### Quick Start
```bash
# Build the core modules
npm run build

# Start the Windows desktop demo
npm run demo:windows

# Or run in development mode with hot reload
npm run demo:windows:dev
```

### Voice Commands
The application responds to these voice commands:

- **"Hello EthervoxAI"** - Activation phrase
- **"What's the weather?"** - Example query (routed to external API with consent)
- **"Set timer for 5 minutes"** - Local processing command
- **"Change language to Spanish"** - Switch language mode
- **"Show privacy settings"** - Open privacy dashboard
- **"Stop listening"** - Pause audio input

## Configuration

### Audio Settings
Edit `config/audio.json`:
```json
{
  "input": {
    "sampleRate": 16000,
    "channels": 1,
    "bitDepth": 16,
    "device": "default"
  },
  "output": {
    "sampleRate": 22050,
    "channels": 2,
    "bitDepth": 16,
    "device": "default"
  },
  "bufferSize": 1024,
  "vadThreshold": 0.3
}
```

### Privacy Configuration
Edit `config/privacy.json`:
```json
{
  "cloudAccessEnabled": false,
  "cloudAccessPerQuery": true,
  "requireExplicitConsent": true,
  "auditLoggingEnabled": true,
  "encryptionEnabled": true,
  "allowedServices": ["weather", "time"],
  "blockedServices": ["location", "contacts"]
}
```

## Architecture

```
┌─────────────────────────────────────────────────┐
│                Windows Desktop App              │
├─────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │
│  │   Audio     │  │  Privacy    │  │   UI     │ │
│  │  Manager    │  │  Dashboard  │  │ Controls │ │
│  └─────────────┘  └─────────────┘  └──────────┘ │
├─────────────────────────────────────────────────┤
│                EthervoxAI Core                  │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │
│  │Multilingual │  │ Local LLM   │  │ Privacy  │ │
│  │  Runtime    │  │    Stack    │  │Dashboard │ │
│  └─────────────┘  └─────────────┘  └──────────┘ │
├─────────────────────────────────────────────────┤
│              Windows Audio APIs                 │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │
│  │  WinMM API  │  │ DirectSound │  │ WASAPI   │ │
│  │ (Legacy)    │  │  (Gaming)   │  │(Modern)  │ │
│  └─────────────┘  └─────────────┘  └──────────┘ │
└─────────────────────────────────────────────────┘
```

## Troubleshooting

### Common Issues

**"Module not found: mic/speaker"**
```bash
# These are optional - demo works without them
# To install, you need Visual Studio Build Tools first
# See installation section above for modern build tools setup
```

**"windows-build-tools failed"**
```bash
# DON'T use windows-build-tools - it's deprecated and broken
# Use Visual Studio Build Tools instead:
# https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
```

**"Permission denied: microphone"**
```bash
# Check Windows privacy settings
# Settings > Privacy > Microphone
# Ensure Node.js has microphone access
```

**"TTS not working"**
```bash
# Install the say package
npm install say

# If that fails, demo will show text output instead
```

### Performance Optimization

1. **Audio Buffer Tuning**:
   - Increase buffer size for stability
   - Decrease for lower latency
   - Monitor CPU usage during processing

2. **Memory Management**:
   ```bash
   # Increase Node.js memory limit
   node --max-old-space-size=4096 demo/windows-desktop.js
   ```

3. **Local LLM Optimization**:
   - Use smaller models for faster response
   - Enable model quantization
   - Cache frequent responses

## Development

### Building from Source
```bash
# Install development dependencies
npm install --save-dev @types/node @types/speaker @types/mic

# Build the desktop application
npm run build:desktop

# Package for Windows distribution
npm run package:windows
```

### Debugging
```bash
# Enable debug logging
DEBUG=ethervox:* npm run demo:windows

# Audio debugging
DEBUG=audio:* npm run demo:windows

# Privacy audit debugging  
DEBUG=privacy:* npm run demo:windows
```

## Security Notes

- **Local Processing**: All voice data is processed locally by default
- **Encryption**: Audio streams are encrypted in memory
- **Privacy Controls**: User consent required for cloud services
- **Audit Logging**: All voice interactions are logged locally
- **Data Retention**: Configurable automatic data deletion

## License

This demo application is part of the EthervoxAI project and follows the same MIT license terms.
