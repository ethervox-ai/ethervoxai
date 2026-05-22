# Vosk STT Backend Implementation

## Overview

Vosk provides real-time speech recognition for voice conversations. It's significantly faster than Whisper (~0.3x realtime vs 4x realtime) and uses less memory (~50MB vs 74MB).

## Status

✅ **Backend Implemented** - vosk_backend.c complete with full streaming API
⏳ **Library Integration Pending** - Requires Vosk C library + model download

## Implementation Details

### Files Created/Modified

**New:**
- `src/stt/vosk_backend.c` (415 lines) - Complete Vosk backend implementation
- `scripts/download-vosk-model.sh` - Model download helper

**Modified:**
- `include/ethervox/stt.h` - Added Vosk function declarations
- `src/stt/stt_core.c` - Integrated Vosk backend into STT core

### API Functions

```c
// Initialize Vosk model and recognizer
int ethervox_stt_vosk_init(ethervox_stt_runtime_t* runtime);

// Start recognition session
int ethervox_stt_vosk_start(ethervox_stt_runtime_t* runtime);

// Process audio frame (streaming)
int ethervox_stt_vosk_process(ethervox_stt_runtime_t* runtime,
                               const ethervox_audio_buffer_t* audio_buffer,
                               ethervox_stt_result_t* result);

// Finalize and get final result
int ethervox_stt_vosk_finalize(ethervox_stt_runtime_t* runtime,
                                ethervox_stt_result_t* result);

// Stop recognition
void ethervox_stt_vosk_stop(ethervox_stt_runtime_t* runtime);

// Cleanup resources
void ethervox_stt_vosk_cleanup(ethervox_stt_runtime_t* runtime);
```

### Usage

```c
// Configure for Vosk
ethervox_stt_config_t config = ethervox_stt_get_default_config();
config.backend = ETHERVOX_STT_BACKEND_VOSK;
config.model_path = "~/.ethervox/models/vosk/vosk-model-small-en-us-0.15";
config.enable_partial_results = true;

// Initialize
ethervox_stt_runtime_t runtime;
ethervox_stt_init(&runtime, &config);

// Start recognition
ethervox_stt_start(&runtime);

// Process audio frames
ethervox_stt_result_t result;
while (processing) {
    ethervox_audio_buffer_t audio;
    // ... get audio ...
    
    int ret = ethervox_stt_process(&runtime, &audio, &result);
    if (ret == 0) {
        // Got result (partial or final)
        printf("%s: %s\n", result.is_final ? "FINAL" : "PARTIAL", result.text);
        ethervox_stt_result_free(&result);
    }
}

// Finalize
ethervox_stt_finalize(&runtime, &result);
printf("Final: %s\n", result.text);
ethervox_stt_result_free(&result);

// Cleanup
ethervox_stt_cleanup(&runtime);
```

## Integration Steps

### 1. Download Vosk Library

```bash
# macOS (Homebrew - if available)
brew install vosk

# Or build from source
git clone https://github.com/alphacep/vosk-api
cd vosk-api/src
make
```

### 2. Download Model

```bash
./scripts/download-vosk-model.sh
```

This downloads `vosk-model-small-en-us-0.15` (~40MB) to `~/.ethervox/models/vosk/`.

**Other models available:**
- **vosk-model-small-en-us-0.15** - 40MB, fast, good for conversations
- **vosk-model-en-us-0.22** - 1.8GB, high accuracy
- **vosk-model-en-us-0.22-lgraph** - 128MB, medium accuracy

See: https://alphacephei.com/vosk/models

### 3. Build with Vosk Support

```bash
# Add to CMakeLists.txt or build flags
cmake -DVOSK_AVAILABLE=1 -DVOSK_INCLUDE_DIR=/path/to/vosk/include \
      -DVOSK_LIBRARY=/path/to/vosk/lib/libvosk.a ..
cmake --build .
```

### 4. CMakeLists.txt Integration

Add to CMakeLists.txt:

```cmake
# Vosk STT support (optional)
option(WITH_VOSK "Enable Vosk STT backend" OFF)

if(WITH_VOSK)
    find_path(VOSK_INCLUDE_DIR vosk_api.h
        HINTS /usr/local/include /opt/homebrew/include
    )
    
    find_library(VOSK_LIBRARY NAMES vosk libvosk
        HINTS /usr/local/lib /opt/homebrew/lib
    )
    
    if(VOSK_INCLUDE_DIR AND VOSK_LIBRARY)
        message(STATUS "Vosk found:")
        message(STATUS "  Include: ${VOSK_INCLUDE_DIR}")
        message(STATUS "  Library: ${VOSK_LIBRARY}")
        
        add_definitions(-DVOSK_AVAILABLE=1)
        include_directories(${VOSK_INCLUDE_DIR})
        list(APPEND ETHERVOXAI_LIBS ${VOSK_LIBRARY})
    else()
        message(WARNING "Vosk requested but not found - backend will be disabled")
    endif()
endif()
```

## Current Build Status

✅ Compiles successfully with stubs (VOSK_AVAILABLE not defined)
⏳ Awaits Vosk library integration

When compiled without Vosk library, backend functions return error:
```
ERROR: Vosk backend not available - recompile with VOSK_AVAILABLE=1
```

## Performance Expectations

Based on Vosk documentation and benchmarks:

- **Latency**: ~200-300ms (0.3x realtime)
- **Memory**: ~50MB model + ~20MB runtime
- **CPU**: 20-30% single core on M1
- **Accuracy**: ~90% WER for small models, ~95% for large models

Comparison with Whisper:
- **Speed**: 10x faster than Whisper (~0.3x vs ~4x realtime)
- **Accuracy**: Slightly lower (90% vs 95%) for small models
- **Memory**: Comparable for small models (50MB vs 74MB)
- **Use Case**: Real-time conversation vs high-accuracy transcription

## Testing

Once library is integrated:

```bash
# Test Vosk backend
./build/ethervoxai
> /conversation
> /convon
> /wakeon
# Say: "Hey Ethervox"
# Vosk will process speech in real-time
```

## Next Steps

1. ✅ Vosk backend implementation (DONE)
2. ⏳ Download/integrate Vosk library
3. ⏳ Add CMake detection and linking
4. ⏳ Connect to conversation session in voice_conversation.c
5. ⏳ Test end-to-end voice conversation flow

## Dependencies

- **Vosk API**: Apache 2.0 license (commercial-friendly)
- **Model**: Apache 2.0 license
- **Size**: 40MB-1.8GB depending on model choice
- **Source**: https://github.com/alphacep/vosk-api

## Architecture Integration

```
Wake Word Detection
  ↓
Conversation Trigger (ethervox_conversation_trigger)
  ↓
Voice Conversation Thread
  ↓
Vosk STT (streaming, real-time) ← YOU ARE HERE
  ↓
Governor (generate response)
  ↓
Piper TTS (speak response)
  ↓
Return to wake word listening
```

## License

Vosk backend implementation: CC BY-NC-SA 4.0 (same as EthervoxAI)
Vosk library: Apache 2.0
