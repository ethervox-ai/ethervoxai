# EthervoxAI Scripts

This folder contains utility scripts for building, deploying, and setting up cross-compilation environments for EthervoxAI on various platforms. Below is a description of each script and example usage.

---

## download-governor-model.sh
**Purpose:** Downloads the Qwen2.5-3B-Instruct quantized model for Governor orchestration.

**Usage:**
```bash
./scripts/download-governor-model.sh
```

**Details:**
- Downloads ~2GB Q4_K_M quantized GGUF model
- Saves to `models/Qwen2.5-3B-Instruct-Q4_K_M.gguf`
- Requires curl or wget
- Resumes interrupted downloads

**Example:**
```bash
./scripts/download-governor-model.sh
# Model will be saved to: models/Qwen2.5-3B-Instruct-Q4_K_M.gguf
```

---

## download-whisper-model.sh
**Purpose:** Downloads Whisper STT models in GGML format for voice transcription.

**Usage:**
```bash
./scripts/download-whisper-model.sh [model_name] [dest_dir]
```

**Parameters:**
- `model_name` (optional): Model to download, defaults to `base`
- `dest_dir` (optional): Destination directory, defaults to `~/.ethervox/models/whisper`

**Available Models:**
- `tiny.en` (~75 MB) - Fastest, English-only
- `base` (~141 MB) - Multilingual, good balance **(DEFAULT)**
- `base.en` (~141 MB) - Good balance, English-only
- `small.en` (~466 MB) - Better accuracy, English-only
- `medium.en` (~1.5 GB) - High accuracy, English-only
- `tiny` (~75 MB) - Multilingual (99 languages)
- `base` (~141 MB) - Multilingual
- `small` (~466 MB) - Multilingual, better accuracy

**Details:**
- Downloads from HuggingFace (ggerganov/whisper.cpp)
- Auto-validates file size
- Requires curl or wget

**Examples:**
```bash
# Download default model (base)
./scripts/download-whisper-model.sh

# Download specific model
./scripts/download-whisper-model.sh small.en

# Download to custom location
./scripts/download-whisper-model.sh base ~/mymodels/whisper
```

**Auto-Download:**
Voice tools will automatically attempt to download `base` (multilingual) if no model is found during initialization. The download script can also be run manually for other models or custom locations, including the English-only `.en` variants if desired.

---

## download-piper-model.sh
**Purpose:** Downloads Piper neural TTS models in ONNX format for high-quality text-to-speech.

**Usage:**
```bash
./scripts/download-piper-model.sh [voice_name] [dest_dir]
```

**Parameters:**
- `voice_name` (optional) - Voice identifier (default: `en_US-lessac-medium`)
- `dest_dir` (optional) - Destination directory (default: `~/.ethervox/models/piper`)

**Multilingual Support - 40+ Languages:**
- English (US/UK): `en_US-lessac-medium` (recommended), `en_GB-alan-medium`
- Spanish: `es_MX-ald-medium`, `es_ES-mls_10246-low`
- French: `fr_FR-siwis-medium`, `fr_FR-tom-medium`
- German: `de_DE-thorsten-medium`, `de_DE-eva_k-medium`
- Italian: `it_IT-riccardo-medium`
- Portuguese: `pt_BR-faber-medium`
- Russian: `ru_RU-ruslan-medium`
- Chinese (Mandarin): `zh_CN-huayan-medium`
- Japanese: `ja_JP-natsuya_enu-medium`
- Korean: `ko_KR-kss-medium`
- Arabic: `ar_JO-kareem-medium`
- Hindi: `hi_IN-wavylocal-medium`
- Plus many more! Run `./scripts/download-piper-model.sh list` for full list

**Features:**
- Downloads ONNX model + JSON config from HuggingFace (rhasspy/piper-voices)
- Validates both files are present
- Shows download progress
- Idempotent (skips if already downloaded)

**Examples:**
```bash
# Download default voice (lessac medium)
./scripts/download-piper-model.sh

# Download specific voice
./scripts/download-piper-model.sh en_US-amy-medium

# Custom destination
./scripts/download-piper-model.sh en_US-lessac-high ~/custom/path
```

**Dependencies:**
- Requires ONNX Runtime: `brew install onnxruntime` (macOS)
- Used with AEC for echo-cancelled voice conversations

---

## build.sh
**Purpose:** Cross-platform build orchestrator for EthervoxAI.

**Usage:**
```bash
./scripts/build.sh [platform] [options]
```
**Platforms:**
- `desktop` (default)
- `debug` (coverage)
- `esp32`, `esp32-s3`, `esp32-c3`, `all-esp32`
- `rpi`, `rpi-zero`, `all-rpi`
- `all` (all supported)

**Options:**
- `--clean` (clean build)
- `--test` (run tests)
- `--verbose` (verbose output)
- `--help` (show help)

**Examples:**
```bash
./scripts/build.sh           # Build for desktop
./scripts/build.sh esp32-s3  # Build for ESP32-S3
./scripts/build.sh rpi       # Build for Raspberry Pi
```

---

## deploy-rpi.sh
**Purpose:** Deploys the built EthervoxAI binary and configs to a Raspberry Pi over SSH.

**Usage:**
```bash
./scripts/deploy-rpi.sh
```
**Environment variables:**
- `RPI_USER` (default: pi)
- `RPI_HOST` (default: raspberrypi.local)
- `RPI_DIR` (default: /home/pi/ethervoxai)

**Example:**
```bash
RPI_USER=pi RPI_HOST=192.168.1.42 ./scripts/deploy-rpi.sh
```

---

## download-windows-deps.sh
**Purpose:** Guides manual download of pre-built Windows libraries (OpenSSL, libcurl) for cross-compilation.

**Usage:**
```bash
./scripts/download-windows-deps.sh
```
**Note:** Follow the printed instructions to download and extract libraries to `sysroot/windows`.

---

## fix_build_errors.sh
**Purpose:** Checks for duplicate type definitions and common build errors in headers.

**Usage:**
```bash
./scripts/fix_build_errors.sh
```

---

## setup-esp32-toolchain.sh
**Purpose:** Installs ESP-IDF, sets up the ESP32 toolchain, and prepares the project structure for ESP32 builds.

**Usage:**
```bash
./scripts/setup-esp32-toolchain.sh
```
**Actions:**
- Installs ESP-IDF (if missing)
- Sets up environment
- Creates `esp32-project` with symlinks to `src` and `include`
- Verifies installation

---

## setup-rpi-toolchain.sh
**Purpose:** Installs ARM cross-compiler and cross-compiles required libraries for Raspberry Pi builds.

**Usage:**
```bash
./scripts/setup-rpi-toolchain.sh
```
**Actions:**
- Installs ARM toolchain
- Cross-compiles `bcm2835` library
- Downloads and prepares ARM versions of `curl`, `openssl` for sysroot

---

## setup-windows-toolchain.sh
**Purpose:** Installs MinGW cross-compiler and sets up sysroot for Windows cross-compilation.

**Usage:**
```bash
./scripts/setup-windows-toolchain.sh
```
**Actions:**
- Installs MinGW toolchain
- Creates sysroot structure
- Guides setup of Windows development libraries

---

## General Notes
- All scripts should be run from the project root unless otherwise specified.
- Some scripts require `sudo` for installing packages.
- For platform-specific builds, ensure the corresponding toolchain setup script has been run first.
