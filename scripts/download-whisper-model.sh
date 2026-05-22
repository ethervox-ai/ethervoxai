#!/bin/bash
# Download Whisper base (multilingual) model for STT
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

# Model configuration
MODEL_NAME="${1:-base}"  # Default to base (multilingual, 141MB)
DEFAULT_MODEL_DIR="${HOME}/.ethervox/models/whisper"
if [ -z "${HOME}" ]; then
    DEFAULT_MODEL_DIR="${PROJECT_ROOT}/.ethervox/models/whisper"
fi
MODEL_DIR="${2:-${DEFAULT_MODEL_DIR}}"
MODEL_FILE="${MODEL_NAME}.bin"
MODEL_PATH="${MODEL_DIR}/${MODEL_FILE}"

# HuggingFace repository with pre-converted GGML models
HF_REPO="ggerganov/whisper.cpp"
DOWNLOAD_URL="https://huggingface.co/${HF_REPO}/resolve/main/ggml-${MODEL_NAME}.bin"

print_info "EthervoxAI Whisper Model Downloader"
print_info "Model: ${MODEL_NAME} (~141 MB for base/base.en)"
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
    DOWNLOAD_CMD="curl"
    print_info "Using curl for download"
elif command -v wget &> /dev/null; then
    DOWNLOAD_CMD="wget"
    print_info "Using wget for download"
else
    print_error "Neither curl nor wget found. Please install one of them."
    exit 1
fi

# Download model
print_info "Downloading ${MODEL_NAME} model from HuggingFace..."
print_info "URL: ${DOWNLOAD_URL}"
print_info "Destination: ${MODEL_PATH}"
echo ""

if [ "$DOWNLOAD_CMD" = "curl" ]; then
    curl -L \
        --progress-bar \
        --create-dirs \
        -o "${MODEL_PATH}.tmp" \
        "${DOWNLOAD_URL}"
else
    wget \
        --show-progress \
        --progress=bar:force:noscroll \
        -O "${MODEL_PATH}.tmp" \
        "${DOWNLOAD_URL}"
fi

# Verify download
if [ ! -f "${MODEL_PATH}.tmp" ]; then
    print_error "Download failed - file not created"
    exit 1
fi

# Check file size (base/base.en should be ~141 MB)
SIZE_BYTES=$(stat -f%z "${MODEL_PATH}.tmp" 2>/dev/null || stat -c%s "${MODEL_PATH}.tmp" 2>/dev/null)
SIZE_MB=$((SIZE_BYTES / 1024 / 1024))

if [ "$SIZE_MB" -lt 100 ]; then
    print_error "Downloaded file is too small (${SIZE_MB} MB). Download may have failed."
    rm -f "${MODEL_PATH}.tmp"
    exit 1
fi

# Move to final location
mv "${MODEL_PATH}.tmp" "${MODEL_PATH}"

SIZE=$(du -h "${MODEL_PATH}" | cut -f1)
print_success "Download complete: ${MODEL_PATH} (${SIZE})"
echo ""
print_info "Available Whisper models:"
print_info "  - tiny.en    (~75 MB)   - Fastest, lowest accuracy, English-only"
print_info "  - base       (~141 MB)  - Multilingual, good for most uses (DEFAULT)"
print_info "  - base.en    (~141 MB)  - Good balance, English-only"
print_info "  - small.en   (~466 MB)  - Better accuracy, English-only"
print_info "  - medium.en  (~1.5 GB)  - High accuracy, English-only"
print_info "  - tiny       (~75 MB)   - Multilingual, 99 languages"
print_info "  - base       (~141 MB)  - Multilingual, good for most uses"
print_info "  - small      (~466 MB)  - Multilingual, better accuracy"
echo ""
print_info "To download a different model:"
print_info "  ./scripts/download-whisper-model.sh <model_name>"
print_info "Example: ./scripts/download-whisper-model.sh small"
