#!/bin/bash
# Setup Python development environment for phonemizer training tools

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"

echo "🐍 Setting up Python development environment"
echo "=============================================="
echo ""

# Check Python version
PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
echo "Python version: $PYTHON_VERSION"

# Check if virtual environment already exists
if [ -d "$VENV_DIR" ]; then
    echo "⚠️  Virtual environment already exists at: $VENV_DIR"
    read -p "Do you want to recreate it? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🗑️  Removing existing virtual environment..."
        rm -rf "$VENV_DIR"
    else
        echo "✅ Using existing virtual environment"
        exit 0
    fi
fi

# Create virtual environment
echo "📦 Creating Python virtual environment..."
python3 -m venv "$VENV_DIR"

# Activate virtual environment
echo "🔧 Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# Upgrade pip
echo "⬆️  Upgrading pip..."
pip install --upgrade pip

# Install required packages
echo "📥 Installing required packages..."
echo "   - phonemizer (for espeak-ng integration)"
pip install phonemizer

# Verify installation
echo ""
echo "✅ Virtual environment setup complete!"
echo ""
echo "Location: $VENV_DIR"
echo "Python:   $(which python3)"
echo ""
echo "Installed packages:"
pip list | grep -E "phonemizer|pip|setuptools"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "To activate this environment manually:"
echo "  source $VENV_DIR/bin/activate"
echo ""
echo "To deactivate:"
echo "  deactivate"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Deactivate for now
deactivate
