# Voice Training API Reference

## Quick Start

### Recording Audio
```c
#include "ethervox/audio_recording.h"

// Record 3 seconds from microphone
int result = ethervox_audio_record_to_file(
    "/tmp/pronunciation.wav",
    3.0f,        // duration in seconds
    16000,       // sample rate (Hz)
    1            // channels (1=mono, 2=stereo)
);
if (result != 0) {
    fprintf(stderr, "Recording failed\n");
}
```

### Writing WAV Files
```c
#include "ethervox/audio_recording.h"

// Generate audio samples (e.g., 440Hz tone)
float samples[16000];
for (int i = 0; i < 16000; i++) {
    samples[i] = sinf(2.0f * M_PI * 440.0f * i / 16000.0f);
}

// Write to WAV file
ethervox_audio_write_wav(
    "/tmp/tone.wav",
    samples,
    16000,       // number of samples
    16000,       // sample rate
    1            // channels
);
```

### Pronunciation Overrides
```c
#include "tts/phonemizer/pronunciation_overrides.h"

// Load overrides from disk
pronunciation_override_store_t* store = pronunciation_overrides_load();

// Add custom pronunciation
pronunciation_override_t override = {0};
strncpy(override.word, "hello", sizeof(override.word) - 1);
strncpy(override.phonemes, "HH AH0 L OW1", sizeof(override.phonemes) - 1);
override.confidence = 0.85f;
override.usage_count = 1;
override.created = time(NULL);
override.last_used = time(NULL);

pronunciation_overrides_add(store, &override);

// Lookup pronunciation
pronunciation_override_t found;
if (pronunciation_overrides_lookup(store, "hello", &found) == 0) {
    printf("Pronunciation: %s\n", found.phonemes);
}

// Save to disk
pronunciation_overrides_save(store);
pronunciation_overrides_free(store);
```

### Accessing Conversation Contexts
```c
#include "ethervox/conversation.h"

ethervox_conversation_session_t* session = /* get from somewhere */;

// Get TTS context
ethervox_tts_context_t* tts = ethervox_conversation_get_tts(session);

// Get STT runtime
ethervox_stt_runtime_t* stt = ethervox_conversation_get_stt(session);

// Get phonemizer (internal type)
void* phonemizer = ethervox_conversation_get_phonemizer(session);
phonemizer_t* p = (phonemizer_t*)phonemizer;  // Cast to actual type
```

## API Reference

### Audio Recording Functions

#### ethervox_audio_record_to_file
```c
int ethervox_audio_record_to_file(
    const char* output_path,
    float duration_sec,
    int sample_rate,
    int channels
);
```
Records audio from the default microphone to a WAV file.

**Parameters:**
- `output_path`: Path to output WAV file
- `duration_sec`: Recording duration in seconds
- `sample_rate`: Sample rate in Hz (typically 16000)
- `channels`: Number of channels (1=mono, 2=stereo)

**Returns:** 0 on success, -1 on error

**Example:**
```c
// Record 5 seconds at 16kHz mono
ethervox_audio_record_to_file("/tmp/voice.wav", 5.0f, 16000, 1);
```

---

#### ethervox_audio_record_with_vad
```c
int ethervox_audio_record_with_vad(
    const char* output_path,
    float max_duration_sec,
    float vad_threshold,
    int silence_ms
);
```
Records audio with Voice Activity Detection (stops on silence).

**Parameters:**
- `output_path`: Path to output WAV file
- `max_duration_sec`: Maximum recording duration
- `vad_threshold`: Voice detection threshold (0.0-1.0)
- `silence_ms`: Silence duration to stop recording (milliseconds)

**Returns:** 0 on success, -1 on error

**Status:** Placeholder implementation

---

#### ethervox_audio_write_wav
```c
int ethervox_audio_write_wav(
    const char* output_path,
    const float* samples,
    int num_samples,
    int sample_rate,
    int channels
);
```
Writes audio samples to a WAV file (16-bit PCM).

**Parameters:**
- `output_path`: Path to output WAV file
- `samples`: Audio samples (float, -1.0 to 1.0)
- `num_samples`: Total number of samples
- `sample_rate`: Sample rate in Hz
- `channels`: Number of channels

