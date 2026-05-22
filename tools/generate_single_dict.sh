#!/bin/bash
# Generate a single espeak dictionary for a specific language variant
# Usage: ./tools/generate_single_dict.sh <variant>
#   where <variant> is: en-us, en-gb-rp, de, cmn, or es-419

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <variant>"
    echo ""
    echo "Available variants:"
    echo "  en-us      - US English (4 models)"
    echo "  en-gb-rp   - British RP (1 model: Alba)"
    echo "  de         - German (2 models)"
    echo "  cmn        - Mandarin Chinese (2 models)"
    echo "  es-419     - Latin American Spanish (1 model)"
    echo ""
    exit 1
fi

VARIANT="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DATA_DIR="$PROJECT_ROOT/src/tts/phonemizer/data"

# Validate variant
case "$VARIANT" in
    en-us|en-gb-rp|de|cmn|es-419)
        ;;
    *)
        echo "❌ Invalid variant: $VARIANT"
        echo "Valid variants: en-us, en-gb-rp, de, cmn, es-419"
        exit 1
        ;;
esac

echo "🎓 Generating Espeak Dictionary for $VARIANT"
echo "================================================"
echo ""

# Check for espeak-ng
if ! command -v espeak-ng &> /dev/null; then
    echo "❌ espeak-ng not found!"
    echo ""
    echo "Install instructions:"
    echo "  macOS:   brew install espeak-ng"
    echo "  Ubuntu:  sudo apt install espeak-ng"
    echo "  Windows: choco install espeak-ng"
    echo ""
    exit 1
fi

mkdir -p "$DATA_DIR"

# Generate dictionary
DICT_FILE="$DATA_DIR/espeak_${VARIANT//-/_}.dict"
HEADER_FILE="$DATA_DIR/espeak_dict_${VARIANT//-/_}.h"

echo "▶ Generating pronunciation dictionary..."
echo "  Output: $DICT_FILE"
echo "  This will take ~5-10 minutes for ~126K words..."
echo ""

python3 "$SCRIPT_DIR/generate_espeak_dict.py" \
    --lang "$VARIANT" \
    --output "$DICT_FILE" \
    --format dict

echo ""
echo "▶ Generating C header file..."
echo "  Output: $HEADER_FILE"
echo ""

python3 "$SCRIPT_DIR/generate_espeak_dict.py" \
    --lang "$VARIANT" \
    --output "$HEADER_FILE" \
    --format header \
    --dict-file "$DICT_FILE"

echo ""
echo "✅ Dictionary generation complete!"
echo ""
echo "Next steps:"
echo "  1. Rebuild with: npm run build:core"
echo "  2. Enable in CMake: -DENABLE_ESPEAK_DICT=ON"
echo "  3. The dictionary will be automatically used for models with language=$VARIANT"
echo ""
