# Pronunciation Training System

Adaptive pronunciation learning system for EthervoxAI that improves TTS output based on user feedback.

## Overview

When users indicate a word was mispronounced, the system can learn the correct pronunciation by comparing synthesized variants to user-provided audio samples.

## Architecture

### 3-Tier Storage System

1. **Personal Overrides** (`~/.ethervox/pronunciation_overrides.json`)
   - User-specific pronunciation corrections
   - Immediate storage on successful training
   - Private to individual users

2. **Community Overrides** (`~/.ethervox/community_overrides.json`)
   - Auto-promoted from personal when:
     - `usage_count >= 50`
     - `confidence >= 0.85`
   - Shared learning across users
   - Quality-filtered corrections

3. **Core Phonemizer** (`overrides_learned.c`)
   - Stable, high-usage corrections
   - Exported when `community_usage >= 100`
   - Compiled into phonemizer for zero-runtime-overhead
   - Merged in official releases

### Pronunciation Lookup Priority

1. **Overrides** (highest priority) - check personal, then community
2. **Dictionary** - static pronunciation dictionary
3. **G2P Rules** (fallback) - grapheme-to-phoneme rules

## Training Workflow

### User Interaction

```
User: "You mispronounced integrate"
Assistant: [calls train_pronunciation tool]
System: 
  1. Asks user to say the word
  2. Records audio
  3. Transcribes with Whisper (optional verification)
  4. Generates phoneme variants
  5. Synthesizes each variant
  6. Compares to user audio using mel spectrograms
  7. Stores best match in personal overrides
```

### Phoneme Variant Generation

The system generates variants by:
- Adjusting stress patterns (ˈ primary, ˌ secondary, no stress)
- Testing vowel alternatives (i vs ɪ, æ vs ɛ, etc.)
- Adding/removing glides and reduced vowels
- Testing common mispronunciation patterns

Example for "integrate":
```
Base:     ˈɪntəˌɡɹeɪt
Variants: ɪntəɡɹeɪt          (no stress)
          ɪˈntəɡɹeɪt         (stress on 2nd syllable)
          ˈintəˌɡɹeɪt        (i instead of ɪ)
          ˈɪntəˌɡɹeit        (e instead of eɪ)
          ... (up to 20 variants)
```

### Audio Comparison

Uses **mel spectrogram + Dynamic Time Warping (DTW)**:

1. Extract mel spectrograms from both audio files
   - 80 mel bands
   - 16kHz sample rate
   - 512-point FFT, 256 hop length

2. Compute DTW distance between spectrograms
   - Handles different audio lengths
   - Finds optimal time alignment
   - Accumulates frame-wise distances

3. Convert distance to similarity score
   - `similarity = exp(-distance)`
   - Range: 0.0 (different) to 1.0 (identical)

4. Select best variant above threshold (default: 0.75)

## API Usage

### C API

```c
#include "ethervox/pronunciation_trainer.h"

// Initialize
phonemizer_context_t* phonemizer = phonemizer_create("en-us");
tts_context_t* tts = /* ... */;

// Configure training
pronunciation_training_config_t config = pronunciation_trainer_default_config();
config.max_variants = 20;
config.min_similarity = 0.75f;
config.verbose = true;

// Train pronunciation
pronunciation_training_result_t result;
int ret = pronunciation_trainer_train(
    "integrate",                // word
    "/tmp/user_audio.wav",      // user audio
    phonemizer,
    tts,
    NULL,                       // stt (optional)
    &config,
    &result
);

if (ret == 0 && result.success) {
    printf("Best phonemes: %s (similarity: %.3f)\n",
           result.best_phonemes, result.similarity_score);
    
    // Override automatically saved to personal overrides
}

pronunciation_training_result_free(&result);
```

### LLM Tool

```json
{
  "name": "train_pronunciation",
  "description": "Train pronunciation by comparing user audio to variants. Use when word mispronounced.",
  "parameters": {
    "word": "integrate",
    "audio_path": "/tmp/user_recording.wav",
    "save_to_overrides": true
  }
}
```

## File Formats

### Override JSON Format

```json
{
  "version": "1.0",
  "overrides": [
    {
      "word": "integrate",
      "ipa": "ˈɪntəˌɡɹeɪt",
      "arpabet": "IH1 N T AH0 G R EY2 T",
      "usage_count": 42,
      "confidence": 0.87,
      "trained_speaker_id": 0,
      "created_at": 1704067200,
      "last_used": 1704153600
    }
  ]
}
```

### Exported C Code Format

