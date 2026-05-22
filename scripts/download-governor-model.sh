#!/bin/bash
# Download Qwen2.5-3B-Instruct quantized model for Governor
# SPDX-License-Identifier: CC-BY-NC-SA-4.0

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Default model configuration
MODEL_NAME="Qwen2.5-3B-Instruct"
QUANTIZATION="Q4_K_M"  # 4-bit quantization, good balance
MODEL_DIR="${PROJECT_ROOT}/models"
MODEL_FILE="${MODEL_NAME}-${QUANTIZATION}.gguf"
MODEL_PATH="${MODEL_DIR}/${MODEL_FILE}"

# HuggingFace repository (converted GGUF models)
HF_REPO="bartowski/Qwen2.5-3B-Instruct-GGUF"
HF_FILE="${MODEL_NAME}-${QUANTIZATION}.gguf"
DOWNLOAD_URL="https://huggingface.co/${HF_REPO}/resolve/main/${HF_FILE}"

print_info "EthervoxAI Governor Model Downloader"
print_info "Model: ${MODEL_NAME} (${QUANTIZATION})"
echo ""

# Create models directory
mkdir -p "${MODEL_DIR}"

# Check if model already exists
if [ -f "${MODEL_PATH}" ]; then
    SIZE=$(du -h "${MODEL_PATH}" | cut -f1)
    print_success "Model already downloaded: ${MODEL_PATH} (${SIZE})"
    print_info "To re-download, delete the file and run this script again"
    exit 0
fi

# Check for download tools
if command -v curl &> /dev/null; then
    DOWNLOADER="curl"
    print_info "Using curl for download"
elif command -v wget &> /dev/null; then
    DOWNLOADER="wget"
    print_info "Using wget for download"
else
    print_error "Neither curl nor wget found. Please install one of them:"
    echo "  macOS:   brew install curl"
    echo "  Ubuntu:  sudo apt-get install curl"
    echo "  Fedora:  sudo dnf install curl"
    exit 1
fi

# Download model
print_info "Downloading from: ${DOWNLOAD_URL}"
print_info "Destination: ${MODEL_PATH}"
print_warning "This will download ~2GB. Please wait..."
echo ""

if [ "$DOWNLOADER" = "curl" ]; then
    curl -L -C - --progress-bar "${DOWNLOAD_URL}" -o "${MODEL_PATH}"
else
    wget -c --show-progress "${DOWNLOAD_URL}" -O "${MODEL_PATH}"
fi

# Verify download
if [ -f "${MODEL_PATH}" ]; then
    SIZE=$(du -h "${MODEL_PATH}" | cut -f1)
    print_success "Download complete: ${MODEL_PATH} (${SIZE})"
    echo ""
    print_info "To use this model with Governor:"
    echo "  ethervox_governor_load_model(governor, \"${MODEL_PATH}\");"
    echo ""
    print_info "Model specifications:"
    echo "  - Name: Qwen2.5-3B-Instruct"
    echo "  - Quantization: ${QUANTIZATION} (4-bit)"
    echo "  - Context: 8K tokens"
    echo "  - Optimized for: Tool calling, instruction following"
    echo "  - RAM usage: ~2-3GB"
else
    print_error "Download failed. Please check your internet connection and try again."
    exit 1
fi
