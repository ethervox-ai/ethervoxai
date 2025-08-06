#!/bin/bash

# EthervoxAI UI Demo - Node.js Wrapper for Linux/Raspberry Pi
# Simple launcher that assumes dependencies are already installed

set -e

echo "========================================"
echo "  EthervoxAI UI Demo - Node.js Wrapper"
echo "  Linux/Raspberry Pi Edition" 
echo "========================================"
echo

# Check if we're on Raspberry Pi
if [[ "$(uname -a)" == *"raspberrypi"* ]] || [[ "$(uname -a)" == *"arm"* ]]; then
    echo "🍓 Raspberry Pi detected - applying optimizations..."
    
    # Memory optimization for RPi
    TOTAL_MEM=$(free -m | awk 'NR==2{printf "%.0f", $2}')
    if [ "$TOTAL_MEM" -lt 1024 ]; then
        export NODE_OPTIONS="--max-old-space-size=512"
        echo "   Memory optimization enabled (${TOTAL_MEM}MB available)"
    fi
fi

# Check for Node.js
if ! command -v node >/dev/null 2>&1; then
    echo "❌ ERROR: Node.js not found!"
    echo "Please install Node.js first:"
    echo "  curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -"
    echo "  sudo apt-get install -y nodejs"
    exit 1
fi

echo "Loading Node.js environment and starting UI Demo..."
echo "Node.js version: $(node --version)"
echo

# Check if demo server exists
if [ ! -f "server/demo-server.js" ]; then
    echo "❌ ERROR: Demo server not found!"
    echo "Please ensure you're in the correct directory:"
    echo "  cd src/examples/ui/demo"
    exit 1
fi

# Start the server
echo "Starting demo server..."
node server/demo-server.js
