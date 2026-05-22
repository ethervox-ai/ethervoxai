#!/bin/bash
# Train phonemizer using espeak-ng (development tool only, not distributed)

set -e

# Parse arguments
SKIP_EXISTING=false
if [ "$1" = "--skip-existing" ]; then
    SKIP_EXISTING=true
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DATA_DIR="$PROJECT_ROOT/src/tts/phonemizer/data"
VENV_DIR="$SCRIPT_DIR/venv"

echo "🎓 EthervoxAI Phonemizer Training Tool"
echo "========================================"
echo ""
echo "This tool uses espeak-ng locally to generate training data."
echo "espeak is NOT included in the distributed software."
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

# Setup Python virtual environment
if [ ! -d "$VENV_DIR" ]; then
    echo "📦 Creating Python virtual environment..."
    python3 -m venv "$VENV_DIR"
    echo "✅ Virtual environment created at: $VENV_DIR"
fi

# Activate virtual environment
echo "🔧 Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# Install/upgrade required packages
echo "📥 Installing Python dependencies..."
pip install --upgrade pip --quiet
pip install phonemizer --quiet

echo "✅ Python environment ready"
echo ""

mkdir -p "$DATA_DIR"

# Auto-detect installed Piper models and their language variants
echo ""
echo "🔍 Scanning for installed Piper models..."
echo ""

MODELS_DIR="$HOME/.ethervox/models/piper"
if [ ! -d "$MODELS_DIR" ]; then
    echo "⚠️  No Piper models directory found at: $MODELS_DIR"
    echo "   Please install Piper models first or models will use fallback pronunciation."
    echo ""
    exit 0
fi

# Collect unique variants using a temporary file (bash 3.2 compatible)
VARIANTS_FILE=$(mktemp)
MODELS_INFO_FILE=$(mktemp)
trap "rm -f $VARIANTS_FILE $MODELS_INFO_FILE" EXIT

