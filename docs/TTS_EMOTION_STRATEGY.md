# TTS Emotion & Quality Strategy for EthervoxAI

## Problem Analysis

**Current State:**
- ✅ GPL-free custom phonemizer (CMU Dict, Unicode Unihan, rule-based German)
- ✅ Piper TTS backend with ONNX Runtime
- ❌ Piper models lack emotional expression
- ❌ Flat prosody from audiobook training data

**What We Need:**
- 🎭 Emotional/expressive speech
- 🎵 Natural prosody and intonation
- 🚀 C/C++ compatible
- ⚖️ License-compatible (no GPL)

## Why StyleTTS2 Won't Work

| Issue | Details |
|-------|---------|
| **GPL Dependency** | Still uses espeak-ng for phonemization |
| **PyTorch Only** | No ONNX export, requires Python runtime |
| **Heavy Dependencies** | BERT, ASR models, pitch extractors (hundreds of MB) |
| **Mobile Incompatible** | Too large for Android/embedded systems |

## Recommended Solution: Enhanced Piper with Emotional Models

### Option A: Expressive Piper Models (BEST FIT)

Use **emotionally-trained Piper models** with our custom phonemizer:

```
Your GPL-free phonemizer → Emotional Piper ONNX → Expressive speech
```

**Available Emotional Piper Models:**
1. **thorsten_emotional** (German, medium quality)
   - Trained on expressive acted speech
   - Multiple emotion styles
   - Already in Piper format

2. **LibriTTS-R** (English, high quality)
   - Multi-speaker with prosody variation
   - Better expressiveness than LJSpeech

3. **VCTK** (English, multi-speaker)
   - 109 speakers with different speaking styles
   - Natural prosody variance

**Implementation:**
- ✅ No code changes needed
- ✅ Drop-in replacement models
- ✅ Works with existing phonemizer
- ✅ Same ONNX Runtime backend

### Option B: ONNX-Compatible Emotional TTS

**Kokoro-82M** (New, December 2024):
- MIT License ✅
- 82M parameters (lightweight)
- ONNX exportable
- Built-in style control
- GitHub: `hexgrad/Kokoro-82M`

**Advantages:**
- Small enough for mobile (82MB model)
- Style/emotion parameters built-in
- Active development
- No espeak dependency

**Challenges:**
- Would need custom integration
- Requires style embedding implementation

### Option C: Prosody Enhancement Layer

Add prosody post-processing to existing Piper output:

```
Text → Phonemizer → Piper ONNX → Prosody Enhancer → Expressive Audio
```

**Components:**
1. **F0 (Pitch) Modulation**
   - Detect emotional cues in text
   - Adjust pitch contours dynamically
   - PSOLA algorithm (lightweight, C++)

2. **Duration Adjustment**
   - Lengthen emphasis words
   - Add emotional pauses
   - Speed variation

3. **Energy Modulation**
   - Volume dynamics
   - Stress patterns

**Libraries:**
- World Vocoder (C++, BSD-3-Clause)
- Praat-inspired algorithms

## Immediate Action Plan

### Phase 1: Model Upgrade (1-2 days)
1. Download emotional Piper models
2. Test with existing phonemizer
3. Benchmark quality vs current models

**Models to try:**
```bash
# LibriTTS-R (English, expressive)
wget https://github.com/rhasspy/piper/releases/download/v1.2.0/voice-en-us-libritts_r-medium.onnx

# Thorsten Emotional (German)
wget https://github.com/rhasspy/piper/releases/download/v1.2.0/voice-de-de-thorsten_emotional-medium.onnx
```

### Phase 2: Runtime Emotion Control (3-5 days)
Add emotion parameters to your TTS API:

```c
typedef enum {
    ETHERVOX_EMOTION_NEUTRAL = 0,
    ETHERVOX_EMOTION_HAPPY,
    ETHERVOX_EMOTION_SAD,
    ETHERVOX_EMOTION_ANGRY,
    ETHERVOX_EMOTION_EXCITED,
    ETHERVOX_EMOTION_CALM
} ethervox_emotion_t;

typedef struct {
    ethervox_tts_backend_t backend;
    ethervox_emotion_t emotion;      // NEW: Emotion control
    float emotion_strength;          // NEW: 0.0-1.0 blend
    float pitch_variance;            // NEW: Pitch modulation
    float energy_variance;           // NEW: Volume dynamics
    int sample_rate;
    // ... existing fields
} ethervox_tts_config_t;
```

Implement emotion via:
- Model selection (different models for different emotions)
- OR prosody post-processing
- OR speaker ID switching (multi-speaker models)

### Phase 3: Prosody Enhancement (1-2 weeks, optional)
If models alone aren't expressive enough:
1. Integrate World Vocoder
2. Add pitch/duration modulation based on text analysis
3. Implement emphasis detection (punctuation, capitalization)

## License Compliance

| Component | License | Status |
|-----------|---------|--------|
| Your phonemizer | CC BY-NC-SA 4.0 | ✅ Original work |
| Piper models | MIT | ✅ Compatible |
| ONNX Runtime | MIT | ✅ Already used |
| World Vocoder | BSD-3-Clause | ✅ Compatible |
| SpeexDSP | BSD-3-Clause | ✅ Already used |

**No GPL dependencies!**

## Performance Impact

| Approach | Model Size | Latency | Quality | Emotion Control |
|----------|------------|---------|---------|-----------------|
| Current Piper | ~30MB | Low | Good | None |
| Emotional Piper | ~50MB | Low | Very Good | Model-based |
| + Prosody Layer | +5MB | +10% | Excellent | Full control |
| StyleTTS2 ❌ | ~500MB+ | High | Excellent | Full ❌ GPL |

## Recommended: Start Simple, Iterate

**Week 1:** Test emotional Piper models
- If quality is good enough → **Ship it**
- If not → Proceed to prosody enhancement

**Week 2-3:** Add prosody control layer (if needed)

**Long-term:** Consider Kokoro-82M when mature

## Implementation Example

```c
// Use different models for different emotions
const char* get_model_for_emotion(ethervox_emotion_t emotion) {
    switch (emotion) {
        case ETHERVOX_EMOTION_HAPPY:
            return "models/en-us-libritts-happy.onnx";
        case ETHERVOX_EMOTION_SAD:
            return "models/en-us-libritts-sad.onnx";
        case ETHERVOX_EMOTION_EXCITED:
            return "models/en-us-libritts-excited.onnx";
        default:
            return "models/en-us-libritts-neutral.onnx";
    }
}

// OR: Modulate prosody post-inference
void apply_emotion_prosody(float* audio, size_t samples, 
                          ethervox_emotion_t emotion,
                          float strength) {
    switch (emotion) {
        case ETHERVOX_EMOTION_HAPPY:
            adjust_pitch(audio, samples, +50Hz * strength);  // Higher pitch
            adjust_speed(audio, samples, 1.1f * strength);    // Faster
            break;
        case ETHERVOX_EMOTION_SAD:
            adjust_pitch(audio, samples, -30Hz * strength);   // Lower pitch
            adjust_speed(audio, samples, 0.9f * strength);     // Slower
            break;
        // ...
    }
}
```

## Next Steps

1. ✅ Document current licensing (done)
2. ✅ Finalize phonemizer architecture (done)
3. ⏭️ **Test emotional Piper models** ← START HERE
4. Evaluate quality vs requirements
5. Implement emotion API if needed

Want me to help you download and test emotional Piper models now?
