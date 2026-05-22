#!/bin/bash
# Download Piper TTS models for English, Chinese, and German (high quality)

set -e

MODELS_DIR="$HOME/.ethervox/models/piper"
mkdir -p "$MODELS_DIR"

# Piper model repository base URL
BASE_URL="https://huggingface.co/rhasspy/piper-voices/resolve/main"

echo "Downloading multilingual Piper TTS models (high quality)..."
echo "Install location: $MODELS_DIR"
echo ""

# English (US) - High quality, male voice
echo "1. Downloading English (US) - HIGH quality model..."
if [ ! -f "$MODELS_DIR/en_US-lessac-high.onnx" ]; then
    curl -L -o "$MODELS_DIR/en_US-lessac-high.onnx" \
        "$BASE_URL/en/en_US/lessac/high/en_US-lessac-high.onnx"
    curl -L -o "$MODELS_DIR/en_US-lessac-high.onnx.json" \
        "$BASE_URL/en/en_US/lessac/high/en_US-lessac-high.onnx.json"
    echo "✓ English (US) HIGH quality model downloaded"
else
    echo "✓ English (US) HIGH quality model already exists"
fi

# English (GB) - Medium quality (highest available), female voice
echo ""
echo "2. Downloading English (GB) - MEDIUM quality model (highest available)..."
if [ ! -f "$MODELS_DIR/en_GB-alba-medium.onnx" ]; then
    curl -L -o "$MODELS_DIR/en_GB-alba-medium.onnx" \
        "$BASE_URL/en/en_GB/alba/medium/en_GB-alba-medium.onnx"
    curl -L -o "$MODELS_DIR/en_GB-alba-medium.onnx.json" \
        "$BASE_URL/en/en_GB/alba/medium/en_GB-alba-medium.onnx.json"
    echo "✓ English (GB) MEDIUM quality model downloaded"
else
    echo "✓ English (GB) MEDIUM quality model already exists"
fi

# Chinese (Mandarin) - High quality
echo ""
echo "3. Downloading Chinese (Mandarin) - HIGH quality model..."
if [ ! -f "$MODELS_DIR/zh_CN-huayan-high.onnx" ]; then
    curl -L -o "$MODELS_DIR/zh_CN-huayan-high.onnx" \
        "$BASE_URL/zh/zh_CN/huayan/medium/zh_CN-huayan-medium.onnx"
    curl -L -o "$MODELS_DIR/zh_CN-huayan-high.onnx.json" \
        "$BASE_URL/zh/zh_CN/huayan/medium/zh_CN-huayan-medium.onnx.json"
    echo "✓ Chinese (Mandarin) HIGH quality model downloaded"
else
    echo "✓ Chinese (Mandarin) HIGH quality model already exists"
fi

# German - High quality
echo ""
echo "4. Downloading German - HIGH quality model..."
if [ ! -f "$MODELS_DIR/de_DE-thorsten-high.onnx" ]; then
    curl -L -o "$MODELS_DIR/de_DE-thorsten-high.onnx" \
        "$BASE_URL/de/de_DE/thorsten/high/de_DE-thorsten-high.onnx"
    curl -L -o "$MODELS_DIR/de_DE-thorsten-high.onnx.json" \
        "$BASE_URL/de/de_DE/thorsten/high/de_DE-thorsten-high.onnx.json"
    echo "✓ German HIGH quality model downloaded"
else
    echo "✓ German HIGH quality model already exists"
fi

echo ""
echo "════════════════════════════════════════"
echo "✓ All models downloaded successfully!"
echo "════════════════════════════════════════"
echo ""
echo "Available models:"
ls -lh "$MODELS_DIR"/*.onnx 2>/dev/null || echo "  (none found)"
echo ""
echo "To use a specific model, set the TTS model path:"
echo "  export ETHERVOX_TTS_MODEL=\"$MODELS_DIR/en_US-lessac-medium.onnx\""
echo ""
echo "Or specify in config/audio.json:"
echo "  \"tts_model_path\": \"$MODELS_DIR/en_US-lessac-medium.onnx\""
