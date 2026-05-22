# EthervoxAI Tools

## Python Development Environment

Python tools use a **virtual environment** to avoid conflicts with system packages.

### Quick Start

```bash
# Automatic setup (recommended)
./train_phonemizer.sh  # Auto-creates venv on first run

# Manual setup
./setup_python_env.sh
source venv/bin/activate
```

**Location:** `tools/venv/` (git-ignored)

See [Python Environment](#python-environment) section below for details.

## Voice Tuner (`ethervox-tune-voice`)

Interactive tool for finding optimal TTS settings for each Piper voice model.

### Prerequisites

- Install `sox` for audio playback:
  ```bash
  # macOS
  brew install sox
  
  # Linux
  sudo apt-get install sox
  ```

- Download at least one Piper voice:
  ```bash
  ./scripts/download-piper-model.sh en_US-amy-medium
  ./scripts/download-piper-model.sh en_GB-alba-medium
  ```

### Usage

```bash
./ethervox-tune-voice
```

### How It Works

1. **Select Voice**: Choose from available Piper models in `~/.ethervox/models/piper/`

2. **Systematic Testing**: The tool cycles through combinations of:
   - **Phoneme Variance**: 0.0, 0.1, 0.2, 0.3, 0.5, 0.667, 0.8
   - **Prosody Variance**: 0.0, 0.1, 0.2, 0.3, 0.5, 0.8, 1.0, 1.2
   - **Speaking Rate**: 0.8, 0.9, 1.0, 1.1, 1.2

3. **Rate Each Sample**: 
   - Press `y` if you like the combination
   - Press `n` if you don't
   - Press `q` to quit early

4. **Save Results**: After testing, the tool saves your preferred settings to the voice's config JSON:
   ```json
   {
     "audio": { ... },
     "phonemes": { ... },
     "inference": { ... },
     "ethervox_optimal": {
       "phoneme_variance": 0.2,
       "prosody_variance": 0.1,
       "speaking_rate": 1.0
     }
   }
   ```

### Test Sentence

The tool uses: *"Hello, this is a test of the American English voice pronunciation."*

You can modify the `TEST_SENTENCE` constant in `voice_tuner.c` to test with different phrases.

### Tips

- **Lower variance values** (0.0-0.3) give clearer, more consistent pronunciation
- **Higher variance values** (0.8-1.2) sound more natural and expressive
- **Start with extremes**: Test 0.0 and 0.8 first to understand the range
- **Focus on problem words**: Modify the test sentence to include words that sound unclear
- **One voice at a time**: Each voice has different optimal settings

### Expanding Voice Support

The tool automatically discovers all `.onnx` models in `~/.ethervox/models/piper/`. To add new voices:

```bash
# Download any Piper voice
./scripts/download-piper-model.sh de_DE-thorsten-medium
./scripts/download-piper-model.sh zh_CN-huayan-medium

# Re-run the tuner
./ethervox-tune-voice
```

### Loading Optimal Settings

Your code can read the optimal settings from the JSON:

```c
cJSON* root = cJSON_Parse(config_json);
cJSON* optimal = cJSON_GetObjectItem(root, "ethervox_optimal");
if (optimal) {
    float phoneme_var = cJSON_GetObjectItem(optimal, "phoneme_variance")->valuedouble;
    float prosody_var = cJSON_GetObjectItem(optimal, "prosody_variance")->valuedouble;
    float rate = cJSON_GetObjectItem(optimal, "speaking_rate")->valuedouble;
    // Use these values in ethervox_tts_config_t
}
```

### Example Workflow

```bash
# 1. Build the tool
make voice_tuner

# 2. Run tuner
./ethervox-tune-voice

# 3. Select voice (e.g., "1" for en_US-amy-medium)

# 4. Listen and rate combinations (press Enter between samples)
[1/280] Testing: phoneme=0.00, prosody=0.00, rate=0.80
<audio plays>
Did you like this combination? (y/n/q to quit): y
Was that a YES (y)? y
✓ Marked as preferred

[2/280] Testing: phoneme=0.00, prosody=0.10, rate=0.80
<audio plays>
Did you like this combination? (y/n/q to quit): n
...
```

## Phonemizer Training Tools

Generate espeak-compatible pronunciation dictionaries for offline use.

### Quick Start

```bash
# One command does everything (auto-creates venv)
./train_phonemizer.sh
```

### Requirements

- **espeak-ng** (system package): `brew install espeak-ng`
- **Python 3.7+** with venv support (standard on modern systems)

### What It Does

1. Creates Python virtual environment (first run only)
2. Installs `phonemizer` library
3. Generates pronunciation dictionaries for en-us, en-gb, de
4. Creates C headers for embedding in binary

**Output files:**
- `src/tts/phonemizer/data/espeak_*.dict` - Text format
- `src/tts/phonemizer/data/espeak_dict_*.h` - Embedded headers

### Individual Tools

```bash
# Setup Python environment (manual)
./setup_python_env.sh

# Generate specific language
source venv/bin/activate
python3 generate_espeak_dict.py --lang en-us --output ../src/tts/phonemizer/data/espeak_en_us.dict

# Validate accuracy
python3 validate_espeak_dict.py --dict ../src/tts/phonemizer/data/espeak_en_us.dict --lang en-us
```

See [docs/PHONEMIZER_TRAINING.md](../docs/PHONEMIZER_TRAINING.md) for details.

## Python Environment

### Automatic Setup (Recommended)

Training scripts automatically manage the virtual environment:

```bash
./train_phonemizer.sh  # Creates venv if needed
```

### Manual Setup

```bash
# Create and install dependencies
./setup_python_env.sh

# Activate when needed
source venv/bin/activate

# Your prompt changes to show (venv)
(venv) $ python3 --version
(venv) $ pip list

# Deactivate when done
(venv) $ deactivate
```

### Troubleshooting

**"externally-managed-environment" error:**
- Solution: Use the virtual environment (automatic in our scripts)

**Virtual environment corrupted:**
```bash
rm -rf venv/
./setup_python_env.sh
```

### Git and CI/CD

- **Location:** `tools/venv/`
- **Git status:** Excluded via `.gitignore`
- **CI builds:** Don't need venv (use committed training data)

## License Notes

Tools may use GPL software (espeak-ng) for **development only** - not distributed. Generated training data is not GPL-infected.


# 5. Save results
Best Settings Found
----------------------------------------
  Phoneme Variance: 0.200
  Prosody Variance: 0.100
  Speaking Rate: 1.00
  (Based on 12 liked combinations)

Save these settings to voice config? (y/n): y
✓ Saved optimal settings to ~/.ethervox/models/piper/en_US-amy-medium.onnx.json
```

### Notes

- Total combinations: 7 × 8 × 5 = **280 tests per voice**
- You can quit early with `q` if you find good settings
- The tool remembers the **last** liked combination as optimal
- Settings are preserved when updating Piper models (stored separately from `inference` block)
