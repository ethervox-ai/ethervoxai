# Voice Conversation Integration - Tool-Based Architecture

## Overview

EthervoxAI implements a **tool-based conversational AI architecture** where the LLM explicitly controls speech input/output, enabling natural turn-taking and bidirectional interruption.

**Key Innovation**: The LLM knows where input came from (voice vs CLI) and can decide how to respond (speak, print, or both) using tools.

## Architecture Highlights

✅ **Context-Aware LLM** - Knows if input was spoken or typed  
✅ **Explicit Output Control** - LLM calls `speak` tool for TTS  
✅ **Natural Turn-Taking** - LLM asks question → auto-listens for answer  
✅ **Bidirectional Interruption** - User can interrupt TTS at any time  
✅ **Flexible Routing** - Same LLM works for voice conversations and CLI queries

## Quick Start

```bash
# Start EthervoxAI
./build/ethervoxai

# Load a model
> /load model.gguf

# Enable conversation with voice mode
> /convon
> /wakeon

# Say "hey ethervox" then ask a question
# LLM will respond using speak tool (TTS)

# Or type a question
> What's 2+2?
# LLM responds with text (no TTS)
```

## Current Status

| Component | Status | Priority |
|-----------|--------|----------|
| Execution Context | ✅ Complete | - |
| Governor API Extended | ✅ Complete | - |
| RMS Energy Helper | ✅ Complete | - |
| Documentation | ✅ Complete | - |
| `speak` Tool | ⏳ In Progress | HIGH |
| `listen` Tool | ⏳ In Progress | HIGH |
| llama Abort Fix | ⏳ TODO | HIGH |
| Interrupt Detection | ⏳ TODO | MEDIUM |
| System Prompt Update | ⏳ TODO | MEDIUM |
| Code Deduplication | ⏳ TODO | LOW |

## New Architecture: Tool-Based Conversational AI

### Overview

EthervoxAI now uses a **tool-based conversational architecture** where the LLM has explicit control over speech output and input, enabling:

✅ **Natural turn-taking** - LLM asks question → automatically listens for answer  
✅ **Bidirectional interruption** - User can interrupt TTS at any time  
✅ **Context awareness** - LLM knows if input was spoken or typed  
✅ **Flexible output routing** - LLM chooses to speak, print, or both  

### Architecture Components

#### 1. Execution Context (`governor.h`)

```c
typedef enum {
    ETHERVOX_INPUT_SOURCE_CLI,      // Typed in terminal
    ETHERVOX_INPUT_SOURCE_VOICE,    // Voice conversation
    ETHERVOX_INPUT_SOURCE_API       // External API
} ethervox_input_source_t;

typedef struct {
    ethervox_input_source_t source;
    bool tts_available;
    bool microphone_available;
    ethervox_conversation_turn_t current_turn;
    ethervox_conversation_callbacks_t* callbacks;
} ethervox_execution_context_t;
```

**Usage**: Passed to `ethervox_governor_execute_with_context()` to inform LLM about conversation state.

#### 2. Conversational Tools

**`speak` Tool** - LLM controls TTS output:
```json
{
  "name": "speak",
  "parameters": {
    "text": "Text to speak aloud",
    "wait_for_response": "true to open mic after speaking",
    "allow_interrupt": "true to allow user interruption"
  }
}
```

**`listen` Tool** - LLM explicitly requests user input:
```json
{
  "name": "listen",
  "parameters": {
    "timeout_ms": "Max wait time (default 5000ms)",
    "prompt_hint": "Optional hint about what to say"
  }
}
```

**Example LLM Usage**:
```xml
<!-- Ask question and wait for answer -->
<tool_call>{"name": "speak", "arguments": {
  "text": "What's your favorite color?",
  "wait_for_response": true
}}</tool_call>

<!-- User's response automatically captured and sent to LLM -->
<!-- LLM continues based on answer... -->
```

#### 3. Interrupt Detection (VAD During TTS)

```c
// Implemented in voice_conversation.c
// Monitors microphone energy while TTS plays
// If energy > threshold: stops TTS, captures speech, sends to LLM

float ethervox_audio_calculate_rms_energy(const float* samples, uint32_t count);
// Shared utility function for consistent VAD across all modules
```

#### 4. System Prompt Enhancement

```
You are EthervoxAI, a conversational AI assistant.

INPUT SOURCE: [Voice Conversation]
CAPABILITIES: speak (TTS), listen (microphone), calculator, memory, files

CONVERSATIONAL GUIDELINES:
1. For voice input, use 'speak' tool to respond audibly
2. Set wait_for_response=true when asking questions
3. User can interrupt you - acknowledge and adapt
4. For CLI input, respond with text (no speak tool)

EXAMPLE:
User (voice): "What's the weather?"
You: <tool_call>{"name": "speak", "arguments": {
  "text": "It's 72 degrees and sunny today.",
  "wait_for_response": false
}}</tool_call>
```

