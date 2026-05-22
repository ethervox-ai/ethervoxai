# Model Management System

## Overview

EthervoxAI includes a comprehensive model management system that handles downloading, status checking, and lifecycle management for all AI models used across the platform:

- **Governor LLM** - Language models for conversation and tool orchestration
- **Whisper STT** - Speech-to-text for transcription (high accuracy)
- **Vosk STT** - Speech-to-text for real-time conversation (low latency)
- **Piper TTS** - Text-to-speech for natural voice output
- **Wake Word Templates** - Audio templates for improved wake word accuracy

## Quick Start

### Check Model Status

```bash
./ethervoxai
> /models
```

This displays all available models with their download status, size, and descriptions.

### Download a Model

```bash
> /modeldownload whisper ggml-base.en.bin
```

Downloads the Whisper base English model (~74MB).

### Check Specific Model Type

```bash
> /modelstatus governor
```

Shows detailed status for all Governor LLM models.

### Delete a Model

```bash
> /modeldelete vosk vosk-model-en-us-0.22
```

Frees disk space by removing the large Vosk model.

## CLI Commands

| Command | Description |
|---------|-------------|
| `/models` | List all models with status and disk usage |
| `/modelstatus <type>` | Check specific model type (governor/whisper/vosk/piper) |
| `/modeldownload <type> <name>` | Download a model |
| `/modeldelete <type> <name>` | Delete a model to free disk space |

## Model Types

### Governor LLM Models

**Purpose**: Core language model for conversation, reasoning, and tool orchestration.

**Recommended**: `granite-3.0-2b-instruct-Q4_K_M.gguf` (1.5GB)
- Fast inference on mobile devices
- 4-bit quantization for efficiency
- Optimized for tool calling

**Alternative**: `granite-3.0-8b-instruct-Q4_K_M.gguf` (5GB)
- Better quality, slower inference
- Requires desktop-class hardware

**Download**:
```bash
> /modeldownload governor granite-3.0-2b-instruct-Q4_K_M.gguf
```

**Location**: `~/.ethervox/models/governor/`

### Whisper STT Models

**Purpose**: High-accuracy speech recognition for transcription, dictation, and meeting notes.

**Recommended**: `ggml-base.en.bin` (74MB)
- Fast inference (~4x realtime on M1)
- 95% WER (Word Error Rate)
- English-only for better accuracy

**Alternatives**:
- `ggml-small.en.bin` (244MB) - Better accuracy, slower
- `ggml-base.bin` (74MB) - Multilingual (99 languages)

**Download**:
```bash
> /modeldownload whisper ggml-base.en.bin
```

**Location**: `~/.ethervox/models/whisper/`

**Usage**:
```bash
> /transcribe
# Speak...
> /stoptranscribe
```

### Vosk STT Models

**Purpose**: Real-time speech recognition for voice conversations (10x faster than Whisper).

**Recommended**: `vosk-model-small-en-us-0.15` (40MB)
- 0.3x realtime latency
- Lightweight, mobile-friendly
- ~90% WER

**Alternative**: `vosk-model-en-us-0.22` (1.8GB)
- Best accuracy (~92% WER)
- Requires more memory

**Download**:
```bash
> /modeldownload vosk vosk-model-small-en-us-0.15
```

**Location**: `~/.ethervox/models/vosk/vosk-model-small-en-us-0.15/`

**Note**: Vosk models are directories (extracted from .zip), not single files.

**Usage**:
```bash
> /wakeon      # Enable wake word
> /convon      # Enable voice conversation
# Say "hey ethervox" to start talking
```

### Piper TTS Models

**Purpose**: Natural text-to-speech for voice responses.

**Recommended**: `en_US-lessac-medium.onnx` (17MB) + config
- Natural, clear voice
- Low latency
- Requires onnxruntime

**Download**:
```bash
> /modeldownload piper en_US-lessac-medium.onnx
> /modeldownload piper en_US-lessac-medium.onnx.json
```

**Location**: `~/.ethervox/models/piper/`

**Note**: Both .onnx and .onnx.json files are required.

### Wake Word Templates

**Purpose**: Improve wake word accuracy by recording custom templates.

**Location**: `~/.ethervox/models/wake_templates/`

**Usage**:
```bash
> /wakerecord
```

Records your voice saying the wake word for better pattern matching (~90% accuracy vs ~75% without template).

