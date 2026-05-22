# Phonemizer Training System

## Overview

EthervoxAI's phonemizer uses a **multi-tier pronunciation system** to achieve espeak-compatible quality without GPL licensing issues:

```
┌─────────────────────────────────────────────────┐
│  1. User Overrides (pronunciation_overrides)   │  ← Highest priority
│     Learned from user corrections              │
├─────────────────────────────────────────────────┤
│  2. Embedded Espeak Dictionary (espeak_dict)   │  ← NEW: Zero-config espeak compatibility
│     Pre-trained offline, compiled into binary  │
├─────────────────────────────────────────────────┤
│  3. CMU/Language Dictionaries                   │
│     Traditional pronunciation resources        │
├─────────────────────────────────────────────────┤
│  4. G2P Fallback Rules                         │  ← Lowest priority
│     Rule-based grapheme-to-phoneme            │
└─────────────────────────────────────────────────┘
```

## Legal Approach

**Espeak is used only as a development tool, NOT distributed in the software.**

- ✅ **Legal**: Use espeak-ng locally to generate pronunciation data
- ✅ **Legal**: Embed the generated data as training corpus
- ❌ **Illegal**: Link against espeak libraries in distributed software
- ❌ **Illegal**: Include espeak source code

This is analogous to training an ML model with GPL-licensed tools - the model weights aren't GPL-infected.

## Quick Start

### 1. Install espeak-ng (development only)

```bash
# macOS
brew install espeak-ng

# Ubuntu/Debian
sudo apt install espeak-ng python3-venv

# Windows
choco install espeak-ng
```

**Note:** Python virtual environment is automatically managed by the training script.

### 2. Generate Training Dictionaries

```bash
# Run the automated training tool
cd /Users/timk/repos/ethervoxai-android/ethervox_core/src/main/cpp
./tools/train_phonemizer.sh

# First run automatically creates Python venv and installs dependencies
# Subsequent runs reuse the existing environment
```

This generates:
- `src/tts/phonemizer/data/espeak_en_us.dict` - Text format dictionary
- `src/tts/phonemizer/data/espeak_dict_en_us.h` - Embedded C header

### 3. Rebuild with Embedded Dictionaries

```bash
npm run build:core
```

The phonemizer now has **zero-config espeak compatibility** out of the box!

## Training for Additional Languages

```bash
# Generate dictionary for specific language
python3 tools/generate_espeak_dict.py \
    --lang de \
    --output src/tts/phonemizer/data/espeak_de.dict \
    --format header

# Add custom vocabulary
python3 tools/generate_espeak_dict.py \
    --lang en-us \
    --vocab-file my_domain_words.txt \
    --output src/tts/phonemizer/data/espeak_en_us_custom.dict
```

## User Training Workflow

Users can still train pronunciations at runtime:

1. **User hears mispronunciation**
2. **User provides correction** via pronunciation trainer
3. **Override saved** to `~/.ethervox/pronunciation_overrides.json`
4. **High-confidence overrides promoted** to community database
5. **Stable community overrides** merged into future releases

### Exporting Learned Pronunciations

```bash
# Export community overrides for next release
./tools/export_community_overrides.sh
```

This generates `src/tts/phonemizer/data/overrides_learned.c` which gets compiled into the next version.

## Architecture Details

### Lookup Flow

```c
int phonemizer_text_to_ipa(phonemizer_t* ctx, const char* text, ...) {
    for each word:
        // 1. Check user overrides (highest priority)
        if (pronunciation_overrides_lookup(ctx->overrides, word, &override) == 0) {
            use override.ipa;
            record_usage(word);
            continue;
        }
        
        // 2. Check embedded espeak dictionary
        #ifdef ESPEAK_DICT_EN_US_ENABLED
        if (espeak_dict_lookup(espeak_dict_en_us, size, word, ipa, ...) == 0) {
            use espeak ipa;
            continue;
        }
        #endif
        
        // 3. Check CMU dictionary
        if (dict_lookup(ctx->dictionary, word, arpabet, ...) == 0) {
            convert arpabet to ipa;
            continue;
        }
        
        // 4. Fallback to G2P rules
        apply_g2p_rules(word, ipa);
}
```

