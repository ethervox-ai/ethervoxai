# 🎤 EthervoxAI Complete Voice Interaction Demo

## Overview

This comprehensive demo combines **real microphone input recording**, **real AI model inference**, and **audio output with TTS** to create a complete voice interaction experience. Users can speak directly to EthervoxAI and receive intelligent, privacy-first responses.

## Features

### 🎙️ **Real Audio Input**
- Live microphone recording using system audio tools
- Voice Activity Detection (VAD)
- Wake word detection ("EthervoxAI", "Hey EthervoxAI")
- Configurable recording parameters (sample rate, channels, duration)

### 🧠 **Real AI Processing**
- Local speech-to-text conversion
- Language detection using EthervoxAI's multilingual runtime
- Intent classification and parsing
- Local LLM response generation
- Privacy-first processing (no cloud dependencies)

### 🗣️ **Real Audio Output**
- Text-to-speech using platform-optimized engines
- Multi-language TTS support
- Raspberry Pi optimized audio (eSpeak, Pico2Wave, Festival)
- Bluetooth speaker support

### 🔄 **Complete Conversation Loop**
1. Listen for wake word
2. Record voice command
3. Convert speech to text
4. Detect language and intent
5. Generate AI response
6. Speak response back to user
7. Update conversation history

## Quick Start

### Windows
```bash
# Run the complete voice interaction demo
.\scripts\demo\run-voice-interaction.bat
```

### Raspberry Pi / Linux
```bash
# Run the optimized Raspberry Pi demo
./scripts/demo/run-voice-interaction-pi.sh
```

## Demo Modes

### 1. **Real Voice Interaction Mode**
- Full pipeline with actual microphone input
- Real-time voice processing
- Live AI inference and TTS output
- Requires: microphone, speakers/headphones, built EthervoxAI modules

### 2. **Voice Simulation Mode**
- Demonstrates the complete pipeline without requiring microphone
- Shows all processing steps with simulated voice commands
- Perfect for testing AI logic and TTS without audio hardware

### 3. **Audio Capability Testing**
- Tests microphone recording
- Tests speaker/TTS output
- Validates audio system configuration
- Platform-specific diagnostics

## System Requirements

### **Audio Hardware**
- **Microphone**: USB microphone, built-in microphone, or audio input device
- **Speakers**: Built-in speakers, headphones, USB speakers, or Bluetooth audio
- **Optional**: Bluetooth speakers for wireless audio output

### **Software Dependencies**
- **Node.js**: Version 16+ for running the demo
- **Audio Tools**: 
  - Linux: `alsa-utils` (arecord, aplay)
  - Windows: Built-in audio APIs
  - macOS: Built-in audio tools
- **Text-to-Speech**:
  - Linux: `espeak`, `pico2wave`, or `festival`
  - Windows: SAPI (built-in)
  - macOS: `say` (built-in)

### **EthervoxAI Modules**
- Built TypeScript modules (`npm run build`)
- Multilingual runtime for language detection
- Local LLM stack for AI inference
- Privacy dashboard for data protection

## Platform-Specific Setup

### **Raspberry Pi 5 Setup**
```bash
# Install audio dependencies
sudo apt update
sudo apt install alsa-utils espeak espeak-data pulseaudio

# For Bluetooth audio support
sudo apt install bluetooth bluez pulseaudio-module-bluetooth

# Test microphone
arecord -l
arecord -f S16_LE -c 1 -r 16000 -d 3 test.wav

# Test speakers
aplay test.wav
espeak "Hello EthervoxAI"

# Run the demo
./scripts/demo/run-voice-interaction-pi.sh
```

### **Windows Setup**
```bash
# Ensure Node.js is installed
node --version

# Build EthervoxAI if not already built
npm run build

# Run the demo
.\scripts\demo\run-voice-interaction.bat
```

## Usage Instructions

### **Starting the Demo**
1. Run the appropriate launcher script for your platform
2. Choose from the interactive menu:
   - **Option 1**: Real voice interaction (requires microphone)
   - **Option 2**: Voice simulation (no microphone needed)
   - **Option 3**: Test audio capabilities
   - **Option 4**: Show system information

### **Voice Interaction**
1. **Wake Word**: Say "EthervoxAI" or "Hey EthervoxAI"
2. **Wait for Prompt**: The system will respond with "Yes?" or a beep
3. **Speak Command**: You have 5 seconds to speak your question/command
4. **Receive Response**: EthervoxAI will process and respond verbally

### **Sample Voice Commands**
- "What time is it?"
- "Tell me a joke"
- "What's the weather like?"
- "How are you doing today?"
- "What can you do?"
- "Turn on the lights" (smart home integration)
- "Set a timer for 5 minutes"
- "Play some music"

## Technical Architecture

### **Audio Pipeline**
```
Microphone → Audio Recording → Voice Activity Detection → Wake Word Detection
     ↓
Speech-to-Text → Language Detection → Intent Classification → LLM Processing
     ↓
Response Generation → Text-to-Speech → Audio Output → Speaker/Headphones
```

### **Privacy Features**
- **Local Processing**: All AI inference happens on-device
- **No Cloud Dependencies**: Speech recognition and AI responses are local
- **Data Protection**: No voice data sent to external services
- **Privacy Dashboard**: Logs and controls for data handling
- **Conversation History**: Local storage with automatic cleanup

