# AEC Implementation - Getting Started

## Progress Summary

### ✅ Completed (Phase 1: Infrastructure)

1. **Reference Buffer** (`src/audio/reference_buffer.c`, 200 lines)
   - Thread-safe circular buffer for TTS reference signal
   - 32000-sample capacity (2 seconds @ 16kHz)
   - Write operation for TTS playback thread
   - Read operation for microphone capture thread

2. **Speex AEC Engine** (`src/audio/aec_speex.c`, 320 lines)
   - Full wrapper around Speex echo cancellation
   - 160-sample frames (10ms @ 16kHz)
   - 1024-sample filter length (~64ms echo tail)
   - Supports NONE, SPEEX, WEBRTC backends
   - Includes noise suppression preprocessing

3. **macOS Audio Playback** (`src/audio/platform_macos.c`)
   - CoreAudio queue-based playback implementation
   - Separate capture and playback queues
   - Playback start/stop state management
   - Ready for TTS integration

4. **Build System** (`src/audio/CMakeLists.txt`)
   - Auto-detects Speex DSP via pkg-config
   - Warns if missing (falls back to passthrough mode)
   - Compiles successfully without errors

### 🔨 Installation Required

**macOS:**
```bash
brew install speex
```

**Linux:**
```bash
sudo apt-get install libspeex-dev
```

After installation, rebuild:
```bash
npm run build
```

### ⏳ Next Steps (Phase 2: Piper TTS)

1. **Piper TTS Integration** (~600 lines)
   - Create `src/tts/piper_backend.c`
   - Load `en_US-lessac-medium.onnx` model
   - Text → phonemes → ONNX → PCM synthesis
   - Resample 22050Hz → 16kHz

2. **Voice Conversation Refactor** (~200 lines)
   - Replace `system("say '...'")` with Piper
   - Feed TTS samples to reference buffer
   - Apply AEC to microphone input
   - Implement interrupt detection

3. **Settings Integration** (~100 lines)
   - Add AEC enable/disable toggle
   - TTS engine selection (Piper vs espeak-ng)
   - AEC suppression level slider

## Testing AEC Infrastructure

### Build Status
✅ Core code compiles successfully
✅ No linker errors
⚠️ Speex not installed (will use passthrough mode)

### Integration Test Plan

Once Speex is installed:

1. **Reference Buffer Test**
   ```c
   ethervox_reference_buffer_t* buf = ethervox_reference_buffer_create(32000);
   // Write 1000 samples
   float samples[1000] = {0.5f, 0.3f, ...};
   size_t written = ethervox_reference_buffer_write(buf, samples, 1000);
   // Read back
   float output[1000];
   size_t read = ethervox_reference_buffer_read(buf, output, 1000);
   assert(written == 1000 && read == 1000);
   ```

2. **AEC Passthrough Test** (no Speex)
   ```c
   ethervox_aec_config_t cfg = ethervox_aec_default_config();
   cfg.backend = ETHERVOX_AEC_NONE;
   ethervox_aec_t* aec = ethervox_aec_create(&cfg);
   assert(!ethervox_aec_is_active(aec));
   ```

3. **AEC Active Test** (with Speex)
   ```c
   ethervox_aec_config_t cfg = ethervox_aec_default_config();
   ethervox_aec_t* aec = ethervox_aec_create(&cfg);
   assert(ethervox_aec_is_active(aec));
   
   float ref[160] = {...};  // TTS samples
   float mic[160] = {...};  // Mic input (contains echo)
   float out[160];
   
   ethervox_aec_set_reference(aec, ref, 160);
   ethervox_aec_process(aec, mic, out, 160);
   // out[] should have echo removed
   ```

## Architecture Notes

### Audio Flow (Target)
```
TTS Text → Piper → Raw PCM ─┬─→ Reference Buffer → AEC Reference Input
                              │
                              └─→ Audio Playback → Speakers

Microphone → Raw PCM → AEC Process (with reference) → Whisper STT
```

### AEC Frame Alignment
- **Frame Size**: 160 samples (10ms @ 16kHz)
- **Reference Signal**: Must arrive BEFORE microphone hears echo
- **Latency Budget**: ~10-20ms between TTS generation and mic capture
- **Buffer Size**: 2 seconds allows flexible alignment

### Sample Rate Standardization
- **EthervoxAI Standard**: 16kHz throughout
- **Piper Native**: 22050Hz (requires resampling)
- **Resampler**: Will use libsamplerate or SpeexDSP resampler

## Files Created

1. `include/ethervox/aec.h` (120 lines) - AEC public API
2. `include/ethervox/reference_buffer.h` (100 lines) - Reference buffer API
3. `src/audio/reference_buffer.c` (200 lines) - Ring buffer implementation
4. `src/audio/aec_speex.c` (320 lines) - Speex AEC wrapper
5. `docs/AEC_PIPER_IMPLEMENTATION_PLAN.md` (350 lines) - Full 3-week plan

## Files Modified

1. `src/audio/platform_macos.c` - Added CoreAudio playback support
2. `src/audio/CMakeLists.txt` - Added AEC files + Speex detection
3. `docs/AEC_PIPER_IMPLEMENTATION_PLAN.md` - Marked Phase 1 complete

## License Note

Speex is BSD-licensed, safe for commercial use in EthervoxAI (CC BY-NC-SA 4.0).
