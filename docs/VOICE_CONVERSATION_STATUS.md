# Voice Conversation System - Implementation Status

## Overview

The voice conversation system provides real-time voice interaction with the LLM through a separate pipeline from transcription.

## Architecture

### Two Voice Pipelines

**Pipeline 1: Transcription** (Existing - Whisper STT)
- Purpose: Meeting transcription, long-form dictation
- Trigger: Manual `/transcribe` command
- STT Backend: Whisper.cpp (base model, 74MB)
- Speed: ~4x realtime on M1
- Session: `g_transcription_session`
- Use Case: Record 10-60 minute meetings, save to `~/.ethervox/transcripts/`

**Pipeline 2: Conversation** (NEW - Vosk STT + Piper TTS)
- Purpose: Natural LLM interaction (replaces keyboard)
- Trigger: Wake word detection
- STT Backend: Vosk (Apache 2.0, 50MB English model) - **TO IMPLEMENT**
- TTS Backend: Piper (MIT, neural vocoder) - **TO IMPLEMENT**
- Speed: <500ms total latency
- Session: `g_conversation_session`
- Use Case: "Hey Ethervox, what's the weather?" → LLM responds with voice

## Implementation Status

### ✅ Completed (December 11, 2025)

1. **Conversation Session Infrastructure**
   - Created `include/ethervox/conversation.h` with full API
   - Implemented `src/dialogue/voice_conversation.c` with background thread
   - Thread states: IDLE → LISTENING → PROCESSING → SPEAKING → IDLE
   - Thread management with pthread mutex/condition variables
   - Added `g_conversation_session` global variable

2. **CLI Commands**
   - `/conversation` - Show conversation session status
   - `/convon` - Enable voice conversation
   - `/convoff` - Disable voice conversation

3. **Build Integration**
   - CMakeLists.txt updated to include voice_conversation.c
   - Successfully compiles and links

### 🔄 In Progress

**None** - Infrastructure complete, ready for backend implementation

### ⏳ Pending

