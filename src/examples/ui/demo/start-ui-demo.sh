#!/bin/bash

# EthervoxAI UI Demo - Quick Start for Linux/Raspberry Pi

echo "========================================"
echo "  EthervoxAI UI Demo"
echo "  Linux/Raspberry Pi Quick Start"
echo "========================================"
echo

# System detection
SYSTEM_INFO=$(uname -a)
if [[ "$SYSTEM_INFO" == *"raspberrypi"* ]] || [[ "$SYSTEM_INFO" == *"arm"* ]]; then
    echo "🍓 Raspberry Pi system detected"
    ARCH=$(uname -m)
    echo "Architecture: $ARCH"
    
    # Check RPi model for specific optimizations
    if [ -f /proc/device-tree/model ]; then
        RPI_MODEL=$(cat /proc/device-tree/model)
        echo "Model: $RPI_MODEL"
    fi
fi

# Check Node.js
if command -v node >/dev/null 2>&1; then
    echo "✅ Node.js version: $(node --version)"
else
    echo "❌ Node.js not found!"
    echo "Install with: sudo apt install nodejs npm"
    exit 1
fi

# Check if dependencies exist
if [ ! -d "node_modules" ]; then
    echo "⚠️  Dependencies not found. Installing..."
    npm install express cors
fi

echo
echo "🚀 Starting EthervoxAI UI Demo..."

# Apply RPi optimizations if needed
if [[ "$SYSTEM_INFO" == *"arm"* ]]; then
    # Reduce memory usage on ARM systems
    export NODE_OPTIONS="--max-old-space-size=512"
fi

# Start the server
exec node server/demo-server.js
