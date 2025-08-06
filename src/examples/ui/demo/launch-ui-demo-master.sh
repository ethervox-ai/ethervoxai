#!/bin/bash

# EthervoxAI UI Demo - Master Launcher for Linux/Raspberry Pi
# Cross-platform launcher with automatic dependency management

set -e  # Exit on any error

echo "========================================="
echo "  EthervoxAI UI Demo - Master Launcher"
echo "  Linux/Raspberry Pi Edition"
echo "========================================="
echo

# Detect system information
SYSTEM_INFO=$(uname -a)
if [[ "$SYSTEM_INFO" == *"raspberrypi"* ]] || [[ "$SYSTEM_INFO" == *"arm"* ]]; then
    echo "🍓 Raspberry Pi system detected"
    IS_RPI=true
else
    echo "🐧 Linux system detected"
    IS_RPI=false
fi

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for Node.js installation
echo "Checking Node.js installation..."
if command_exists node; then
    NODE_VERSION=$(node --version)
    echo "✅ Node.js found: $NODE_VERSION"
    
    # Check Node.js version compatibility
    NODE_MAJOR=$(echo $NODE_VERSION | cut -d'.' -f1 | sed 's/v//')
    if [ "$NODE_MAJOR" -lt 14 ]; then
        echo "⚠️  Warning: Node.js version $NODE_VERSION is older than recommended (v14+)"
        if [ "$IS_RPI" = true ]; then
            echo "   For Raspberry Pi, consider upgrading Node.js for better performance"
        fi
    fi
else
    echo "❌ Node.js not found!"
    echo
    echo "Please install Node.js first:"
    if [ "$IS_RPI" = true ]; then
        echo "  # For Raspberry Pi:"
        echo "  curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -"
        echo "  sudo apt-get install -y nodejs"
    else
        echo "  # For Ubuntu/Debian:"
        echo "  curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -"
        echo "  sudo apt-get install -y nodejs"
        echo
        echo "  # Or use your package manager:"
        echo "  sudo apt install nodejs npm"
    fi
    echo
    exit 1
fi

# Check for npm
if command_exists npm; then
    NPM_VERSION=$(npm --version)
    echo "✅ npm found: $NPM_VERSION"
else
    echo "❌ npm not found! Please install npm alongside Node.js"
    exit 1
fi

# Raspberry Pi specific optimizations
if [ "$IS_RPI" = true ]; then
    echo
    echo "🔧 Applying Raspberry Pi optimizations..."
    
    # Check available memory
    TOTAL_MEM=$(free -m | awk 'NR==2{printf "%.0f", $2}')
    echo "   Available memory: ${TOTAL_MEM}MB"
    
    if [ "$TOTAL_MEM" -lt 1024 ]; then
        echo "   ⚠️  Low memory detected - enabling lightweight mode"
        export NODE_OPTIONS="--max-old-space-size=512"
    fi
    
    # Check if we're on ARM64 or ARM32
    ARCH=$(uname -m)
    echo "   Architecture: $ARCH"
    
    if [[ "$ARCH" == "armv6l" ]]; then
        echo "   📱 Raspberry Pi Zero/1 detected - using minimal configuration"
        export ETHERVOX_LIGHTWEIGHT=true
    fi
fi

echo
echo "Installing/updating dependencies..."

# Check if package.json exists, create minimal one if not
if [ ! -f "package.json" ]; then
    echo "Creating minimal package.json for demo..."
    cat > package.json << EOF
{
  "name": "ethervoxai-ui-demo",
  "version": "1.0.0",
  "description": "EthervoxAI UI Demo Server",
  "main": "server/demo-server.js",
  "scripts": {
    "start": "node server/demo-server.js",
    "demo:ui": "node server/demo-server.js"
  }
}
EOF
fi

# Install dependencies with platform-specific optimizations
if [ "$IS_RPI" = true ]; then
    echo "Installing dependencies with Raspberry Pi optimizations..."
    # Use fewer concurrent jobs to avoid memory issues
    npm install express cors --production --no-optional --maxsockets=1
else
    echo "Installing dependencies..."
    npm install express cors --production
fi

echo
echo "✅ Dependencies installed successfully!"
echo

# Start the demo server
echo "Starting EthervoxAI UI Demo Server..."
echo "Press Ctrl+C to stop the server"
echo

# Use different startup options for RPi
if [ "$IS_RPI" = true ]; then
    echo "🍓 Starting in Raspberry Pi mode..."
    NODE_ENV=production node server/demo-server.js
else
    echo "🐧 Starting in Linux mode..."
    node server/demo-server.js
fi
