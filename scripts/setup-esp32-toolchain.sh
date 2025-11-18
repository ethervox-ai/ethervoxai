#!/usr/bin/env bash
# Setup ESP32 development environment
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=========================================="
echo "  ESP32 Development Setup"
echo "=========================================="
echo "Project root: ${PROJECT_ROOT}"
echo ""

# Check if ESP-IDF is installed
if [ ! -d "$HOME/esp/esp-idf" ]; then
    echo "ESP-IDF not found. Installing..."
    
    # Install prerequisites
    sudo apt-get update
    sudo apt-get install -y git wget flex bison gperf python3 python3-pip \
        python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
    
    # Clone ESP-IDF
    mkdir -p ~/esp
    cd ~/esp
    git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
    
    # Install toolchain
    cd esp-idf
    ./install.sh esp32,esp32s3,esp32c3
    
    echo ""
    echo "ESP-IDF installed successfully!"
else
    echo "ESP-IDF already installed at ~/esp/esp-idf"
fi

# Check if environment is set up
if [ -z "${IDF_PATH:-}" ]; then
    echo ""
    echo "Setting up ESP-IDF environment..."
    . $HOME/esp/esp-idf/export.sh
fi

# Create ESP32 project structure
mkdir -p esp32-project
cd esp32-project

# Create symlinks to source files
ln -s ../src src
ln -s ../include include

# Create main component
mkdir main


# Verify installation
echo ""
echo "Verifying ESP-IDF installation..."
idf.py --version

echo ""
echo "=========================================="
echo "  ESP32 Setup Complete!"
echo "=========================================="
echo ""
echo "ESP-IDF Path: $IDF_PATH"
echo "Python: $(which python)"
echo ""
echo "Next steps:"
echo "  1. In each new terminal, run: . ~/esp/esp-idf/export.sh"
echo "  2. Or add to ~/.bashrc: alias get_idf='. ~/esp/esp-idf/export.sh'"
echo "  3. Navigate to your project: cd ${PROJECT_ROOT}"
echo "  4. Set target: idf.py set-target esp32s3"
echo "  5. Configure: idf.py menuconfig"
echo "  6. Build: idf.py build"
echo "  7. Flash: idf.py -p /dev/ttyUSB0 flash monitor"
echo ""
echo "For WSL USB access, see: https://github.com/dorssel/usbipd-win"
echo ""