# AEC + Piper TTS Implementation Plan

## Overview

Replace macOS `say` command with Piper TTS library to enable acoustic echo cancellation (AEC), preventing the AI from hearing itself speak while still allowing user interruptions.

## Architecture

```
User Speech ─┐
             ├─→ [Microphone] ─→ [AEC Engine] ─→ [Whisper STT] ─→ [Governor]
TTS Output ──┤                        ↑                                   │
             └─────────────────────────┘                                   │
                   Reference Signal                                        │
                                                                           ↓
             ┌─────────────────────────────────────────────────────────────┘
             │
             ├─→ [Piper TTS] ─→ Raw PCM Samples ─┬─→ [AEC Reference Buffer]
             │                                     │
             └─────────────────────────────────────┴─→ [Audio Playback]
```

## Implementation Steps

### 1. Audio Playback Infrastructure (~300 lines) ✅ COMPLETED

**Files:**
- `src/audio/platform_macos.c` - ✅ Replaced stub `write_audio()` with CoreAudio playback
- `src/audio/reference_buffer.c` - ✅ Ring buffer for TTS reference samples
- `include/ethervox/reference_buffer.h` - ✅ Public API for reference buffer

**Completed Tasks:**
- ✅ Implemented CoreAudio queue-based playback in `macos_audio_write()`
- ✅ Created 2-second circular reference buffer (32000 samples @ 16kHz)
- ✅ Added playback state tracking (is_playing, start/stop playback)
- ✅ Thread-safe buffer access with pthread_mutex
- ✅ Separate capture and playback queues

**Implemented APIs:**
```c
// Reference buffer (src/audio/reference_buffer.c)
ethervox_reference_buffer_t* ethervox_reference_buffer_create(size_t capacity);
size_t ethervox_reference_buffer_write(buffer, const float* samples, size_t count);
size_t ethervox_reference_buffer_read(buffer, float* samples, size_t count);
size_t ethervox_reference_buffer_available(buffer);
void ethervox_reference_buffer_clear(buffer);
void ethervox_reference_buffer_destroy(buffer);

// macOS audio playback (src/audio/platform_macos.c)
static int macos_audio_start_playback(ethervox_audio_runtime_t* runtime);
static int macos_audio_stop_playback(ethervox_audio_runtime_t* runtime);
static int macos_audio_write(runtime, const ethervox_audio_buffer_t* buffer);
```

### 2. Speex AEC Integration (~400 lines) ✅ COMPLETED

**Files:**
- `src/audio/aec_speex.c` - ✅ Speex AEC wrapper implementation
- `include/ethervox/aec.h` - ✅ AEC public API

**Dependencies:**
```bash
brew install speex  # macOS
apt-get install libspeex-dev  # Linux
# Note: CMakeLists.txt updated to auto-detect speexdsp via pkg-config
```

**Completed Tasks:**
- ✅ Created `ethervox_aec_t` context structure with Speex echo state
- ✅ Implemented `ethervox_aec_create()` with configurable backends (NONE, SPEEX, WEBRTC)
- ✅ Implemented `ethervox_aec_set_reference()` for TTS output
- ✅ Implemented `ethervox_aec_process()` for microphone input
- ✅ Configured filter_length=1024 samples (~64ms echo tail), frame_size=160 (10ms @ 16kHz)
- ✅ Added Speex preprocessing (noise suppression)
- ✅ Float32 ↔ int16 conversion for Speex compatibility
- ✅ Passthrough mode when AEC is disabled or backend is NONE
- ⏳ TODO: Integrate into `audio_core.c` capture path

**Implemented APIs:**
```c
ethervox_aec_config_t ethervox_aec_default_config(void);
ethervox_aec_t* ethervox_aec_create(const ethervox_aec_config_t* config);
int ethervox_aec_set_reference(ethervox_aec_t* aec, const float* reference, size_t count);
int ethervox_aec_process(ethervox_aec_t* aec, const float* input, float* output, size_t count);
bool ethervox_aec_is_active(const ethervox_aec_t* aec);
void ethervox_aec_reset(ethervox_aec_t* aec);
void ethervox_aec_destroy(ethervox_aec_t* aec);
```

### 3. Piper TTS Backend (~600 lines) ✅ COMPLETED

**Files:**
- `src/tts/piper_backend.c` - ✅ Piper + ONNX Runtime integration
- `src/tts/tts.c` - ✅ Backend dispatcher (Piper/espeak/system)
- `include/ethervox/tts.h` - ✅ TTS backend abstraction
- `scripts/download-piper-model.sh` - ✅ Model downloader

**Dependencies:**
```bash
brew install onnxruntime  # macOS (~15MB)
./scripts/download-piper-model.sh  # Downloads en_US-lessac-medium.onnx (60MB)
```