**Returns:** 0 on success, -1 on error

**Example:**
```c
float silence[16000] = {0};  // 1 second of silence
ethervox_audio_write_wav("/tmp/silence.wav", silence, 16000, 16000, 1);
```

### Pronunciation Override Functions

#### pronunciation_overrides_load
```c
pronunciation_override_store_t* pronunciation_overrides_load(void);
```
Loads pronunciation overrides from disk.

**Returns:** Override store or NULL on error

**Files Loaded:**
- `~/.ethervox/pronunciation_overrides.json` (personal)
- `~/.ethervox/community_overrides.json` (community)

---

#### pronunciation_overrides_lookup
```c
int pronunciation_overrides_lookup(
    pronunciation_override_store_t* store,
    const char* word,
    pronunciation_override_t* out_override
);
```
Looks up pronunciation for a word (case-insensitive).

**Parameters:**
- `store`: Override store
- `word`: Word to lookup
- `out_override`: Output buffer (caller allocates)

**Returns:** 0 if found, -1 if not found

---

#### pronunciation_overrides_add
```c
int pronunciation_overrides_add(
    pronunciation_override_store_t* store,
    const pronunciation_override_t* override
);
```
Adds or updates a pronunciation override.

**Parameters:**
- `store`: Override store
- `override`: Override to add/update

**Returns:** 0 on success, -1 on error

**Note:** If word already exists, usage_count is incremented.

---

#### pronunciation_overrides_save
```c
int pronunciation_overrides_save(pronunciation_override_store_t* store);
```
Saves overrides to disk.

**Parameters:**
- `store`: Override store

**Returns:** 0 on success, -1 on error

---

#### pronunciation_overrides_free
```c
void pronunciation_overrides_free(pronunciation_override_store_t* store);
```
Frees override store.

### Conversation Context Accessors

#### ethervox_conversation_get_tts
```c
ethervox_tts_context_t* ethervox_conversation_get_tts(
    ethervox_conversation_session_t* session
);
```
Gets TTS context from conversation session.

**Returns:** TTS context or NULL if not initialized

---

#### ethervox_conversation_get_stt
```c
ethervox_stt_runtime_t* ethervox_conversation_get_stt(
    ethervox_conversation_session_t* session
);
```
Gets STT runtime from conversation session.

**Returns:** STT runtime or NULL if not initialized

---

#### ethervox_conversation_get_phonemizer
```c
void* ethervox_conversation_get_phonemizer(
    ethervox_conversation_session_t* session
);
```
Gets phonemizer from conversation session (returns opaque pointer).

**Returns:** Phonemizer or NULL if not initialized

**Note:** Cast to `phonemizer_t*` in implementation code.

## Data Structures

### pronunciation_override_t
```c
typedef struct {
    char word[MAX_WORD_LENGTH];           // Original word
    char phonemes[MAX_PHONEME_LENGTH];    // ARPAbet (e.g., "HH AH0 L OW1")
    char ipa[MAX_IPA_LENGTH];             // IPA (e.g., "həˈloʊ")
    uint32_t usage_count;                 // Times used
    float confidence;                      // 0.0-1.0
    int trained_speaker_id;                // Speaker ID
    time_t created;                        // Creation timestamp
    time_t last_used;                      // Last usage timestamp
    bool is_community;                     // Promoted to community
} pronunciation_override_t;
```

### Constants
```c
#define MAX_WORD_LENGTH 128
#define MAX_PHONEME_LENGTH 256
#define MAX_IPA_LENGTH 256
```

## Error Handling

All functions return:
- **0** on success
- **-1** on error

Check return values:
```c
if (ethervox_audio_record_to_file("voice.wav", 3.0f, 16000, 1) != 0) {
    fprintf(stderr, "Error: Recording failed\n");
    return -1;
}
```

## File Formats

### WAV File Structure
```
[RIFF Header]  - 8 bytes
[WAV Header]   - 4 bytes  
[fmt chunk]    - 24 bytes (PCM format)
[data header]  - 8 bytes
[PCM samples]  - variable (num_samples * channels * 2 bytes)
```