### File Structure

```
src/tts/phonemizer/
├── phonemizer.c              # Main implementation
├── pronunciation_overrides.c  # User training system
├── espeak_dict.c             # Embedded dictionary lookup (binary search)
├── dictionary.c              # CMU dictionary loader
├── rules_en.c                # G2P fallback rules
└── data/
    ├── espeak_en_us.dict     # Generated: Text format
    ├── espeak_dict_en_us.h   # Generated: Embedded C header
    ├── espeak_dict_en_gb.h
    ├── espeak_dict_de.h
    ├── cmudict-0.7b.txt      # Traditional CMU dictionary
    └── overrides_learned.c   # Promoted community overrides

tools/
├── setup_python_env.sh       # Setup Python venv (manual)
├── train_phonemizer.sh       # Main training script (auto-setup)
├── generate_espeak_dict.py   # Dictionary generator
└── validate_espeak_dict.py   # Accuracy validator

tools/venv/                   # Python virtual environment (git-ignored)
└── ...
```

## Performance

### Dictionary Sizes (Estimated)

| Language | Entries | Binary Size | Lookup Speed |
|----------|---------|-------------|--------------|
| en-us    | ~134K   | ~5-8 MB     | O(log n)     |
| en-gb    | ~134K   | ~5-8 MB     | O(log n)     |
| de       | ~50K    | ~2-3 MB     | O(log n)     |

### Memory Optimization

For embedded devices (ESP32), dictionaries can be:
- **Trimmed** to common words only (reduces to ~50KB per language)
- **Compressed** using LZ4 or zlib
- **Memory-mapped** from flash storage

See `cmake/esp32-toolchain.cmake` for conditional compilation flags.

## Testing

### Accuracy Testing

Compare phonemizer output against espeak:

```bash
# Test English
npm run test:phonemizer -- --lang en-us --compare-espeak

# Test German
npm run test:phonemizer -- --lang de --compare-espeak
```

### Performance Testing

```bash
# Benchmark lookup speed
npm run test:phonemizer -- --benchmark

# Expected results:
#   User override:      ~100-500 ns
#   Embedded dict:      ~1-5 μs (binary search)
#   CMU dict:           ~2-10 μs (hash table)
#   G2P fallback:       ~50-200 μs
```

## Continuous Improvement

### Community Override Promotion

Overrides are automatically promoted based on:
- **Usage count** ≥ 50
- **Confidence** ≥ 0.85
- **Multiple users** reporting same correction

### Release Cycle

1. **Development**: Users train corrections locally
2. **Beta**: Community overrides shared and validated
3. **Release**: Stable overrides compiled into espeak_dict headers
4. **Update**: Users get improved pronunciations automatically

## Troubleshooting

### "espeak-ng not found"

Espeak is only needed for **development/training**, not runtime:

```bash
# Install for development
brew install espeak-ng  # macOS
sudo apt install espeak-ng  # Linux
```

### "Dictionary not loaded"

Check that headers are included at compile time:

```bash
# Verify espeak dict is enabled
grep -r "ESPEAK_DICT.*ENABLED" build/

# Rebuild
npm run clean
npm run build:core
```

### "Pronunciation still incorrect"

1. **Add user override**: Use pronunciation trainer UI
2. **Check espeak output**: `espeak-ng -v en-us -x "word"` 
3. **Report issue**: Add to `pronunciation_issues.md`

## References

- [CMU Pronouncing Dictionary](http://www.speech.cs.cmu.edu/cgi-bin/cmudict)
- [Espeak-ng Documentation](https://github.com/espeak-ng/espeak-ng/blob/master/docs/index.md)
- [IPA Specification](https://en.wikipedia.org/wiki/International_Phonetic_Alphabet)
- [Piper TTS Phoneme Format](https://github.com/rhasspy/piper/blob/master/TRAINING.md)