**Tasks:**
- [ ] Create `ethervox_tts_piper_t` context with ONNX session
- [ ] Load model from `~/.ethervox/models/tts/en_US-lessac-medium.onnx`
- [ ] Parse JSON config for phoneme mappings
- [ ] Implement text → phonemes → ONNX inference → float32 PCM @ 22050Hz
- [ ] Add resampler: 22050Hz → 16kHz (SpeexDSP or libsamplerate)
- [ ] Return `ethervox_audio_buffer_t` with raw samples (no direct playback)

**Key APIs:**
```c
ethervox_tts_piper_t* ethervox_tts_piper_create(const char* model_path, const char* config_path);
int ethervox_tts_piper_synthesize(ethervox_tts_piper_t* tts, const char* text, ethervox_audio_buffer_t* output);
void ethervox_tts_piper_destroy(ethervox_tts_piper_t* tts);
```

### 4. Voice Conversation Refactor (~200 line changes)

**File:** `src/dialogue/voice_conversation.c`

**Changes in `handle_governor_response()` (~line 200):**
```c
// OLD: system("say '...' &");
// NEW:
ethervox_audio_buffer_t tts_buffer = {0};
ethervox_tts_synthesize(tts_ctx, response_text, &tts_buffer);

// Feed to AEC reference
ethervox_aec_set_reference(aec_ctx, tts_buffer.data, tts_buffer.size);

// Start playback
g_tts_is_playing = true;
platform_write_audio(&audio_runtime, &tts_buffer);
```

**Changes in STT processing loop (~line 395):**
```c
while (listening) {
    capture_microphone(&mic_buffer, 480);
    
    // Apply AEC if TTS is playing
    if (g_tts_is_playing) {
        ethervox_aec_process(aec_ctx, mic_buffer.data, mic_buffer.size);
        
        // Check for user interrupt
        float energy = calculate_rms(mic_buffer.data, mic_buffer.size);
        if (energy > tts_baseline_energy * 2.0f) {
            // User interrupting! Stop TTS
            platform_stop_tts(&audio_runtime);
            g_tts_is_playing = false;
        } else {
            continue;  // Ignore echo, don't process with Whisper
        }
    }
    
    // Process with Whisper
    ethervox_stt_process(stt, mic_buffer.data, mic_buffer.size, &result);
}
```

### 5. espeak-ng Fallback (~300 lines)

**Files:**
- `src/tts/espeak_backend.c` - NEW: espeak-ng integration

**Dependencies:**
```bash
brew install espeak-ng  # macOS
apt-get install libespeak-ng-dev  # Linux
```

**Tasks:**
- [ ] Create `ethervox_tts_espeak_t` context
- [ ] Implement same `ethervox_tts_synthesize()` interface as Piper
- [ ] Real-time streaming synthesis (better for AEC)
- [ ] Ultra-lightweight fallback (1-2MB vs Piper's 30MB)
- [ ] Auto-select at runtime based on available resources

**Use cases:**
- Embedded devices (ESP32, low-end Android)
- Minimal binary size requirements
- Piper unavailable or ONNX Runtime missing

### 6. Settings & Configuration (~150 line changes) ✅ COMPLETED

**Files:**
- `include/ethervox/settings.h` - ✅ TTS and AEC structures defined
- `include/ethervox/config.h` - ✅ PIPER_SUBDIR configured
- `src/config/settings.c` - ✅ JSON save/load for TTS+AEC
- `src/common/settings_menu.c` - ✅ TTS engine/voice/speed/volume + AEC settings exposed

**Implemented settings structure:**
```c
// In ethervox_persistent_settings_t
typedef struct {
    bool enable_aec;                      // Enable acoustic echo cancellation
    ethervox_aec_backend_t aec_backend;   // SPEEX, WEBRTC
    ethervox_tts_backend_t tts_backend;   // PIPER, ESPEAK, SYSTEM
    float interrupt_threshold_multiplier;  // 2.0 = need 2x louder than TTS to interrupt
} ethervox_aec_settings_t;
```

**Config defaults (config.h):**
```c
#define ETHERVOX_AEC_ENABLED true
#define ETHERVOX_AEC_BACKEND_DEFAULT ETHERVOX_AEC_SPEEX
#define ETHERVOX_TTS_BACKEND_DEFAULT ETHERVOX_TTS_PIPER
#define ETHERVOX_AEC_INTERRUPT_THRESHOLD 2.0f
```

**Menu items:**
- Enable/Disable AEC
- AEC Backend selection (Speex/WebRTC)
- TTS Engine selection (Piper/espeak-ng/System)
- Interrupt sensitivity (1.5-3.0x multiplier)

### 7. Cross-Platform Audio Playback (~400 lines per platform)

**Linux (`src/audio/platform_linux.c`):**
- [ ] PulseAudio simple API or ALSA PCM output
- [ ] Ring buffer for reference signal
- [ ] Playback state tracking

**Windows (`src/audio/platform_windows.c`):**
- [ ] WASAPI exclusive mode or shared mode
- [ ] COM initialization for audio endpoint
- [ ] Reference buffer management

**Android (`implementations/android/jni/audio_android.c`):**
- [ ] AAudio stream for low-latency playback
- [ ] Built-in AAudio AEC alternative
- [ ] Piper optional for high-end devices only

### 8. Build System Updates (~50 line changes)

**CMakeLists.txt:**
```cmake
# Optional dependencies
option(ETHERVOX_WITH_AEC "Enable acoustic echo cancellation" ON)
option(ETHERVOX_WITH_PIPER "Enable Piper TTS (requires ONNX Runtime)" ON)

if(ETHERVOX_WITH_AEC)
    find_package(Speex REQUIRED)
    target_link_libraries(ethervoxai PRIVATE Speex::Speex)
    target_compile_definitions(ethervoxai PRIVATE ETHERVOX_WITH_AEC=1)
endif()

if(ETHERVOX_WITH_PIPER)
    find_package(OnnxRuntime REQUIRED)
    target_link_libraries(ethervoxai PRIVATE OnnxRuntime::OnnxRuntime)
    target_compile_definitions(ethervoxai PRIVATE ETHERVOX_WITH_PIPER=1)
endif()
```

**Platform-specific:**
```bash
# macOS
brew install speex onnxruntime espeak-ng libsamplerate

# Linux
sudo apt-get install libspeex-dev onnxruntime-dev espeak-ng libsamplerate0-dev

# Android (NDK)
# Add ONNX Runtime AAR to gradle dependencies
# Build Speex from source for ARM/ARM64
```

## Decision Points

### 1. Default TTS Engine

**Options:**
- A) Piper (high quality, +30MB binary)
- B) espeak-ng (lightweight, robotic quality)