**Format:** 16-bit PCM, Little Endian

### Pronunciation Override Storage
**File:** `~/.ethervox/pronunciation_overrides.json`

```json
{
  "word1": {
    "word": "word1",
    "phonemes": "W ER1 D",
    "ipa": "wɜrd",
    "usage_count": 5,
    "confidence": 0.9,
    "trained_speaker_id": 0,
    "created": 1702854000,
    "last_used": 1702940400,
    "is_community": false
  }
}
```

## Testing

### Unit Tests
```bash
make test_voice_training
./tests/test_voice_training
```

**Tests:**
- WAV file writing
- Audio format validation
- File size verification

### Integration Tests (Manual)
```bash
make test_voice_training_integration
./tests/test_voice_training_integration
```

**Requirements:**
- Microphone hardware
- User interaction (speak when prompted)

**Tests:**
- End-to-end recording workflow
- WAV file creation and validation
- Pronunciation override storage/lookup

## Examples

### Complete Training Workflow
```c
#include "ethervox/audio_recording.h"
#include "tts/phonemizer/pronunciation_overrides.h"

int train_pronunciation(const char* word) {
    // 1. Record user pronunciation
    char wav_path[256];
    snprintf(wav_path, sizeof(wav_path), "/tmp/%s.wav", word);
    
    printf("Say '%s' when you hear the beep...\n", word);
    if (ethervox_audio_record_to_file(wav_path, 3.0f, 16000, 1) != 0) {
        fprintf(stderr, "Recording failed\n");
        return -1;
    }
    
    // 2. Process audio (extract phonemes)
    // TODO: Implement phoneme extraction from audio
    const char* phonemes = "P L EY S HH OW L D ER";  // Placeholder
    
    // 3. Store pronunciation override
    pronunciation_override_store_t* store = pronunciation_overrides_load();
    if (!store) {
        fprintf(stderr, "Failed to load overrides\n");
        return -1;
    }
    
    pronunciation_override_t override = {0};
    strncpy(override.word, word, sizeof(override.word) - 1);
    strncpy(override.phonemes, phonemes, sizeof(override.phonemes) - 1);
    override.confidence = 0.85f;
    override.usage_count = 1;
    override.created = time(NULL);
    override.last_used = time(NULL);
    
    pronunciation_overrides_add(store, &override);
    pronunciation_overrides_save(store);
    pronunciation_overrides_free(store);
    
    printf("✅ Learned pronunciation: %s → %s\n", word, phonemes);
    return 0;
}
```

### Using Overrides in TTS
```c
#include "tts/phonemizer/pronunciation_overrides.h"

const char* get_pronunciation(const char* word) {
    static pronunciation_override_store_t* store = NULL;
    if (!store) {
        store = pronunciation_overrides_load();
    }
    
    pronunciation_override_t override;
    if (pronunciation_overrides_lookup(store, word, &override) == 0) {
        return override.phonemes;  // Use custom pronunciation
    }
    
    return NULL;  // Use default phonemizer
}
```

## Performance Tips

1. **Reuse audio buffers** - Allocate once, reuse for multiple recordings
2. **Batch override saves** - Save after multiple additions, not each one
3. **Cache override lookups** - Load store once, reuse for session
4. **Use appropriate sample rates** - 16kHz for voice, 44.1kHz for music

## Troubleshooting

### Recording produces silence
- Check microphone permissions
- Verify audio device exists: `ethervox_audio_list_devices()`
- Test with system recording tool first

### WAV file won't play
- Verify sample rate (16000 Hz)
- Check file size (should be ~192KB for 3s @ 16kHz mono)
- Use `file` command: `file recording.wav`

### Overrides not persisting
- Check file permissions: `~/.ethervox/pronunciation_overrides.json`
- Verify `pronunciation_overrides_save()` is called
- Check return value for errors

### Build errors
```bash
# Regenerate build files
cmake .

# Clean and rebuild
make clean
make -j8
```

## See Also

- [Voice Training Complete Documentation](VOICE_TRAINING_COMPLETE.md)
- [Audio System Documentation](../README.md#audio)
- [TTS Phonemizer Documentation](../src/tts/phonemizer/README.md)