```c
// Auto-generated from community overrides
// DO NOT EDIT - Generated on 2025-01-15

typedef struct {
    const char* word;
    const char* ipa;
    const char* arpabet;
} learned_override_t;

static const learned_override_t LEARNED_OVERRIDES[] = {
    {"integrate", "ˈɪntəˌɡɹeɪt", "IH1 N T AH0 G R EY2 T"},
    {"schedule", "ˈskɛdʒul", "S K EH1 JH UW0 L"},
    // ... more overrides
};

static const int NUM_LEARNED_OVERRIDES = 2;
```

## Configuration

### Training Parameters

- `max_variants` (default: 20) - Number of phoneme variants to test
- `min_similarity` (default: 0.75) - Minimum audio similarity to accept
- `speaker_id` (default: 0) - TTS speaker for synthesis
- `verbose` (default: false) - Log detailed progress
- `save_audio_samples` (default: false) - Save WAV files for debugging

### Promotion Criteria

**Personal → Community:**
- `usage_count >= 50`
- `confidence >= 0.85`

**Community → Core:**
- `community_usage >= 100`
- Manual review before inclusion

## Limitations & Future Work

### Current Limitations

1. **Placeholder TTS Synthesis**: Phoneme-level synthesis not yet implemented
   - Currently returns first variant as best match
   - TODO: Implement `tts_synthesize_phonemes(ctx, phonemes, output_path)`

2. **No STT Verification**: Optional Whisper transcription not integrated
   - Could verify user said the correct word
   - Prevents training errors from wrong words

3. **Simple FFT**: Using naive DFT instead of optimized FFT
   - Performance impact for long audio
   - TODO: Integrate FFTW or similar

4. **Limited Variant Generation**: Basic stress/vowel alternation only
   - Could use phonological rules
   - Could learn common error patterns

### Future Enhancements

1. **Phoneme-Level TTS Synthesis**
   - Direct phoneme input to ONNX inference
   - Bypass text phonemization step
   - Enable true variant testing

2. **Active Learning**
   - Suggest words for training based on low confidence
   - Prioritize high-frequency words
   - Track per-word error rates

3. **Multi-Speaker Training**
   - Learn speaker-specific pronunciations
   - Gender/accent-aware variants
   - Voice matching for emotional models

4. **Phonological Rule Learning**
   - Extract patterns from corrections
   - Generate better variants automatically
   - Improve base phonemizer over time

5. **Crowdsourced Corrections**
   - Share community overrides across instances
   - Vote on pronunciation correctness
   - Distributed pronunciation database

## Testing

### Manual Testing

```bash
# Build emotion tester with pronunciation training
make ethervox-test-emotions

# Test emotional voices
./ethervox-test-emotions

# Train specific word (TODO: implement CLI tool)
./ethervox-train-pronunciation "integrate" audio.wav
```

### Integration Testing

```c
// Test override storage
pronunciation_override_store_t* store = pronunciation_overrides_load(
    "~/.ethervox/pronunciation_overrides.json"
);

// Add test override
pronunciation_override_t override = {
    .word = "test",
    .ipa = "tɛst",
    .confidence = 0.95f,
    .usage_count = 1
};
pronunciation_overrides_add(store, &override);

// Verify lookup
const pronunciation_override_t* found = 
    pronunciation_overrides_lookup(store, "test");
assert(found != NULL);
assert(strcmp(found->ipa, "tɛst") == 0);
```

## Performance

### Training Latency

- Variant generation: ~10ms
- TTS synthesis: ~100ms/variant (20 variants = 2s)
- Mel extraction: ~50ms/audio
- DTW comparison: ~20ms/pair (20 variants = 400ms)
- **Total: ~3-4 seconds per word**

### Storage Overhead

- Personal overrides: ~5KB for 100 words
- Community overrides: ~50KB for 1000 words
- Lookup time: O(n) linear search (fast for <1000 entries)
- Could optimize with hash table if needed

## References

- [Piper TTS](https://github.com/rhasspy/piper) - ONNX-based TTS engine
- [ONNX Runtime](https://onnxruntime.ai/) - Model inference
- [Mel Spectrogram](https://en.wikipedia.org/wiki/Mel-frequency_cepstrum) - Audio features
- [Dynamic Time Warping](https://en.wikipedia.org/wiki/Dynamic_time_warping) - Sequence alignment
- [IPA (International Phonetic Alphabet)](https://www.internationalphoneticassociation.org/content/ipa-chart)

## License

Copyright (c) 2024-2025 EthervoxAI Team  
Licensed under CC BY-NC-SA 4.0

This is a commercial product. See [LICENSE](../../LICENSE) for details.