**Recommendation:** Piper for desktop, espeak-ng for embedded. Auto-detect at runtime.

### 2. Sample Rate Strategy

**Issue:** Piper outputs 22050Hz, Whisper expects 16000Hz, microphone may be 48000Hz.

**Options:**
- A) Force all components to 16kHz (add resampling everywhere)
- B) Use native rates, resample only at boundaries

**Recommendation:** Option A - standardize on 16kHz for simplicity. Use SpeexDSP resampler (already have Speex dependency).

### 3. Android TTS Strategy

**Issue:** ONNX Runtime adds 15-30MB to APK.

**Options:**
- A) Piper on all Android devices
- B) Built-in Android TTS + AAudio AEC
- C) Piper only on high-end, built-in for mid/low-end

**Recommendation:** Option C - detect device capabilities at runtime. Use Piper if >4GB RAM + >100MB free storage.

### 4. AEC Backend Default

**Options:**
- A) Speex AEC (simpler, adequate)
- B) WebRTC AEC (better quality, more complex)

**Recommendation:** Start with Speex, add WebRTC as optional backend later. Most users won't notice difference.

### 5. Testing Strategy

**Challenges:**
- Need real acoustic environment
- Hard to automate AEC quality testing
- False negatives (blocking real user speech) are worse than false positives

**Approach:**
1. Unit tests: Verify APIs work correctly
2. Synthetic tests: Mix TTS + user speech WAV files, verify AEC removes TTS
3. Manual testing: Real-world usage with various acoustic conditions
4. A/B testing: Compare with/without AEC in conversation mode

## Implementation Timeline

**Week 1: Core Infrastructure**
- Day 1-2: Audio playback (macOS CoreAudio)
- Day 3-4: Speex AEC integration
- Day 5: Reference buffer management

**Week 2: Piper TTS**
- Day 1-2: ONNX Runtime integration
- Day 3-4: Piper backend implementation
- Day 5: Sample rate conversion

**Week 3: Integration & Testing**
- Day 1-2: Voice conversation refactor
- Day 3: Settings & configuration
- Day 4-5: Testing & bug fixes

**Total: ~3 weeks for full implementation**

## Success Criteria

- [ ] TTS playback with raw PCM sample capture
- [ ] AEC removes TTS echo from microphone input
- [ ] User can interrupt AI while it's speaking (energy detection works)
- [ ] No audio glitches or latency issues
- [ ] Cross-platform support (macOS, Linux, Windows)
- [ ] Settings menu for AEC enable/disable
- [ ] Fallback to system TTS if dependencies unavailable
- [ ] Documentation updated

## Future Enhancements

1. **WebRTC AEC Backend** - Higher quality echo cancellation
2. **RNNoise Integration** - Remove background noise in addition to echo
3. **Coqui XTTS** - Streaming neural TTS (lower latency than Piper)
4. **Voice Activity Detection (VAD)** - Smarter interrupt detection
5. **Multi-speaker Support** - Track multiple reference signals
6. **iOS Support** - Use AVAudioEngine for playback + AEC
7. **ESP32 Support** - Ultra-lightweight espeak-ng only

## References

- Piper TTS: https://github.com/rhasspy/piper
- Speex AEC: https://www.speex.org/docs/api/speex-api-reference/
- ONNX Runtime: https://onnxruntime.ai/docs/api/c/
- WebRTC APM: https://webrtc.googlesource.com/src/+/refs/heads/main/modules/audio_processing/
- CoreAudio: https://developer.apple.com/documentation/coreaudio
