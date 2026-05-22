#!/bin/bash
# Download Piper TTS model for neural text-to-speech
# Supports 40+ languages including English, Spanish, French, German, Chinese, Japanese, Arabic, Hindi, and more
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

# Show available voices if requested
if [ "$1" = "list" ] || [ "$1" = "--list" ] || [ "$1" = "-l" ]; then
    echo "Available Piper Voices (40+ languages):"
    echo ""
    echo "🎭 EXPRESSIVE/EMOTIONAL MODELS (Recommended for Natural Speech):"
    echo "  en_US-libritts_r-medium     ⭐ High quality, natural prosody, multi-speaker"
    echo "  en_US-libritts-high         ⭐ Very high quality, expressive"
    echo "  de_DE-thorsten_emotional-medium  Emotional German voice"
    echo ""
    echo "English (US) - Standard:"
    echo "  en_US-lessac-low, en_US-lessac-medium, en_US-lessac-high"
    echo "  en_US-amy-medium, en_US-danny-low, en_US-joe-medium"
    echo ""
    echo "English (UK): en_GB-alan-medium, en_GB-jenny_dioco-medium"
    echo "Spanish: es_ES-mls_10246-low, es_MX-ald-medium"
    echo "French: fr_FR-siwis-medium, fr_FR-tom-medium"
    echo "German: de_DE-thorsten-medium, de_DE-eva_k-medium"
    echo "Italian: it_IT-riccardo-medium"
    echo "Portuguese: pt_BR-faber-medium"
    echo "Russian: ru_RU-ruslan-medium"
    echo "Chinese (Mandarin): zh_CN-huayan-medium"
    echo "Japanese: ja_JP-natsuya_enu-medium"
    echo "Korean: ko_KR-kss-medium"
    echo "Arabic: ar_JO-kareem-medium"
    echo "Hindi: hi_IN-wavylocal-medium"
    echo ""
    echo "Full list: https://huggingface.co/rhasspy/piper-voices/tree/main"
    echo ""
    echo "Usage: $0 <voice_name>"
    echo "Example: $0 en_US-libritts_r-medium    # Expressive English"
    echo "         $0 de_DE-thorsten_emotional-medium  # Emotional German"
    echo "         $0 es_MX-ald-medium            # Spanish"
    exit 0
fi

# Model configuration
VOICE_NAME="${1:-en_US-libritts_r-medium}"  # Default to expressive LibriTTS-R model
DEFAULT_MODEL_DIR="${HOME}/.ethervox/models/piper"
if [ -z "${HOME}" ]; then
    DEFAULT_MODEL_DIR="${PROJECT_ROOT}/.ethervox/models/piper"
fi
MODEL_DIR="${2:-${DEFAULT_MODEL_DIR}}"
MODEL_FILE="${VOICE_NAME}.onnx"
CONFIG_FILE="${VOICE_NAME}.onnx.json"
MODEL_PATH="${MODEL_DIR}/${MODEL_FILE}"
CONFIG_PATH="${MODEL_DIR}/${CONFIG_FILE}"

# HuggingFace Piper voices repository
HF_REPO="rhasspy/piper-voices"
# Parse voice name to construct URL path
# Format: en_US-lessac-medium -> en/en_US/lessac/medium/
LANG_CODE=$(echo "$VOICE_NAME" | cut -d'-' -f1)     # en_US
LANG_SHORT=$(echo "$LANG_CODE" | cut -d'_' -f1)     # en
VOICE=$(echo "$VOICE_NAME" | cut -d'-' -f2)         # lessac
QUALITY=$(echo "$VOICE_NAME" | cut -d'-' -f3)       # medium

MODEL_URL="https://huggingface.co/${HF_REPO}/resolve/main/${LANG_SHORT}/${LANG_CODE}/${VOICE}/${QUALITY}/${MODEL_FILE}"
CONFIG_URL="https://huggingface.co/${HF_REPO}/resolve/main/${LANG_SHORT}/${LANG_CODE}/${VOICE}/${QUALITY}/${CONFIG_FILE}"

print_info "EthervoxAI Piper TTS Model Downloader"
print_info "Voice: ${VOICE_NAME}"
print_info "Language: ${LANG_CODE}, Quality: ${QUALITY}"
echo ""

# Create models directory
mkdir -p "${MODEL_DIR}"

# Check if model already exists
if [ -f "${MODEL_PATH}" ] && [ -f "${CONFIG_PATH}" ]; then
    MODEL_SIZE=$(du -h "${MODEL_PATH}" | cut -f1)
    CONFIG_SIZE=$(du -h "${CONFIG_PATH}" | cut -f1)
    print_success "Model already downloaded:"
    print_success "  ${MODEL_PATH} (${MODEL_SIZE})"
    print_success "  ${CONFIG_PATH} (${CONFIG_SIZE})"
    print_info "To re-download, delete the files and run this script again"
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

# Download model file
print_info "Downloading ONNX model..."
print_info "URL: ${MODEL_URL}"
if [ "$DOWNLOAD_CMD" = "curl" ]; then
    curl -L --progress-bar -o "${MODEL_PATH}" "${MODEL_URL}"
else
    wget --show-progress -O "${MODEL_PATH}" "${MODEL_URL}"
fi

# Verify model download
if [ ! -f "${MODEL_PATH}" ]; then
    print_error "Failed to download model file"
    exit 1
fi

MODEL_SIZE=$(du -h "${MODEL_PATH}" | cut -f1)
print_success "Model downloaded: ${MODEL_FILE} (${MODEL_SIZE})"

# Download config file
print_info "Downloading model config..."
print_info "URL: ${CONFIG_URL}"
if [ "$DOWNLOAD_CMD" = "curl" ]; then
    curl -L --progress-bar -o "${CONFIG_PATH}" "${CONFIG_URL}"
else
    wget --show-progress -O "${CONFIG_PATH}" "${CONFIG_URL}"
fi

# Verify config download
if [ ! -f "${CONFIG_PATH}" ]; then
    print_error "Failed to download config file"
    print_warning "Model downloaded but config is missing. TTS may not work correctly."
    exit 1
fi

CONFIG_SIZE=$(du -h "${CONFIG_PATH}" | cut -f1)
print_success "Config downloaded: ${CONFIG_FILE} (${CONFIG_SIZE})"

echo ""
print_success "Piper model downloaded successfully!"
echo ""
print_info "Model location: ${MODEL_PATH}"
print_info "Config location: ${CONFIG_PATH}"
echo ""
print_info "Usage in C code:"
echo "  ethervox_tts_config_t config = ethervox_tts_default_config();"
echo "  config.backend = ETHERVOX_TTS_BACKEND_PIPER;"
echo "  config.model_path = \"${MODEL_PATH}\";"
echo "  config.config_path = \"${CONFIG_PATH}\";"
echo ""
print_info "Or set via /settings menu: TTS Engine = 'piper', TTS Voice = '${VOICE_NAME}'"
echo ""
print_info "Multilingual tip: Piper supports 40+ languages!"
echo "  Run '$0 list' to see all available voices"
echo ""
print_info "🎭 For expressive/emotional speech, try:"
echo "  $0 en_US-libritts_r-medium         # High quality, natural prosody"
echo "  $0 en_US-libritts-high             # Very high quality"
echo "  $0 de_DE-thorsten_emotional-medium # Emotional German"
echo ""
print_info "Standard models (less expressive):"
echo "  $0 es_MX-ald-medium    # Spanish"
echo "  $0 fr_FR-siwis-medium  # French"
echo "  $0 zh_CN-huayan-medium # Chinese"
