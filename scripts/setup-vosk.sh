#!/bin/bash
# Download and setup Vosk API for macOS

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
EXTERNAL_DIR="$PROJECT_ROOT/external"
VOSK_DIR="$EXTERNAL_DIR/vosk-api"

echo "=========================================="
echo "  Vosk API Setup for macOS"
echo "=========================================="
echo ""

# Create external directory if it doesn't exist
mkdir -p "$EXTERNAL_DIR"
cd "$EXTERNAL_DIR"

# Check if already cloned
if [ -d "$VOSK_DIR" ]; then
    echo "✓ Vosk API already exists at $VOSK_DIR"
    echo "  To reinstall, remove the directory and run again"
    exit 0
fi

# Clone Vosk API
echo "1. Cloning Vosk API from GitHub..."
git clone --depth 1 https://github.com/alphacep/vosk-api.git
cd vosk-api

echo ""
echo "2. Building Vosk for macOS..."
cd src
make

echo ""
echo "=========================================="
echo "✅ Vosk API setup complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Rebuild EthervoxAI: cmake --build build"
echo "  2. Download model: ./scripts/download-vosk-model.sh"
echo "  3. Test: ./build/test_mic_direct"
echo ""
