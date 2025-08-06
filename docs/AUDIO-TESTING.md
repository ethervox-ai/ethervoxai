# Audio Testing Suite

## Overview

This project now includes a comprehensive audio input/output testing suite designed to validate audio capabilities across all platforms, especially ARM64 Windows where the original speaker package had compatibility issues.

## Quick Start

### Method 1: Windows Batch Script
```bash
.\run-audio-test.bat
```

### Method 2: NPM Script
```bash
npm run test:audio
```

### Method 3: Direct Node.js
```bash
node tests/audio-input-output/launch-audio-test.js
```

## Features

### Audio Output Testing
The test suite supports 5 different audio output methods:
1. **Native Windows TTS** - Uses Windows Speech Platform
2. **WAV Player** - Plays WAV files using node-wav-player
3. **Play-Sound** - Cross-platform audio using play-sound package
4. **Say TTS** - Cross-platform text-to-speech using say package
5. **PowerShell TTS** - Windows TTS via PowerShell commands

### Audio Input Testing
- **Device Discovery** - Lists all available audio input devices
- **Recording** - Records audio from selected input device
- **Playback** - Plays back recorded audio through selected output method

### Interactive Commands

Once running, you can use these commands:

- `devices` - List all available audio input devices
- `modules` - List all available audio output modules
- `config` - Show current configuration
- `record [duration_seconds] [device_index]` - Start recording
- `stop` - Stop current recording
- `play [module_name]` - Play back last recording
- `tts [module_name] "text"` - Test text-to-speech
- `status` - Show current status
- `help` - Show all commands
- `exit` - Exit the application

## Example Usage

```bash
# Start the test console
npm run test:audio

# List available devices
> devices

# Record 5 seconds from device 0
> record 5 0

# Play back using native TTS
> play native

# Test text-to-speech
> tts wav "Hello, this is a test"

# Show current status
> status

# Exit
> exit
```

## ARM64 Windows Compatibility

This audio system is specifically designed to work on ARM64 Windows by avoiding native compilation dependencies that caused issues with the original speaker package. All audio methods use pure JavaScript implementations or leverage built-in Windows capabilities.

## Integration

The CrossPlatformAudioManager is already integrated into the main EthervoxAI system and can be imported for use in your applications:

```typescript
import { CrossPlatformAudioManager } from './src/modules/crossPlatformAudio';

const audioManager = new CrossPlatformAudioManager();
await audioManager.playAudio("Hello World", "wav");
```

## Troubleshooting

If you encounter issues:

1. Ensure you have the required dependencies installed:
   ```bash
   npm install
   ```

2. Check that your audio devices are properly connected and recognized by Windows

3. Try different audio output modules if one doesn't work:
   ```bash
   > modules
   > play native
   > play wav
   > play playsound
   ```

4. Review the console output for detailed error messages and fallback information
