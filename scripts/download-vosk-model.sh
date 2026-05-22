#!/bin/bash
# Download Vosk model for real-time conversation

set -e

MODELS_DIR="$HOME/.ethervox/models/vosk"
MODEL_NAME="vosk-model-small-en-us-0.15"
MODEL_URL="https://alphacephei.com/vosk/models/${MODEL_NAME}.zip"

echo "🎤 Vosk Model Downloader for EthervoxAI"
echo "========================================"
echo ""

# Create models directory
mkdir -p "$MODELS_DIR"

# Check if model already exists
if [ -d "$MODELS_DIR/$MODEL_NAME" ]; then
    echo "✓ Model already exists: $MODELS_DIR/$MODEL_NAME"
    echo ""
    echo "Model info:"
    du -sh "$MODELS_DIR/$MODEL_NAME"
    exit 0
fi

echo "Downloading Vosk model: $MODEL_NAME"
echo "Size: ~40MB"
echo "URL: $MODEL_URL"
echo ""

# Download model
cd "$MODELS_DIR"
echo "Downloading to: $MODELS_DIR"
curl -L -o "${MODEL_NAME}.zip" "$MODEL_URL"

echo ""
echo "Extracting model..."
unzip -q "${MODEL_NAME}.zip"

echo "Cleaning up..."
rm "${MODEL_NAME}.zip"

echo ""
echo "✓ Model downloaded successfully!"
echo ""
echo "Location: $MODELS_DIR/$MODEL_NAME"
echo "Size: $(du -sh "$MODELS_DIR/$MODEL_NAME" | cut -f1)"
echo ""
echo "You can now use Vosk STT backend with:"
echo "  config.backend = ETHERVOX_STT_BACKEND_VOSK;"
echo "  config.model_path = \"$MODELS_DIR/$MODEL_NAME\";"
