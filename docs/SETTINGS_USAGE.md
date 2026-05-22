# Settings Persistence System - Usage Examples

## Overview

The EthervoxAI settings persistence system provides JSON-based configuration for:
- Whisper STT parameters (model, language, temperature, beam size, threads, GPU)
- Conversation settings (timeouts, energy threshold, hallucination filtering)
- Wake word detection (phrase, thresholds, VAD parameters, cooldown)

## Command-Line Usage

### View Current Settings
```bash
/config
```
Shows all current settings in a formatted display.

### Save Settings to Disk
```bash
/config save
```
Saves current settings to `~/.ethervox/settings.json`.

### Load Settings from Disk
```bash
/config load
```
Reloads settings from the saved file.

### Reset to Defaults
```bash
/config reset
```
Resets all settings to default values.

### Export as JSON
```bash
/config export
```
Outputs settings as JSON string (useful for backup/sharing).

## Programmatic Usage

### Include Header
```c
#include "ethervox/settings.h"
```

### Get Default Settings
```c
ethervox_persistent_settings_t settings = ethervox_settings_get_defaults();
```

### Customize Settings
```c
// Whisper configuration
strncpy(settings.whisper.model_name, "small.bin", sizeof(settings.whisper.model_name) - 1);
strncpy(settings.whisper.language, "en", sizeof(settings.whisper.language) - 1);
settings.whisper.temperature = 0.0f;
settings.whisper.beam_size = 5;
settings.whisper.n_threads = 4;
settings.whisper.use_gpu = false;

// Conversation settings
settings.conversation.listen_timeout_ms = 5000;
settings.conversation.silence_timeout_ms = 2000;
settings.conversation.audio_energy_threshold = 0.01f;
settings.conversation.filter_hallucinations = true;
settings.conversation.max_audio_chunk_size = 16000 * 3; // 3 seconds at 16kHz

// Wake word settings
strncpy(settings.wake_word.wake_phrase, "hey ethervox", 
        sizeof(settings.wake_word.wake_phrase) - 1);
settings.wake_word.detection_threshold = 0.4f;
settings.wake_word.expected_syllables = 3;
settings.wake_word.min_syllables = 2;
settings.wake_word.max_syllables = 5;
settings.wake_word.vad_energy_threshold = 0.01f;
settings.wake_word.vad_zcr_min = 0.01f;
settings.wake_word.vad_zcr_max = 0.35f;
settings.wake_word.cooldown_ms = 3000;
```

### Save Settings
```c
// Save to default location (~/.ethervox/settings.json)
if (ethervox_settings_save(&settings, NULL) == 0) {
    printf("Settings saved successfully\n");
}

// Save to custom location
if (ethervox_settings_save(&settings, "/path/to/config.json") == 0) {
    printf("Settings saved to custom location\n");
}
```

### Load Settings
```c
ethervox_persistent_settings_t settings;

// Load from default location
if (ethervox_settings_load(&settings, NULL) == 0) {
    printf("Settings loaded successfully\n");
} else {
    // Falls back to defaults if file not found
    settings = ethervox_settings_get_defaults();
}

// Load from custom location
if (ethervox_settings_load(&settings, "/path/to/config.json") == 0) {
    printf("Settings loaded from custom location\n");
}
```

### Export/Import JSON
```c
// Export to JSON string
char* json = ethervox_settings_export(&settings);
if (json) {
    printf("JSON: %s\n", json);
    free(json);
}

// Import from JSON string
const char* json_config = "{\"version\": 1, \"whisper\": {...}}";
ethervox_persistent_settings_t imported;
if (ethervox_settings_import(&imported, json_config) == 0) {
    printf("Settings imported successfully\n");
}
```

### Display Settings
```c
ethervox_settings_print(&settings);
```

## JSON Format Example

```json
{
  "version": 1,
  "whisper": {
    "model": "base.bin",
    "language": "auto",
    "temperature": 0.0,
    "beam_size": 5,
    "translate_to_english": false,
    "n_threads": -1,
    "use_gpu": false
  },
  "conversation": {
    "listen_timeout_ms": 5000,
    "conversation_timeout_ms": 30000,
    "silence_timeout_ms": 2000,
    "audio_energy_threshold": 0.01,
    "filter_hallucinations": true,
    "max_audio_chunk_size": 48000
  },
  "wake_word": {
    "wake_phrase": "hey ethervox",
    "detection_threshold": 0.4,
    "expected_syllables": 3,
    "min_syllables": 2,
    "max_syllables": 5,
    "vad_energy_threshold": 0.01,
    "vad_zcr_min": 0.01,
    "vad_zcr_max": 0.35,
    "cooldown_ms": 3000
  }
}
```

## Default File Location

Settings are automatically saved to:
- **macOS/Linux**: `~/.ethervox/settings.json`
- **Windows**: `%USERPROFILE%\.ethervox\settings.json`

## Test Coverage

The settings system includes comprehensive unit tests covering:
- Default settings initialization (10 assertions)
- Save/load roundtrip (15 assertions)
- Nonexistent file handling (2 assertions)
- JSON export/import (10 assertions)
- Invalid JSON handling (3 assertions)
- Partial JSON parsing (6 assertions)
- Whisper settings validation (4 assertions)
- Conversation settings validation (5 assertions)
- Wake word settings validation (8 assertions)
- Default path validation (4 assertions)
- Concurrent access (7 assertions)
- Settings display (1 assertion)

**Total: 70 test assertions, all passing ✓**

Run tests with:
```bash
./build/tests/test_settings_persistence
# or via CTest:
cd build && ctest -R SettingsPersistence -V
```

Test reports are saved to: `~/.ethervox/reports/settings_test_*.txt`

## Integration with Voice Conversation

Settings are loaded at application startup in `main.c`:
```c
// Load persistent settings
if (ethervox_settings_load(&g_settings, NULL) != 0) {
    g_settings = ethervox_settings_get_defaults();
}
```

Future integration with conversation initialization will use these settings to configure:
- Whisper STT model selection and parameters
- Conversation timeouts and behavior
- Wake word detection sensitivity and thresholds

## Notes

- Settings use sensible defaults if file not found
- Partial JSON is supported (missing fields use defaults)
- Invalid JSON returns error but doesn't crash
- Thread-safe for concurrent save/load operations
- All settings validated within acceptable ranges