for config_file in "$MODELS_DIR"/*.onnx.json; do
    if [ ! -f "$config_file" ]; then
        continue
    fi
    
    model_name=$(basename "$config_file" .onnx.json)
    
    # Extract language variant from JSON (phonemes.language field)
    variant=$(python3 -c "
import json, sys
try:
    with open('$config_file') as f:
        config = json.load(f)
        lang = config.get('phonemes', {}).get('language')
        if lang and lang != 'None':
            print(lang)
except:
    pass
" 2>/dev/null)
    
    if [ -n "$variant" ]; then
        echo "$variant" >> "$VARIANTS_FILE"
        echo "$variant|$model_name" >> "$MODELS_INFO_FILE"
    fi
done

# Get unique variants
UNIQUE_VARIANTS=$(sort -u "$VARIANTS_FILE")

if [ -z "$UNIQUE_VARIANTS" ]; then
    echo "⚠️  No models with espeak language variants found."
    echo "   Your models may use default pronunciation without variant-specific dictionaries."
    echo ""
    exit 0
fi

# Count and display variants
VARIANT_COUNT=$(echo "$UNIQUE_VARIANTS" | wc -l | tr -d ' ')
echo "Found $VARIANT_COUNT unique language variant(s) required:"
echo ""

for variant in $UNIQUE_VARIANTS; do
    echo "  📌 $variant"
    # Find all models using this variant
    models=$(grep "^$variant|" "$MODELS_INFO_FILE" | cut -d'|' -f2 | tr '\n' ',' | sed 's/,$//' | sed 's/,/, /g')
    echo "     Models: $models"
done
echo ""

# Map espeak variants to our dictionary naming scheme
map_variant_to_dict_name() {
    case "$1" in
        "en-us") echo "en-us" ;;
        "en-gb-x-rp") echo "en-gb-rp" ;;
        "de") echo "de" ;;
        "cmn") echo "cmn" ;;
        "es-419") echo "es-419" ;;
        *) echo "$1" ;;  # Unknown variant, use as-is
    esac
}

# Check which dictionaries need to be generated
TO_GENERATE_FILE=$(mktemp)
TO_SKIP_FILE=$(mktemp)
trap "rm -f $VARIANTS_FILE $MODELS_INFO_FILE $TO_GENERATE_FILE $TO_SKIP_FILE" EXIT

for variant in $UNIQUE_VARIANTS; do
    # Map variant to dictionary name
    dict_name=$(map_variant_to_dict_name "$variant")
    
    dict_file="$DATA_DIR/espeak_${dict_name//-/_}.dict"
    header_file="$DATA_DIR/espeak_dict_${dict_name//-/_}.h"
    
    if [ -f "$dict_file" ] && [ -f "$header_file" ]; then
        if [ "$SKIP_EXISTING" = true ]; then
            echo "$dict_name" >> "$TO_SKIP_FILE"
            echo "⏭️  Dictionary for $variant already exists - skipping (--skip-existing mode)"
        else
            # Interactive mode - check if running in a terminal
            if [ -t 0 ]; then
                echo -n "📝 Dictionary for $variant already exists. Overwrite? [y/N] "
                read -r REPLY
                if [[ $REPLY =~ ^[Yy]$ ]]; then
                    echo "$dict_name" >> "$TO_GENERATE_FILE"
                    echo "   ✅ Will regenerate $variant"
                else
                    echo "$dict_name" >> "$TO_SKIP_FILE"
                    echo "   ⏭️  Skipping $variant"
                fi
            else
                # Non-interactive (piped) - skip by default
                echo "$dict_name" >> "$TO_SKIP_FILE"
                echo "⏭️  Dictionary for $variant already exists - skipping (use --skip-existing to auto-skip)"
            fi
        fi
    else
        echo "$dict_name" >> "$TO_GENERATE_FILE"
        echo "✅ Will generate $variant (not found)"
    fi
done

# Check if there's anything to generate
if [ ! -s "$TO_GENERATE_FILE" ]; then
    echo ""
    echo "✅ All required dictionaries already exist. Nothing to do!"
    echo ""
    exit 0
fi

# Generate dictionaries
DICT_COUNT=$(wc -l < "$TO_GENERATE_FILE" | tr -d ' ')
echo ""
echo "📚 Generating $DICT_COUNT pronunciation dictionar(ies)..."
echo "   Note: Each language takes ~5-10 minutes for ~126K words"
echo "   Progress will be shown for each dictionary below..."
echo ""

CURRENT=0
while IFS= read -r dict_name; do
    CURRENT=$((CURRENT + 1))
    
    # Map back to display name
    case "$dict_name" in
        "en-us") display_name="English (US) [en-us]" ;;
        "en-gb-rp") display_name="English (GB-RP) [en-gb-x-rp]" ;;
        "de") display_name="German [de]" ;;
        "cmn") display_name="Mandarin Chinese [cmn]" ;;
        "es-419") display_name="Spanish (Latin American) [es-419]" ;;
        *) display_name="$dict_name" ;;
    esac
    
    echo ""
    echo "[$CURRENT/$DICT_COUNT] ▶ $display_name..."
    echo "Starting generation at $(date '+%H:%M:%S')..."
    
    dict_file="$DATA_DIR/espeak_${dict_name//-/_}.dict"
    header_file="$DATA_DIR/espeak_dict_${dict_name//-/_}.h"
    
    # Generate dictionary (this will show progress internally)
    python3 "$SCRIPT_DIR/generate_espeak_dict.py" \
        --lang "$dict_name" \
        --output "$dict_file" \
        --format dict
    
    # Generate C header
    echo ""
    echo "  📝 Generating C header for $dict_name..."
    python3 "$SCRIPT_DIR/generate_espeak_dict.py" \
        --lang "$dict_name" \
        --output "$header_file" \
        --format header \
        --dict-file "$dict_file"
    
    echo "  ✅ Completed $dict_name at $(date '+%H:%M:%S')"
done < "$TO_GENERATE_FILE"

echo ""
echo "✅ Training complete!"
echo ""
echo "Generated dictionaries:"
while IFS= read -r dict_name; do
    dict_file="$DATA_DIR/espeak_${dict_name//-/_}.dict"
    if [ -f "$dict_file" ]; then
        size=$(du -h "$dict_file" | awk '{print $1}')
        echo "  ✅ $dict_name ($size)"
    fi
done < "$TO_GENERATE_FILE"

if [ -s "$TO_SKIP_FILE" ]; then
    echo ""
    echo "Skipped (already exist):"
    while IFS= read -r dict_name; do
        echo "  ⏭️  $dict_name"
    done < "$TO_SKIP_FILE"

if [ ${#TO_SKIP[@]} -gt 0 ]; then
    echo ""
    echo "Skipped (already exist):"
    for dict_name in "${TO_SKIP[@]}"; do
        echo "  ⏭️  $dict_name"
    done
fi

echo ""
echo "Next steps:"
echo "  1. Rebuild with: cmake --preset default -DENABLE_ESPEAK_DICT=ON"
echo "  2. Then: npm run build:core"
echo "  3. Dictionaries will be automatically matched to models by language variant"
echo ""
echo "Tip: Use --skip-existing flag to skip existing dictionaries without prompting"
echo ""
echo "Generated files:"
ls -lh "$DATA_DIR"/espeak_* 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}' || echo "  (No files generated)"
echo ""
echo "Next steps:"
echo "  1. Review generated dictionaries in: $DATA_DIR"
echo "  2. Rebuild phonemizer with: npm run build:core"
echo "  3. Test pronunciation accuracy against espeak output"
echo ""
echo "The phonemizer will now use espeak-trained pronunciations!"

# Deactivate virtual environment
deactivate