## API Reference

### C API

```c
#include "ethervox/model_downloader.h"

// Check if model exists
ethervox_model_status_t status = ethervox_model_whisper_status("ggml-base.en.bin");
if (status == ETHERVOX_MODEL_STATUS_NOT_FOUND) {
    // Download model
    ethervox_model_download(ETHERVOX_MODEL_TYPE_WHISPER, "ggml-base.en.bin", NULL, NULL);
}

// Get model information
ethervox_model_info_t info;
ethervox_model_check_status(ETHERVOX_MODEL_TYPE_GOVERNOR, NULL, &info);
printf("Model: %s\n", info.name);
printf("Status: %s\n", ethervox_model_status_string(info.status));
printf("Size: %.2f MB\n", info.size_bytes / 1024.0 / 1024.0);

// List all models
ethervox_model_info_t* models = NULL;
uint32_t count = 0;
ethervox_model_list(ETHERVOX_MODEL_TYPE_VOSK, &models, &count);
for (uint32_t i = 0; i < count; i++) {
    printf("%s - %s\n", models[i].name, models[i].description);
}
free(models);
```

### Model Status Enum

```c
typedef enum {
    ETHERVOX_MODEL_STATUS_NOT_FOUND,   // Model not present
    ETHERVOX_MODEL_STATUS_FOUND,       // Model exists and valid
    ETHERVOX_MODEL_STATUS_CORRUPT,     // Model exists but corrupted
    ETHERVOX_MODEL_STATUS_DOWNLOADING, // Currently downloading
    ETHERVOX_MODEL_STATUS_INCOMPLETE,  // Partial download
    ETHERVOX_MODEL_STATUS_UNKNOWN      // Cannot determine status
} ethervox_model_status_t;
```

## Model Storage

All models are stored in `~/.ethervox/models/` with subdirectories by type:

```
~/.ethervox/models/
├── governor/
│   └── granite-3.0-2b-instruct-Q4_K_M.gguf
├── whisper/
│   ├── ggml-base.en.bin
│   └── ggml-small.en.bin
├── vosk/
│   └── vosk-model-small-en-us-0.15/
│       ├── am/
│       ├── conf/
│       ├── graph/
│       └── ivector/
├── piper/
│   ├── en_US-lessac-medium.onnx
│   └── en_US-lessac-medium.onnx.json
└── wake_templates/
    └── hey_ethervox.raw
```

## Disk Space Management

### Check Total Usage

```bash
> /models
Total disk usage: 1847.23 MB
```

### Check Available Space

The system automatically checks disk space before downloading:
- Requires 120% of model size as safety margin
- Aborts download if insufficient space

### Free Space

Delete unused models:

```bash
> /modeldelete governor granite-3.0-8b-instruct-Q4_K_M.gguf
This will free 5000.00 MB of disk space
Are you sure? (y/N): y
✅ Model deleted
```

## UI Integration

### Android/iOS Model Picker

```kotlin
// Kotlin example for Android UI
val modelManager = ModelDownloader()

// Get available models
val governorModels = modelManager.listModels(ModelType.GOVERNOR)
governorModels.forEach { model ->
    println("${model.name} - ${model.status}")
    if (model.status == ModelStatus.NOT_FOUND) {
        // Show download button
    }
}

// Download with progress
modelManager.download(ModelType.WHISPER, "ggml-base.en.bin") { progress ->
    updateProgressBar(progress.downloadedBytes, progress.totalBytes)
}
```

### Status Icons

Use these icons in UI:

| Status | Icon | Color |
|--------|------|-------|
| `FOUND` | ✅ | Green |
| `NOT_FOUND` | ❌ | Gray |
| `INCOMPLETE` | ⚠️ | Yellow |
| `CORRUPT` | 🔴 | Red |
| `DOWNLOADING` | 📥 | Blue |

### Recommended Models Badge

Models with `is_default = true` should show a "Recommended" badge.

## Performance Characteristics

| Model Type | Size | Inference Speed | Accuracy | Use Case |
|------------|------|-----------------|----------|----------|
| Granite 2B | 1.5GB | Fast | Good | Mobile conversations |
| Granite 8B | 5GB | Slow | Excellent | Desktop workflows |
| Whisper Base | 74MB | 4x realtime | 95% WER | Transcription |
| Vosk Small | 40MB | 0.3x realtime | 90% WER | Live conversation |
| Vosk Large | 1.8GB | 0.5x realtime | 92% WER | High-quality live |
| Piper Medium | 17MB | <100ms | Natural | Voice output |