### **Performance Optimizations**
- **Raspberry Pi Audio**: Optimized TTS engines and audio routing
- **Chunked Recording**: Efficient wake word detection with short audio chunks
- **Async Processing**: Non-blocking audio recording and AI inference
- **Resource Management**: Automatic cleanup of temporary audio files
- **Response Caching**: Faster responses for common queries

## Statistics and Monitoring

The demo tracks:
- **Total Interactions**: Number of voice commands processed
- **Success Rate**: Percentage of successful speech recognitions
- **Response Time**: Average processing time from speech to response
- **Languages**: Detected languages during conversations
- **Conversation History**: Recent interactions with timestamps

## Troubleshooting

### **No Microphone Input**
```bash
# Linux: Check audio devices
arecord -l
alsamixer

# Test recording
arecord -f S16_LE -c 1 -r 16000 -d 3 test.wav
```

### **No Audio Output**
```bash
# Linux: Check speakers
aplay -l
speaker-test

# Test TTS
espeak "test message"
```

### **Bluetooth Audio Issues (Raspberry Pi)**
```bash
# Check Bluetooth status
bluetoothctl show
pactl list sinks short

# Restart audio services
sudo systemctl restart bluetooth
sudo systemctl restart pulseaudio
```

### **Build Issues**
```bash
# Clean and rebuild
npm run clean
npm install
npm run build
```

## Integration with EthervoxAI Modules

### **Multilingual Runtime**
- **Language Detection**: Automatic detection of spoken language
- **Multi-language Support**: Responses in detected language
- **Confidence Scoring**: Language detection confidence metrics

### **Local LLM Stack**
- **Intent Classification**: Understanding user commands and questions
- **Response Generation**: Context-aware AI responses
- **Conversation Context**: Maintains conversation history for better responses

### **Privacy Dashboard**
- **Query Logging**: Records voice interaction statistics
- **Privacy Controls**: Configurable data handling preferences
- **Local Data**: All processing logs stored locally

### **Raspberry Pi Audio Manager**
- **TTS Engine Selection**: Automatic selection of best available TTS
- **Bluetooth Integration**: Seamless wireless audio support
- **Audio Routing**: Optimized audio pipeline for Pi hardware

## Advanced Configuration

### **Audio Settings**
```javascript
audioConfig: {
    sampleRate: 16000,      // Audio quality
    channels: 1,            // Mono recording
    recordingDuration: 5000, // Command recording time (ms)
    vadThreshold: 0.3,      // Voice activity sensitivity
    wakeWordThreshold: 0.7  // Wake word confidence threshold
}
```

### **TTS Settings**
```javascript
ttsConfig: {
    engine: 'espeak',       // TTS engine preference
    speakingRate: 150,      // Words per minute
    voice: 'en',           // Voice language/variant
    volume: 80             // Output volume (0-100)
}
```

### **Privacy Settings**
```javascript
privacyConfig: {
    allowCloudServices: false,  // Disable cloud AI services
    logQueries: true,          // Log interactions locally
    shareAnalytics: false,     // Disable analytics sharing
    dataRetention: 24          // Hours to keep conversation history
}
```

## Example Output

```
🎤 EthervoxAI Voice Interaction Demo
=================================================

🔧 Initializing audio system...
🍓 Using Raspberry Pi optimized audio manager
✅ Audio recording available (arecord)
✅ Audio playback and TTS available
✅ Voice Activity Detection available

🧠 Initializing AI modules...
✅ EthervoxAI AI modules loaded

📊 System Capabilities Summary:
================================
🎤 Audio Recording: ✅ Available
🔊 Audio Playback: ✅ Available
🗣️  Text-to-Speech: ✅ Available
👂 Voice Activity Detection: ✅ Available
🧠 AI Inference: ✅ Available
🔒 Privacy Controls: ✅ Available
🔵 Bluetooth Audio: ✅ Connected

✅ Voice Interaction Demo initialized successfully!

🎤 Starting Voice Interaction Mode
===================================
Say "EthervoxAI" or "Hey EthervoxAI" followed by your question
Press Ctrl+C to stop

👂 Listening for wake word...
🎯 Wake word detected!
🎤 Wake word detected! Recording full command...
🎙️  Recording your command (5 seconds)...
✅ Command recorded
🧠 Processing voice command...
📝 Transcription: "What time is it?"
🌐 Detected language: en (95% confidence)
🎯 Intent: time_query (92% confidence)
💭 Generating response...
🤖 Response: "The current time is 3:45 PM"
🗣️  Speaking response...
⏱️  Total processing time: 2847ms
```

## Integration Examples

### **Smart Home Integration**
```javascript
// Extend the demo for home automation
const response = await homeAutomation.processCommand(transcription);
await this.speakResponse(response);
```

### **Calendar Integration**
```javascript
// Add calendar functionality
const events = await calendar.getTodaysEvents();
const response = `You have ${events.length} events today...`;
```

### **Music Control**
```javascript
// Music playback control
await musicPlayer.play(requestedSong);
await this.speakResponse("Now playing your requested song");
```

This complete voice interaction demo showcases EthervoxAI's full capabilities, providing a foundation for building sophisticated voice-controlled applications while maintaining privacy and security through local processing.
