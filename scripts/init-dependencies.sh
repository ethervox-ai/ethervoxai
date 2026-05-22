#!/bin/bash
# init-dependencies.sh
# Manually initialize external dependencies using git submodules
# This is optional - CMake will auto-download if not present

set -e

echo "=========================================="
echo "  EthervoxAI Dependency Initialization"
echo "=========================================="
echo ""

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "❌ Error: Not in a git repository"
    echo "   Please run this script from the EthervoxAI root directory"
    exit 1
fi

# Check if .gitmodules exists
if [ ! -f ".gitmodules" ]; then
    echo "❌ Error: .gitmodules not found"
    exit 1
fi

echo "📦 Initializing git submodules..."
echo ""

# Initialize and update submodules
git submodule update --init --recursive

echo ""
echo "✅ Dependencies initialized successfully!"
echo ""
echo "📁 Downloaded to:"
if [ -d "external/llama.cpp" ]; then
    echo "   ✓ external/llama.cpp"
else
    echo "   ✗ external/llama.cpp (failed)"
fi

if [ -d "external/whisper.cpp" ]; then
    echo "   ✓ external/whisper.cpp"
else
    echo "   ✗ external/whisper.cpp (failed)"
fi

echo ""
echo "🚀 Next steps:"
echo "   mkdir build && cd build"
echo "   cmake .."
echo "   make -j\$(nproc)"
echo ""
echo "💡 Note: You can skip this script in the future."
echo "   CMake will auto-download missing dependencies."
echo ""