1. **Vosk STT Backend** (Task #3)
   - Download/integrate Vosk library
   - Create `src/stt/vosk_backend.c`
   - Load model from `~/.ethervox/models/vosk/`
   - Implement real-time streaming recognition
   - Expected: ~50MB model, <0.3x realtime latency

2. **Wake Word → Conversation Integration** (Task #4)
   - Modify wake word detector to call `ethervox_conversation_trigger()`
   - Add audio feedback beeps on wake detection
   - Implement timeout to return to listening

3. **Piper TTS Backend** (Task #5)
   - Download/integrate Piper + onnxruntime
   - Create `src/tts/piper_backend.c`
   - Load voice model from `~/.ethervox/models/piper/`
   - Implement text → audio synthesis
   - Expected: 10-50MB models, >1.0x realtime

4. **End-to-End Flow** (Task #6)
   - Connect wake word → Vosk → Governor → Piper
   - Add conversation loop (return to listening after response)
   - Test complete voice interaction

## Thread Architecture

```
Main Thread (CLI + Governor)
  │
  ├─> Wake Word Thread (background, always-on if enabled)
  │     └─> Triggers Conversation Thread
  │
  ├─> Conversation Thread (Vosk STT → Governor → Piper TTS)
  │     └─> Returns to idle after timeout
  │
  └─> Transcription Thread (Whisper STT → File)
        └─> Manual /transcribe command
```

## API Reference

### Conversation Session

```c
// Initialize session
ethervox_conversation_config_t config = ethervox_conversation_get_default_config();
ethervox_conversation_session_t* session = ethervox_conversation_init(&config, governor);

// Start background thread
ethervox_conversation_start(session);

// Trigger from wake word
ethervox_conversation_trigger(session);

// Query state
ethervox_conversation_state_t state = ethervox_conversation_get_state(session);
bool active = ethervox_conversation_is_active(session);

// Stop and cleanup
ethervox_conversation_stop(session);
ethervox_conversation_cleanup(session);
```

### States

- `ETHERVOX_CONV_STATE_UNINITIALIZED` - Not initialized
- `ETHERVOX_CONV_STATE_IDLE` - Waiting for wake word trigger
- `ETHERVOX_CONV_STATE_LISTENING` - Capturing user speech
- `ETHERVOX_CONV_STATE_PROCESSING` - Sending to Governor
- `ETHERVOX_CONV_STATE_SPEAKING` - Playing TTS response
- `ETHERVOX_CONV_STATE_ERROR` - Error state

## Configuration

Default configuration (from `ethervox_conversation_get_default_config()`):

```c
// Vosk STT
config.vosk.sample_rate = 16000.0f;
config.vosk.max_alternatives = 1;
config.vosk.enable_partial_results = true;

// Piper TTS
config.piper.speed = 1.0f;
config.piper.sample_rate = 22050;

// Timeouts
config.listen_timeout_ms = 5000;        // 5s silence → stop listening
config.conversation_timeout_ms = 30000; // 30s max conversation

// Audio feedback
config.enable_beep_on_wake = true;
config.enable_beep_on_listen_end = true;
```

## Usage Example

```bash
# Start EthervoxAI
./build/ethervoxai

# Check conversation status
> /conversation
Status: Not initialized
💡 Use /convon to enable voice conversation

# Enable conversation
> /convon
✓ Voice conversation ENABLED
  Listening for wake word triggers...
💡 Make sure wake word detection is enabled (/wakeon)

# Enable wake word
> /wakeon
✓ Wake word detection ENABLED

# Now say "Hey Ethervox" to trigger conversation!
# (Currently shows infrastructure, Vosk/Piper backends pending)

# Disable conversation
> /convoff
✓ Voice conversation DISABLED

# Check status
> /conversation
Status: Idle (waiting for wake word)
Active: No
```

## Dependencies

### Required (Pending Integration)

- **Vosk** (Apache 2.0)
  - C API for speech recognition
  - Download: https://alphacephei.com/vosk/
  - Model: `vosk-model-en-us-0.22` (~50MB)
  - Location: `~/.ethervox/models/vosk/`

- **Piper** (MIT)
  - Neural TTS with onnxruntime
  - Download: https://github.com/rhasspy/piper
  - Voice models: en_US-lessac-medium (~17MB)
  - Location: `~/.ethervox/models/piper/`

- **onnxruntime** (MIT)
  - Required by Piper for model inference
  - ~30MB library

### Total Addition
~150-200MB (Vosk + Piper + onnxruntime + models)

## Performance Expectations

- **Wake word CPU**: ~8% continuous
- **Conversation CPU**: 30-40% when active, <1% idle
- **Latency**: <2 seconds total (wake → response)
  - Wake detection: <100ms
  - Vosk STT: <500ms
  - Governor: ~1s
  - Piper TTS: <300ms

## Testing

Currently implemented commands for testing infrastructure:

```bash
# Test conversation session lifecycle
/conversation  # Show status
/convon        # Initialize and start
/conversation  # Verify running
/convoff       # Stop
```

Once Vosk/Piper are integrated:

```bash
# Full voice conversation test
/wakeon        # Enable wake word
/convon        # Enable conversation
# Say: "Hey Ethervox, what time is it?"
# Expected: Voice response with current time
```

## Next Steps

See todo list for detailed implementation plan:

1. ✅ Rename g_cli_voice_session → g_transcription_session (DONE)
2. ✅ Create conversation session infrastructure (DONE)
3. ⏳ Implement Vosk STT backend
4. ⏳ Add conversation thread with wake word trigger
5. ⏳ Implement Piper TTS integration
6. ⏳ Wire conversation flow end-to-end

## Files Modified/Created

### New Files
- `include/ethervox/conversation.h` (186 lines)
- `src/dialogue/voice_conversation.c` (328 lines)
- `test_conversation_commands.sh` (test script)

### Modified Files
- `src/main.c` - Added conversation commands, cleanup, global variable
- `CMakeLists.txt` - Include voice_conversation.c in build

## License

Same as EthervoxAI core (CC BY-NC-SA 4.0)
