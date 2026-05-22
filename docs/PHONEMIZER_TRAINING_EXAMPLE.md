# Phonemizer Training - Quick Example

This example shows how to train the phonemizer using espeak locally.

## Step-by-Step Example

### 1. Initial Setup (One-time)

```bash
# Install espeak-ng (development tool, not distributed)
brew install espeak-ng              # macOS
# or
sudo apt install espeak-ng          # Linux

# Setup Python virtual environment (one-time)
cd /Users/timk/repos/ethervoxai-android/ethervox_core/src/main/cpp
./tools/setup_python_env.sh

# This creates tools/venv/ with all required packages
# The venv/ directory is automatically excluded from git
```

### 2. Generate Training Dictionaries

```bash
cd /Users/timk/repos/ethervoxai-android/ethervox_core/src/main/cpp

# Run automated training (generates all languages)
./tools/train_phonemizer.sh
```

**Output:**
```
🎓 EthervoxAI Phonemizer Training Tool
========================================

� Creating Python virtual environment...
✅ Virtual environment created at: tools/venv
🔧 Activating virtual environment...
📥 Installing Python dependencies...
✅ Python environment ready

�📚 Generating pronunciation dictionaries...

▶ English (US)...
Loaded 134373 words from CMU dictionary
Added 33 common en words
Generating 134406 pronunciations for en-us...
  Processed 1000/134406 words...
  Processed 10000/134406 words...
  ...
Generated 133891 pronunciations
✅ Wrote 133891 entries to src/tts/phonemizer/data/espeak_en_us.dict
   File size: 4821.3 KB

▶ English (GB)...
...

✅ Training complete!
```

### 3. Rebuild with Embedded Dictionaries

```bash
npm run build:core
```

**CMake output:**
```
-- ✅ Espeak dictionary (en-us) found - enabling
-- ✅ Espeak dictionary (en-gb) found - enabling
-- ✅ Espeak dictionary (de) found - enabling
```

### 4. Test Pronunciation Quality

```python
# Test with phonemizer
from ethervox import Phonemizer

phonemizer = Phonemizer("en-us")

# These now use espeak-trained pronunciations
print(phonemizer.text_to_ipa("hello world"))
# Output: h ə ˈ l o ʊ   w ɝ l d

print(phonemizer.text_to_ipa("integration"))
# Output: ˌɪ n t ə ɡ ɹ ˈ eɪ ʃ ə n
```

### 5. Validate Accuracy

```bash
# Compare against live espeak
python3 tools/validate_espeak_dict.py \
    --dict src/tts/phonemizer/data/espeak_en_us.dict \
    --lang en-us \
    --sample-size 1000
```

**Output:**
```
Loading dictionary: src/tts/phonemizer/data/espeak_en_us.dict
Testing 1000 random words against espeak-ng...

============================================================
Validation Results
============================================================
Tested:      1000 words
Exact match: 967 (96.7%)
Mismatches:  33

============================================================
✅ PASS: Dictionary is highly accurate
```

## Adding Custom Domain Vocabulary

If your application has specialized terminology:

```bash
# Create vocabulary file
cat > my_vocab.txt << EOF
kubernetes
microservice
dockerfile
containerization
EOF

# Generate dictionary with custom vocab
python3 tools/generate_espeak_dict.py \
    --lang en-us \
    --vocab-file my_vocab.txt \
    --output src/tts/phonemizer/data/espeak_en_us_custom.dict \
    --format header

# Rebuild
npm run build:core
```

## User Training at Runtime

Users can still correct pronunciations:

```python
# User hears mispronunciation and provides correction
phonemizer.train_pronunciation(
    word="AWS",
    correct_audio="/path/to/recording.wav",
    speaker_id=1
)

# Override saved to ~/.ethervox/pronunciation_overrides.json
# Future lookups use the correction automatically
```

## Memory Usage Comparison

| Configuration | Memory | Accuracy | Latency |
|--------------|--------|----------|---------|
| G2P rules only | 50 KB | ~75% | 200 μs/word |
| + CMU Dict | 5 MB | ~85% | 10 μs/word |
| + Espeak Dict | 12 MB | ~96% | 5 μs/word |
| + User Overrides | 12 MB + dynamic | ~99%+ | 1 μs/word |

For embedded devices (ESP32), use trimmed dictionaries:

```bash
# Generate compact dictionary (top 5000 words only)
python3 tools/generate_espeak_dict.py \
    --lang en-us \
    --max-words 5000 \
    --output src/tts/phonemizer/data/espeak_en_us_compact.dict
```

## Continuous Improvement Workflow

```
┌─────────────────┐
│  User Reports   │
│ Mispronunciation│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Pronunciation   │
│    Trainer UI   │
└────────┬────────┘
         │
         ▼
┌─────────────────────┐
│ Local Override File │ (Highest priority)
│ ~/.ethervox/...json │
└────────┬────────────┘
         │
         │ (usage_count >= 50, confidence >= 0.85)
         ▼
┌──────────────────────┐
│ Community Overrides  │
│ Shared across users  │
└────────┬─────────────┘
         │
         │ (validated, stable)
         ▼
┌──────────────────────┐
│  Next Release        │
│ Merged into espeak   │
│ dictionary headers   │
└──────────────────────┘
```

## Troubleshooting

### "espeak-ng: command not found"

Espeak is only needed for **generating** dictionaries, not runtime:

```bash
# Install on your development machine
brew install espeak-ng
```

### Dictionary not loading

Check CMake found the headers:

```bash
grep "Espeak dictionary" build/CMakeCache.txt
```

If not found, regenerate:

```bash
./tools/train_phonemizer.sh
npm run clean
npm run build:core
```

### Pronunciation still wrong

1. Check override priority in logs
2. Add manual override via pronunciation trainer
3. Report to community overrides

## Legal Compliance

✅ **Legal**: Espeak used as development tool  
✅ **Legal**: Generated pronunciation data embedded  
✅ **Legal**: Training corpus derived from espeak output  
❌ **Illegal**: Linking against espeak libraries  
❌ **Illegal**: Distributing espeak source code  

This approach is equivalent to using GPL tools (gcc, grep) to create proprietary software - the output isn't GPL-infected.
