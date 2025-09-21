#!/bin/bash

# 🎤 EthervoxAI Complete Voice Interaction Demo for Raspberry Pi 5
# Optimized for Raspberry Pi with real audio recording and AI inference

echo ""
echo "================================================="
echo "🎤 EthervoxAI Voice Interaction Demo (Raspberry Pi)"
echo "================================================="
echo ""
echo "This demo provides:"
echo "  🎙️  Real microphone input recording"
echo "  🧠 Real AI model inference"
echo "  🗣️  Audio output with TTS"
echo "  👂 Voice activity detection"
echo "  🎯 Wake word detection"
echo "  🔄 Full conversation loop"
echo ""

# Check if Node.js is available
if ! command -v node &> /dev/null; then
    echo "❌ Node.js is not installed"
    echo "   Install with: sudo apt install nodejs npm"
    exit 1
fi

echo "✅ Node.js version: $(node --version)"

# Check if audio tools are available
echo ""
echo "🔧 Checking audio capabilities..."

AUDIO_RECORD_OK=false
AUDIO_PLAY_OK=false
TTS_OK=false

# Check recording capability
if command -v arecord &> /dev/null; then
    echo "✅ Audio recording available (arecord)"
    AUDIO_RECORD_OK=true
else
    echo "❌ Audio recording not available (install alsa-utils)"
fi

# Check playback capability
if command -v aplay &> /dev/null; then
    echo "✅ Audio playback available (aplay)"
    AUDIO_PLAY_OK=true
else
    echo "❌ Audio playback not available (install alsa-utils)"
fi

# Check TTS capability
if command -v espeak &> /dev/null; then
    echo "✅ Text-to-speech available (espeak)"
    TTS_OK=true
elif command -v pico2wave &> /dev/null; then
    echo "✅ Text-to-speech available (pico2wave)"
    TTS_OK=true
elif command -v festival &> /dev/null; then
    echo "✅ Text-to-speech available (festival)"
    TTS_OK=true
else
    echo "❌ Text-to-speech not available"
    echo "   Install with: sudo apt install espeak espeak-data"
fi

# Check PulseAudio for Bluetooth
if command -v pactl &> /dev/null; then
    echo "✅ PulseAudio available (for Bluetooth audio)"
else
    echo "⚠️  PulseAudio not available (optional for Bluetooth)"
fi

# Microphone permissions check for modern systems
if [ -e /dev/snd ]; then
    echo "✅ Audio devices available"
else
    echo "❌ Audio devices not accessible"
fi

echo ""

# Install missing audio packages if needed
if [ "$AUDIO_RECORD_OK" = false ] || [ "$AUDIO_PLAY_OK" = false ]; then
    echo "🔧 Installing missing audio packages..."
    sudo apt update
    sudo apt install -y alsa-utils
fi

if [ "$TTS_OK" = false ]; then
    echo "🔧 Installing text-to-speech..."
    sudo apt install -y espeak espeak-data
fi

# Check if the demo file exists
DEMO_FILE="$(dirname "$0")/voice-interaction-demo.js"
if [ ! -f "$DEMO_FILE" ]; then
    echo "❌ Voice interaction demo file not found"
    echo "   Expected location: $DEMO_FILE"
    exit 1
fi

# Check if EthervoxAI is built
DIST_DIR="$(dirname "$0")/../../dist"
if [ ! -d "$DIST_DIR" ]; then
    echo "⚠️  EthervoxAI modules not built. Building now..."
    cd "$(dirname "$0")/../.."
    npm run build
    if [ $? -ne 0 ]; then
        echo "❌ Build failed"
        exit 1
    fi
    echo "✅ Build completed"
fi

# Test audio setup
echo ""
echo "🧪 Testing audio setup..."

# Test microphone
echo "🎤 Testing microphone (3 second recording)..."
TEST_RECORDING="/tmp/ethervox_test_recording.wav"
arecord -f S16_LE -c 1 -r 16000 -d 3 "$TEST_RECORDING" 2>/dev/null

if [ -f "$TEST_RECORDING" ]; then
    # Check if file has content
    FILE_SIZE=$(stat -c%s "$TEST_RECORDING" 2>/dev/null || echo "0")
    if [ "$FILE_SIZE" -gt 1000 ]; then
        echo "✅ Microphone recording successful ($FILE_SIZE bytes)"
        
        # Test playback
        echo "🔊 Testing playback..."
        aplay "$TEST_RECORDING" 2>/dev/null &
        PLAY_PID=$!
        sleep 1
        kill $PLAY_PID 2>/dev/null
        echo "✅ Audio playback test completed"
    else
        echo "⚠️  Microphone recording too quiet (check microphone levels)"
    fi
    rm -f "$TEST_RECORDING"
else
    echo "❌ Microphone recording failed"
fi

# Test TTS
echo "🗣️  Testing text-to-speech..."
if command -v espeak &> /dev/null; then
    echo "EthervoxAI audio test" | espeak 2>/dev/null
    echo "✅ Text-to-speech test completed"
fi

echo ""
echo "🚀 Starting voice interaction demo..."
echo "   (Use Ctrl+C to stop)"
echo ""

# Start the voice interaction demo
node "$DEMO_FILE"

# Check exit code
if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Demo encountered an error"
    exit 1
else
    echo ""
    echo "✅ Demo completed successfully"
fi

echo ""
