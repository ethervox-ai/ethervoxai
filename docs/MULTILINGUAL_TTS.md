# Multilingual TTS Support

## Overview

Piper TTS backend supports multiple languages using ONNX neural models with custom phonemizer.

## Supported Languages

- **English (US)**: `en_US-lessac-medium.onnx` - Male voice
- **Chinese (Mandarin)**: `zh_CN-huayan-medium.onnx`
- **German**: `de_DE-thorsten-medium.onnx`
- **Spanish (Mexico)**: `es_MX-ald-medium.onnx`

Note: Custom phonemizer currently in development. Initial implementation will support English.

## Installation

Download models using the provided script:

```bash
./scripts/download-multilingual-models.sh
```

Models are installed to: `~/.ethervox/models/piper/`

## How It Works

### 1. Model Configuration Auto-Detection

Each Piper model includes a `.onnx.json` config file with:

```json
{
  "phonemes": { "language": "en-us" },  // Phoneme language identifier
  "language": { "code": "en_US" },      // Locale code
  "phoneme_id_map": { ... }              // IPA → ID mappings
}
```

Note: Legacy Piper models may use `"espeak": { "voice": "..." }` - both formats are supported.

The backend automatically:
- Parses the phoneme language from model JSON
- Converts text to phonemes using custom phonemizer (dictionary-based for English)
- Maps phonemes to IDs using the model's `phoneme_id_map`

### 2. Tokenization

Piper models expect **individual IPA characters** as separate tokens:

- `hello` → IPA: `həˈloʊ` → tokens: `[h, ə, ˈ, l, o, ʊ]`
- Stress markers (`ˈ`, `ˌ`) are separate tokens
- Length markers (`ː`) are separate tokens  
- Diphthongs like `oʊ` → `o` + `ʊ` (two tokens)

### 3. Model Selection

Currently, you must specify the model path via:

**Environment variable:**
```bash
export ETHERVOX_TTS_MODEL="$HOME/.ethervox/models/piper/en_US-lessac-medium.onnx"
./voice_assistant_demo
```

**Code (voice_conversation.c or similar):**
```c
ethervox_tts_config_t tts_config = {
    .backend = ETHERVOX_TTS_PIPER,
    .model_path = "/Users/yourname/.ethervox/models/piper/en_US-lessac-medium.onnx"
};
```

## Future Enhancements

### Multi-Model Support

To support **runtime language switching**, we need:

1. **Model Registry** - Load multiple models simultaneously
2. **Language Detection** - Detect input text language
3. **LLM Tool Parameter** - Let the LLM specify language explicitly

### Example LLM Tool API

```json
{
  "tool": "speak",
  "parameters": {
    "text": "Hello, how are you?",
    "language": "en_US",
    "wait_for_response": true
  }
}
```

```json
{
  "tool": "speak", 
  "parameters": {
    "text": "你好",
    "language": "zh_CN"
  }
}
```

### Implementation Plan

**1. Add language parameter to speak tool:**
```c
// src/plugins/conversation_tools/speak.c
const char* language = cJSON_GetStringValue(cJSON_GetObjectItem(params, "language"));
if (language) {
    // Select model based on language code
    const char* model_path = get_model_for_language(language);
    // Update TTS config or switch active model
}
```

**2. Create model registry:**
```c
typedef struct {
    char language_code[16];  // "en_US", "zh_CN", "de_DE"
    char model_path[256];
    ethervox_tts_runtime_t* runtime;  // Pre-loaded TTS runtime
} tts_model_entry_t;

tts_model_entry_t g_tts_models[] = {
    {"en_US", "~/.ethervox/models/piper/en_US-lessac-medium.onnx", NULL},
    {"zh_CN", "~/.ethervox/models/piper/zh_CN-huayan-medium.onnx", NULL},
    {"de_DE", "~/.ethervox/models/piper/de_DE-thorsten-medium.onnx", NULL},
};
```

**3. Lazy-load models on first use:**
```c
ethervox_tts_runtime_t* get_tts_for_language(const char* lang_code) {
    for (int i = 0; i < num_models; i++) {
        if (strcmp(g_tts_models[i].language_code, lang_code) == 0) {
            if (!g_tts_models[i].runtime) {
                // Lazy load
                g_tts_models[i].runtime = ethervox_tts_create(&config);
            }
            return g_tts_models[i].runtime;
        }
    }
    return default_tts;  // Fallback to English
}
```

## Testing

Test each language:

```bash
# English
export ETHERVOX_TTS_MODEL="$HOME/.ethervox/models/piper/en_US-lessac-medium.onnx"
./voice_assistant_demo --text
# Type: "Hello, how are you?"

# Chinese
export ETHERVOX_TTS_MODEL="$HOME/.ethervox/models/piper/zh_CN-huayan-medium.onnx"
./voice_assistant_demo --text
# Type: "你好"

# German  
export ETHERVOX_TTS_MODEL="$HOME/.ethervox/models/piper/de_DE-thorsten-medium.onnx"
./voice_assistant_demo --text
# Type: "Guten Tag"
```

## Current Status

✅ **Completed:**
- ONNX Runtime integration for Piper models
- Individual character tokenization (UTF-8 IPA support)
- Language code parsing from model JSON
- Download script for en/zh/de models
- Removed GPL espeak-ng dependency

🔄 **In Progress:**
- Custom phonemizer implementation (dictionary-based)
- Starting with English (CMU Pronouncing Dictionary)
- Chinese and German support to follow

🔄 **Next Steps:**
- Add language parameter to `speak` tool
- Implement model registry for multi-language support
- Add language detection or LLM-based language selection
- Update system prompt to document language parameter

## References

- Piper voices: https://github.com/rhasspy/piper/blob/master/VOICES.md
- IPA Reference: https://en.wikipedia.org/wiki/International_Phonetic_Alphabet
- IPA chart: https://www.internationalphoneticassociation.org/content/ipa-chart