### Implementation Status

| Component | Status | File |
|-----------|--------|------|
| Execution Context | ✅ Complete | `include/ethervox/governor.h` |
| Governor API Extension | ✅ Complete | `ethervox_governor_execute_with_context()` |
| RMS Energy Helper | ✅ Complete | `src/audio/platform_macos.c` |
| `speak` Tool | ⏳ In Progress | `src/plugins/conversation_tools/speak.c` |
| `listen` Tool | ⏳ In Progress | `src/plugins/conversation_tools/listen.c` |
| Interrupt Detection | ⏳ TODO | `voice_conversation.c` |
| LLM Abort Callback | ⏳ TODO | `llama_backend.c` (fix state corruption) |
| System Prompt Update | ⏳ TODO | `governor.c` |

### Key Technical Decisions

#### llama.cpp Interrupt Fix

**Problem**: Current code notes "We don't try to cancel mid-generation as it causes state corruption."

**Solution**: Use proper `llama_set_abort_callback()` API:
```c
// In llama_backend.c
static bool llama_abort_check(void* user_data) {
    llama_backend_context_t* ctx = (llama_backend_context_t*)user_data;
    return ctx->cancel_requested;  // Return true to abort
}

// At context creation:
llama_set_abort_callback(ctx->ctx, llama_abort_check, ctx);

// To cancel:
ctx->cancel_requested = true;
```

**Benefit**: Clean interruption without state corruption, preserves KV cache.

#### Code Reuse & Deduplication

**Found Duplicates**:
- `calculate_energy()` in `wake_word_core.c` (lines 97-108)
- `calculate_energy()` in `whisper_backend.c` (lines 113-125)

**Solution**: Created `ethervox_audio_calculate_rms_energy()` in `audio.h/platform_macos.c` as shared utility.

**TODO**: Refactor wake_word and whisper to use shared function.

#### License Compliance

All new code: **CC BY-NC-SA 4.0** (commercial application, no GPL contamination)

Dependencies:
- llama.cpp: MIT ✅
- whisper.cpp: MIT ✅
- cJSON: MIT ✅
- CoreAudio (macOS): System library ✅

No GPL/AGPL dependencies added.

### Next Implementation Steps

1. **Implement speak/listen tools** (2-3 hours)
   - Create `src/plugins/conversation_tools/`
   - Tool wrappers with JSON parsing
   - Register in `dialogue_core.c`
   - Unit tests

2. **Fix llama interrupt mechanism** (1 hour)
   - Add abort callback in `llama_backend.c`
   - Test cancellation timing
   - Remove state corruption workaround

3. **Update voice_conversation.c** (3-4 hours)
   - Integrate execution context
   - Implement interrupt detection during TTS
   - Handle tool callbacks
   - Test turn-taking

4. **System prompt generation** (1 hour)
   - Add execution context to prompt builder
   - Include tool examples for voice mode
   - Test LLM behavior (CLI vs voice)

5. **End-to-end testing** (2 hours)
   - Voice with interrupts
   - CLI mode (no TTS)
   - Turn-taking with questions
   - Document findings

### Testing Plan

#### Unit Tests

1. **RMS Energy Calculation**
```bash
# Test VAD helper function
./build/tests/test_audio_vad
```

2. **Tool Wrappers**
```bash
# Test speak tool
./build/tests/test_conversation_tools_speak

# Test listen tool  
./build/tests/test_conversation_tools_listen
```

#### Integration Tests

1. **Voice Mode**
```bash
./build/ethervoxai
> /load model.gguf
> /convon
> /wakeon
# Say: "hey ethervox"
# Say: "What's 17 times 23?"
# Expected: TTS speaks "391"
```

2. **Interrupt Test**
```bash
# While assistant is speaking:
# Start speaking to interrupt
# Expected: TTS stops, captures your speech, continues conversation
```

3. **Turn-Taking Test**
```bash
# Say: "Ask me a question"
# Expected: LLM asks question, automatically listens for answer
# Provide answer
# Expected: LLM acknowledges and continues
```

4. **CLI Mode (No TTS)**
```bash
./build/ethervoxai
> What's the capital of France?
# Expected: Text response only, no speak tool called
```

### Known Issues & Improvements Needed

#### 1. llama.cpp State Corruption on Cancel

**Current**: Comment says "We don't try to cancel mid-generation as it causes state corruption"

**Root Cause**: Not using proper abort callback API

**Fix**: Implement `llama_set_abort_callback()` with volatile flag check

**Priority**: HIGH (blocks interrupt feature)

#### 2. Duplicate RMS Energy Functions

**Found In**:
- `wake_word_core.c:97` - `calculate_energy()`
- `whisper_backend.c:113` - `calculate_energy()`

