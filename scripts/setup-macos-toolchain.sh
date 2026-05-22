#!/usr/bin/env bash
# Setup macOS development toolchain for EthervoxAI
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXTERNAL_DIR="${PROJECT_ROOT}/external"

echo "=========================================="
echo "  macOS Development Setup for EthervoxAI"
echo "=========================================="
echo "Project root: ${PROJECT_ROOT}"
echo ""

# Check if running on macOS
if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Error: This script must be run on macOS"
    exit 1
fi

# Check for Homebrew
if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    
    # Add Homebrew to PATH for Apple Silicon Macs
    if [[ $(uname -m) == "arm64" ]]; then
        echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/opt/homebrew/bin/brew shellenv)"
    fi
else
    echo "✓ Homebrew already installed"
fi

# Update Homebrew
echo ""
echo "Updating Homebrew..."
brew update

# Install build tools
echo ""
echo "Installing build tools..."
brew install cmake ninja pkg-config

# Install Xcode Command Line Tools if not present
if ! xcode-select -p &> /dev/null; then
    echo ""
    echo "Installing Xcode Command Line Tools..."
    xcode-select --install
    echo "Please complete the Xcode Command Line Tools installation and run this script again."
    exit 0
else
    echo "✓ Xcode Command Line Tools already installed"
fi

# Install development libraries
echo ""
echo "Installing development libraries..."
brew install openssl curl libomp

# Install Node.js for dashboard (optional)
if ! command -v node &> /dev/null; then
    echo ""
    echo "Installing Node.js for dashboard..."
    brew install node
else
    echo "✓ Node.js already installed: $(node --version)"
fi

# Clone llama.cpp if not present
echo ""
echo "Setting up llama.cpp..."
mkdir -p "${EXTERNAL_DIR}"

if [ ! -d "${EXTERNAL_DIR}/llama.cpp" ]; then
    echo "Cloning llama.cpp..."
    cd "${EXTERNAL_DIR}"
    git clone https://github.com/ggerganov/llama.cpp.git
    echo "✓ llama.cpp cloned successfully"
else
    echo "✓ llama.cpp already present"
    echo "Updating llama.cpp..."
    cd "${EXTERNAL_DIR}/llama.cpp"
    git pull origin master || echo "Note: Could not update llama.cpp (possibly local changes)"
fi

# Verify installations
echo ""
echo "=========================================="
echo "  Verification"
echo "=========================================="
echo "CMake: $(cmake --version | head -n1)"
echo "Make: $(make --version | head -n1)"
echo "Clang: $(clang --version | head -n1)"
echo "Node.js: $(node --version 2>/dev/null || echo 'Not installed')"
echo "npm: $(npm --version 2>/dev/null || echo 'Not installed')"
echo ""

# Create build directory
echo "Creating build directory..."
mkdir -p "${PROJECT_ROOT}/build"

echo ""
echo "=========================================="
echo "  macOS Toolchain Setup Complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Navigate to project root: cd ${PROJECT_ROOT}"
echo "  2. Configure build with OpenMP support:"
echo "     cmake -DOpenMP_C_FLAGS=\"-Xpreprocessor -fopenmp -I\$(brew --prefix libomp)/include\" \\"
echo "           -DOpenMP_C_LIB_NAMES=\"omp\" \\"
echo "           -DOpenMP_CXX_FLAGS=\"-Xpreprocessor -fopenmp -I\$(brew --prefix libomp)/include\" \\"
echo "           -DOpenMP_CXX_LIB_NAMES=\"omp\" \\"
echo "           -DOpenMP_omp_LIBRARY=\$(brew --prefix libomp)/lib/libomp.dylib ."
echo "  3. Build project: make -j\$(sysctl -n hw.ncpu)"
echo "  4. (Optional) Build dashboard: cd dashboard && npm install && npm run build"
echo ""
echo "Note: Metal GPU acceleration is automatically enabled on Apple Silicon"
echo ""
