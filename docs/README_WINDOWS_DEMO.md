# 🖥️ Windows Desktop Demo - Quick Start

This is a sample Windows desktop application that demonstrates EthervoxAI's real-time voice processing capabilities.

## 🚀 Quick Start

### Option 1: Double-click to run
```
start-windows-demo.bat
```

### Option 2: Command line
```bash
npm run demo:windows:dev
```

## 🎤 Features Demonstrated

- **Voice Input**: Microphone capture (or simulation)
- **Speech Recognition**: Convert speech to text
- **Language Detection**: Automatic language identification  
- **AI Processing**: Local LLM intent parsing and response
- **Privacy Controls**: User consent and audit logging
- **Voice Output**: Text-to-speech synthesis

## 🎯 Try These Voice Commands

- "Hello EthervoxAI" - Activation phrase
- "What time is it" - Time query
- "Set timer for 5 minutes" - Local processing
- "Change language to Spanish" - Language switching
- "Show privacy settings" - Privacy dashboard
- "Stop listening" - Pause voice input

## 🛠️ Interactive Commands

While running, type these commands:

- `start` - Begin voice listening
- `stop` - Stop voice listening
- `status` - Show current status  
- `quit` - Exit application

## 📊 What You'll See

```
🚀 Initializing EthervoxAI Windows Desktop Demo...
✅ Desktop demo initialized successfully!
💬 Say "Hello EthervoxAI" to start voice interaction

🎤 Starting voice input...
🗣️ Started voice session: session_1703123456789
🎯 Processing speech input...
📝 Transcription: "Hello EthervoxAI"
🌐 Detected language: en (95% confidence)
🧠 Intent: greeting (87% confidence)
🏠 Response from local LLM
💬 Response: "Hello! I'm EthervoxAI, your privacy-focused voice assistant."
🔊 Generating speech output in en...
🔊 [SIMULATION] Playing: "Hello! I'm EthervoxAI..."
```

## 🔧 Configuration

- **Audio settings**: `config/audio.json`
- **Privacy settings**: `config/privacy.json`

## 🚨 Troubleshooting

**No audio libraries?**
- The demo runs great with just TTS (`npm install say`)
- Advanced audio is optional and requires Visual Studio Build Tools
- ❌ DON'T use `windows-build-tools` (deprecated and broken)
- ✅ Use Visual Studio Build Tools or Community edition instead

**No speech output?**
- Install TTS engine: `npm install say`
- Uses Windows built-in voices (works on all Windows systems)
- Falls back to text display if unavailable

**Build errors?**
- Skip advanced audio libraries if you get build errors
- Basic demo with TTS works perfectly without mic/speaker packages
- Use `setup-audio-advanced.bat` for guided installation

**Permission errors?**
- Check Windows microphone privacy settings
- Settings > Privacy > Microphone > Allow apps

**High CPU usage?**
- Increase buffer sizes in `config/audio.json`
- Use smaller LLM models

## 🏗️ Architecture

```
User Voice → Microphone → EthervoxAI → Local LLM → Privacy Check → Response → Speakers
```

This demo showcases EthervoxAI's core capabilities in a real Windows environment!