## Network Requirements

### Download Bandwidth

- **Governor 2B**: ~1.5GB - Recommend WiFi
- **Whisper Base**: ~74MB - Can use cellular
- **Vosk Small**: ~40MB - Can use cellular
- **Piper**: ~17MB - Can use cellular

### Resumable Downloads

Currently downloads are **not resumable** - they start from scratch if interrupted. Planned for future release.

### Offline Mode

Once downloaded, all models work completely offline. No internet required for inference.

## Error Handling

### Download Failures

```bash
> /modeldownload governor granite-3.0-2b-instruct-Q4_K_M.gguf
📥 Downloading Governor LLM model...
❌ Download failed: Network timeout

# Retry the download
> /modeldownload governor granite-3.0-2b-instruct-Q4_K_M.gguf
```

### Corrupt Models

```bash
> /modelstatus whisper
ggml-base.en.bin
  Status: Corrupt
  
# Delete and re-download
> /modeldelete whisper ggml-base.en.bin
> /modeldownload whisper ggml-base.en.bin
```

### Insufficient Disk Space

```bash
> /modeldownload governor granite-3.0-8b-instruct-Q4_K_M.gguf
❌ Insufficient disk space for model
Required: 6000 MB, Available: 3500 MB
```

## Best Practices

### For Mobile Apps

1. **Default Bundle**: Ship with Vosk Small (40MB) for instant conversations
2. **Lazy Loading**: Download Governor/Whisper on first use
3. **User Choice**: Let users choose model quality vs size
4. **WiFi Only**: Default to WiFi-only for large downloads (>100MB)
5. **Progress UI**: Show download progress with cancel option

### For Desktop Apps

1. **Full Bundle**: Include all recommended models
2. **Auto-Update**: Check for model updates periodically
3. **Model Switching**: Allow switching between quality tiers
4. **Backup**: Keep previous model version during updates

### For Embedded Devices

1. **Minimal Set**: Only include required models (Vosk Small)
2. **Pre-flash**: Flash models to device during manufacturing
3. **No Downloads**: Disable download UI (limited storage)

## Security Considerations

### Model Verification

- Currently verifies file size and format
- **TODO**: Add SHA256 checksum verification
- **TODO**: Signature verification for model authenticity

### Download Security

- Uses HTTPS for all downloads
- Hosted on trusted sources (Hugging Face, Alpha Cephei)
- **TODO**: Add checksum verification

### Privacy

- Models downloaded to user's home directory
- No telemetry on model usage
- Completely offline inference

## Troubleshooting

### "Model not found" after download

Check file permissions:
```bash
ls -la ~/.ethervox/models/whisper/
chmod 644 ~/.ethervox/models/whisper/ggml-base.en.bin
```

### Vosk model directory not recognized

Ensure extraction completed:
```bash
ls ~/.ethervox/models/vosk/vosk-model-small-en-us-0.15/
# Should see: am/ conf/ graph/ ivector/
```

### Download stuck at 0%

Check curl is installed:
```bash
which curl
# If missing: brew install curl  (macOS)
#            apt install curl    (Linux)
```

### Model works on desktop but not Android

Check asset packaging in build.gradle:
```gradle
android {
    sourceSets {
        main {
            assets.srcDirs = ['src/main/assets', '../.ethervox/models']
        }
    }
}
```

## Future Enhancements

- [ ] Resumable downloads with progress persistence
- [ ] SHA256 checksum verification
- [ ] Model compression (LZMA) during download
- [ ] Differential updates for model versions
- [ ] Automatic cache cleanup (LRU eviction)
- [ ] Cloud sync for user's downloaded models
- [ ] Model recommendation based on device capabilities
- [ ] Bandwidth throttling for background downloads
- [ ] Download scheduling (off-peak hours)

## Related Documentation

- [Voice Conversation System](VOICE_CONVERSATION_STATUS.md)
- [Vosk Backend Status](VOSK_BACKEND_STATUS.md)
- [Governor LLM Architecture](../GOVERNOR_ARCHITECTURE.md)
- [STT Integration Guide](../INTEGRATION_GUIDE.md)