**Solution**: Refactor to use `ethervox_audio_calculate_rms_energy()`

**Priority**: MEDIUM (technical debt)

#### 3. Echo Cancellation

**Problem**: Microphone picks up TTS audio from speakers

**Temporary**: Lower VAD threshold, require higher energy for true speech

**Long-term**: Implement acoustic echo cancellation (AEC) or use directional mic

**Priority**: LOW (acceptable false positives rare in practice)

#### 4. Governor Thread Safety

**Problem**: Conversation thread calls Governor without mutex lock

**Solution**: Add `g_governor_mutex` lock in `voice_conversation.c` before execute

**Priority**: MEDIUM (race condition possible but rare)

### Detailed Flow Diagram

```
┌───────────────────────────────────────────────────────────────────┐
│              Bidirectional Conversational AI Flow                  │
└───────────────────────────────────────────────────────────────────┘

1. INPUT
   ├─ Voice: Wake word → STT → text + source=VOICE
   └─ CLI: User types → text + source=CLI

2. GOVERNOR EXECUTION
   ├─ Create execution_context with source, TTS available, callbacks
   ├─ Call ethervox_governor_execute_with_context()
   ├─ System prompt includes: "INPUT SOURCE: [Voice]"
   └─ LLM generates response with tools

3. LLM TOOL CALLS
   
   For Voice Input:
   ├─ <tool_call>{"name": "speak", "arguments": {"text": "...", "wait_for_response": true}}</tool_call>
   ├─ Callback: on_speak() → TTS synthesis
   ├─ During TTS: Monitor mic for interrupt (VAD)
   ├─ If interrupted: Stop TTS, capture speech, goto step 2
   └─ If wait_for_response: Auto-listen, capture answer, goto step 2
   
   For CLI Input:
   └─ Return text response directly (no speak tool)

4. TURN MANAGEMENT
   ├─ User turn: Microphone active, TTS silent
   ├─ Assistant turn: TTS playing, interrupt detection active
   └─ Waiting turn: Microphone listening (from wait_for_response=true)

5. INTERRUPT HANDLING
   ├─ VAD detects energy > threshold during TTS
   ├─ Stop TTS immediately
   ├─ Start STT capture
   ├─ Send to Governor with context: "User interrupted with: [speech]"
   └─ LLM adapts response based on interruption
```

### Thread Safety Model

**Governor Mutex**: Internal to Governor struct (not global)
```c
struct ethervox_governor {
    pthread_mutex_t execute_mutex;  // Protects execute() calls
    // ...
};

// In governor.c execute functions:
pthread_mutex_lock(&governor->execute_mutex);
// ... LLM inference ...
pthread_mutex_unlock(&governor->execute_mutex);
```

**Benefits**:
- No need to pass mutex to conversation_init()
- Encapsulated within Governor
- Multiple Governors can coexist (future: per-user sessions)

**Conversation Session**: Internal mutex for state
```c
struct ethervox_conversation_session {
    pthread_mutex_t mutex;           // Protects session state
    volatile bool interrupt_flag;    // Atomic interrupt detection
    // ...
};
```

### Files Modified

**Core Architecture**:
1. `include/ethervox/governor.h` - Execution context structs, new API
2. `include/ethervox/audio.h` - RMS energy helper declaration
3. `src/audio/platform_macos.c` - RMS energy implementation
4. `src/governor/governor.c` - execute_with_context implementation

**Tools** (TODO):
5. `src/plugins/conversation_tools/speak.c` - TTS tool
6. `src/plugins/conversation_tools/listen.c` - Microphone tool
7. `src/dialogue/dialogue_core.c` - Register conversation tools

**Integration** (TODO):
8. `src/dialogue/voice_conversation.c` - Use execution context, handle interrupts
9. `src/llm/llama_backend.c` - Fix abort callback
10. `src/main.c` - Pass execution context to Governor

**Documentation**:
11. `docs/CONVERSATION_INTEGRATION.md` - This file (updated architecture)

### Development Checklist

- [x] Design execution context architecture
- [x] Update Governor API headers
- [x] Add RMS energy shared utility
- [x] Document new architecture
- [ ] Implement llama abort callback fix
- [ ] Create conversation_tools plugin directory
- [ ] Implement speak tool + tests
- [ ] Implement listen tool + tests
- [ ] Register tools in dialogue_core.c
- [ ] Update voice_conversation.c with context
- [ ] Implement interrupt detection (VAD during TTS)
- [ ] Update system prompt generation
- [ ] End-to-end testing
- [ ] Refactor duplicated RMS energy code

### References

- Tool implementation guide: `docs/ADDING_NEW_LLM_TOOL.md`
- Governor architecture: `docs/GOVERNOR_ARCHITECTURE.md` (if exists)
- llama.cpp API: `external/llama.cpp/include/llama.h`
- Audio interface: `include/ethervox/audio.h`
