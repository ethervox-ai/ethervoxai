# Voice Tools Design Document

**Status**: Partially Implemented (STT + Speaker Detection Complete)  
**Target**: EthervoxAI Governor Integration  
**License Requirements**: MIT-compatible only (commercial-safe)  
**Last Updated**: December 4, 2025

## Overview

This document outlines the design for integrating voice input/output capabilities into EthervoxAI, enabling the LLM to listen to conversations, transcribe speech, identify speakers, and respond with natural-sounding synthesized speech.

## Core Requirements

### Functional Requirements
1. **Text-to-Speech (TTS)**: Speak LLM responses with natural voice
2. **Speech-to-Text (STT)**: Transcribe user speech to text
3. **Dynamic Listening**: LLM-controlled or user-forced listening duration
4. **Speaker Detection**: Identify and separate different speakers
5. **Transcript Storage**: Full verbatim records in memory system
6. **Summary Generation**: LLM creates concise summaries from transcripts
7. **User Control**: `/listen`, `/stop`, `/tts` commands
8. **LLM Tool Access**: Governor tools for autonomous voice interaction

### Non-Functional Requirements
1. **Privacy**: All processing must be local (no cloud dependencies)
2. **Performance**: Real-time or near-real-time processing on CPU
3. **License**: MIT or Apache 2.0 only (commercial-safe)
4. **Platform Support**: macOS, Linux, Windows, Raspberry Pi
5. **Quality**: Natural-sounding TTS, accurate STT transcription
6. **Memory Efficient**: Run on modest hardware (8GB RAM minimum)

## Technology Stack

### Text-to-Speech: Piper (MIT License)

**Selected Implementation**: Original Piper (rhasspy/piper)
- **Repository**: https://github.com/rhasspy/piper
- **License**: MIT ✅
- **Technology**: VITS neural vocoder
- **Language**: C++ (onnxruntime)
- **Model Size**: 10-50MB per voice
- **Speed**: Real-time on CPU (>1.0x)
- **Quality**: Natural-sounding (8-9/10)
- **Voices**: 50+ languages, multiple speakers per language

**Why Piper**:
- Native C++ implementation (easy integration)
- Fast CPU inference (no GPU required)
- Small model files (embedded-friendly)
- MIT license (fully commercial)
- Proven stability (production-ready)

**Alternative Considered**: Piper1-GPL (rejected due to GPL-3.0 license)

### Speech-to-Text: Whisper.cpp (MIT License) ✅ IMPLEMENTED

**Selected Implementation**: whisper.cpp (ggerganov/whisper.cpp)
- **Repository**: https://github.com/ggerganov/whisper.cpp
- **License**: MIT ✅
- **Technology**: OpenAI Whisper in C/C++
- **Model Sizes**: tiny (39M), base (74M), small (244M), medium (769M), large (1550M)
- **Speed**: Real-time with tiny/base models on CPU
- **Quality**: State-of-the-art accuracy
- **Features**: Multi-language, timestamps, word-level alignment

**Recommended Model**: `base.en` (74MB, English-only, ~4x real-time on M1)

**Implementation Status**:
- ✅ Whisper.cpp integration complete (`src/stt/whisper_backend.c`)
- ✅ Beam search with configurable parameters (reduces repetition loops)
- ✅ Multi-language auto-detection support
- ✅ Translation toggle (`/translate` command)
- ✅ Streaming with 200ms overlap buffer for context
- ✅ Configurable quality thresholds in `config.h`
- ✅ Timestamps for segment alignment (10ms resolution)

**Why Whisper.cpp**:
- Industry standard for local STT
- Excellent accuracy-to-speed ratio
- Multi-language support
- Timestamp information for speaker segmentation
- Active maintenance
- MIT licensed

### Speaker Diarization Strategy ✅ IMPLEMENTED (Phase 1)

**Phase 1**: Acoustic Feature Analysis ✅ COMPLETE
- ✅ **RMS Energy Tracking**: Detects volume changes between speakers (30% threshold)
- ✅ **Zero-Crossing Rate**: Estimates pitch/voice characteristics (15% threshold)
- ✅ **Pause Detection**: Identifies silence between speakers (500ms threshold)
- ✅ **Multi-Factor Confirmation**: Requires 2+ factors to confirm speaker change (reduces false positives)
- ✅ **Configurable Thresholds**: All values adjustable in `config.h`
- **Implementation**: `src/stt/whisper_backend.c` - `detect_speaker_change()`, `calculate_energy()`, `estimate_pitch()`
- **Accuracy**: 60-70% (good enough for basic use, no external dependencies)

**Configuration (`include/ethervox/config.h`)**:
```c
#define ETHERVOX_SPEAKER_ENERGY_CHANGE_THRESHOLD 0.3f  // 30% RMS energy change
#define ETHERVOX_SPEAKER_PITCH_CHANGE_THRESHOLD 0.15f  // 15% pitch change
#define ETHERVOX_SPEAKER_PAUSE_THRESHOLD 50            // 500ms pause (10ms units)
#define ETHERVOX_SPEAKER_CHANGE_MIN_FACTORS 2          // Minimum factors required
#define ETHERVOX_SPEAKER_MAX_SPEAKERS 10               // Maximum speakers to track
#define ETHERVOX_SPEAKER_EXAMPLE_QUOTES 3              // Quotes shown during identification
```

**Phase 2** (Future): Advanced Diarization
- pyannote.audio subprocess (MPL license - user installs separately)
- Voice fingerprinting with MFCC features
- External tool integration (user consent required)
- Accuracy: 90%+

**Implementation Note**: Phase 1 complete and working, Phase 2 is future opt-in plugin

## Implementation Status

### ✅ Completed Features

**Speech-to-Text (Whisper Integration)**:
- ✅ Whisper.cpp backend with beam search (`src/stt/whisper_backend.c`)
- ✅ Multi-language auto-detection and transcription
- ✅ Translation toggle (`/translate` command)
- ✅ Streaming with overlap buffer (200ms context)
- ✅ Configurable quality thresholds (noise filtering, confidence)
- ✅ Real-time transcription with `/transcribe` command

**Speaker Detection**:
- ✅ Acoustic feature analysis (energy, pitch, pause)
- ✅ Multi-factor speaker change detection (2+ factors required)
- ✅ Speaker ID assignment (Speaker 0, 1, 2, ...)
- ✅ Speaker turn tracking across sessions
- ✅ Configurable detection thresholds in `config.h`

**Speaker Identification** (`src/plugins/voice_tools/voice_tools.c`):
- ✅ Post-transcription speaker naming prompt
- ✅ Example quote extraction (shows 3 quotes per speaker)
- ✅ Interactive name assignment ("Speaker 0" → "Alice")
- ✅ Transcript file update with named speakers
- ✅ Anonymous speaker labels if user declines naming

**Configuration System** (`include/ethervox/config.h`):
- ✅ Whisper beam search size (default: 5)
- ✅ Quality thresholds (no_speech, logprob, entropy)
- ✅ Temperature settings for decoding fallback
- ✅ Speaker detection thresholds (energy, pitch, pause)
- ✅ Min factors for speaker change confirmation
- ✅ Max speakers and example quotes count

**User Commands** (`src/main.c`):
- ✅ `/transcribe` - Start microphone transcription
- ✅ `/stoptranscribe` - Stop and save transcript
- ✅ `/translate` - Toggle translation on/off
- ✅ `/testwhisper` - Test Whisper with sample audio

### 🚧 In Progress

**Voice Tools Integration**:
- 🚧 Memory storage for transcripts
- 🚧 LLM-controlled listening duration
- 🚧 Auto-summarization after transcription

### 📋 Planned Features

**Text-to-Speech (Piper)**:
- ⏳ Piper library integration
- ⏳ TTS output for LLM responses
- ⏳ `/tts on/off` command
- ⏳ `voice_speak` Governor tool

**Advanced Features**:
- ⏳ LLM autonomous listening (`voice_listen` tool)
- ⏳ Continuous listening mode (duration=0)
- ⏳ Voice memory search and retrieval
- ⏳ Export transcripts (text, SRT subtitles)

## Architecture Design

### Current Implementation Structure

**STT Runtime** (`include/ethervox/stt.h`):
```c
typedef struct {
    char model_path[512];           // Path to Whisper model
    char language[8];               // Language code ("auto", "en", etc.)
    uint32_t sample_rate;           // Audio sample rate (16000 Hz)
    bool enable_partial_results;    // Stream partial transcripts
    bool enable_punctuation;        // Add punctuation
    float vad_threshold;            // Voice activity detection
    bool translate_to_english;      // Translation toggle
} ethervox_stt_config_t;

typedef struct {
    ethervox_stt_config_t config;
    void* backend_ctx;              // Whisper backend context
    bool is_active;
} ethervox_stt_runtime_t;
```

**Whisper Backend Context** (`src/stt/whisper_backend.c`):
```c
typedef struct {
    struct whisper_context* ctx;           // Whisper.cpp context
    struct whisper_full_params params;     // Beam search parameters
    
    // Streaming buffers
    float* audio_buffer;                   // Main accumulation buffer
    size_t audio_buffer_size;
    float* overlap_buffer;                 // 200ms overlap for context
    size_t overlap_size;
    
    // Language detection
    bool auto_detect_language;
    char detected_language[3];
    bool language_detected;
    
    // Speaker tracking
    int current_speaker;                   // Current active speaker ID
    bool show_speaker_labels;
    
    // Acoustic feature tracking for speaker detection
    float last_segment_energy;             // RMS energy of last segment
    float last_segment_pitch;              // Estimated pitch (zero-crossing rate)
    int64_t last_segment_end_time;         // End time for pause detection
    bool first_segment;
    
    // Duplicate detection
    char* last_transcript;
    int duplicate_count;
} whisper_backend_context_t;
```

**Voice Session** (`include/ethervox/voice_tools.h`):
```c
typedef struct {
    bool is_recording;
    bool is_initialized;
    
    // STT runtime
    ethervox_stt_runtime_t stt_runtime;
    ethervox_audio_runtime_t audio_runtime;
    
    // Accumulated transcript
    char* full_transcript;
    size_t transcript_len;
    
    // Session metadata
    uint64_t session_start_time;
    uint32_t segment_count;
    char last_transcript_file[1024];
    
    // Speaker tracking
    int max_speaker_id;                    // Highest speaker ID in session
    char** speaker_names;                  // Array of assigned names
    int speaker_names_capacity;
} ethervox_voice_session_t;

typedef struct {
    // TTS Engine (Piper)
    void* piper_ctx;          // Piper synthesis context
    char tts_model_path[512]; // Path to voice model (.onnx)
    
    // STT Engine (Whisper.cpp)
    void* whisper_ctx;        // Whisper context
    char stt_model_path[512]; // Path to Whisper model
    
    // Audio I/O
    void* audio_input;        // Platform-specific input stream
    void* audio_output;       // Platform-specific output stream
    
    // State
    bool is_listening;        // Currently recording audio
    bool tts_enabled;         // Auto-speak LLM responses
    uint32_t listen_duration_ms; // Target listening duration
    time_t listen_start_time; // When listening started
    
    // Buffers
    float* audio_buffer;      // Raw audio samples
    size_t buffer_pos;        // Current buffer position
    size_t buffer_capacity;   // Total buffer size
    
    // Speaker tracking
    uint32_t num_speakers;    // Detected speakers count
    float speaker_profiles[ETHERVOX_VOICE_MAX_SPEAKERS][128]; // Voice prints
    
    // Configuration
    bool detect_speakers;     // Enable speaker diarization
    float vad_threshold;      // Voice activity detection threshold
    uint32_t sample_rate;     // Audio sample rate (16000 Hz)
    
} ethervox_voice_engine_t;
```

### Core API Functions

```c
/**
 * Initialize voice engine
 * 
 * @param engine Voice engine to initialize
 * @param tts_model_path Path to Piper voice model (.onnx)
 * @param stt_model_path Path to Whisper model (base.en.bin)
 * @param config Optional configuration (NULL = defaults)
 * @return 0 on success, negative on error
 */
int ethervox_voice_init(
    ethervox_voice_engine_t* engine,
    const char* tts_model_path,
    const char* stt_model_path,
    const ethervox_voice_config_t* config
);

/**
 * Text-to-Speech: Synthesize and play audio
 * 
 * @param engine Voice engine
 * @param text Text to speak
 * @param interruptible Can be stopped by user input
 * @return 0 on success, negative on error
 */
int ethervox_voice_speak(
    ethervox_voice_engine_t* engine,
    const char* text,
    bool interruptible
);

/**
 * Start listening to audio input
 * 
 * @param engine Voice engine
 * @param duration_ms Listen duration (0 = continuous until stopped)
 * @param detect_speakers Enable speaker diarization
 * @return 0 on success, negative on error
 */
int ethervox_voice_listen_start(
    ethervox_voice_engine_t* engine,
    uint32_t duration_ms,
    bool detect_speakers
);

/**
 * Stop listening and retrieve transcript
 * 
 * @param engine Voice engine
 * @param transcript_out Full combined transcript (caller must free)
 * @param segments_out Array of timestamped segments (caller must free)
 * @param num_segments Number of segments returned
 * @return 0 on success, negative on error
 */
int ethervox_voice_listen_stop(
    ethervox_voice_engine_t* engine,
    char** transcript_out,
    ethervox_voice_segment_t** segments_out,
    uint32_t* num_segments
);

/**
 * Check if currently listening
 */
bool ethervox_voice_is_listening(const ethervox_voice_engine_t* engine);

/**
 * Enable/disable auto-TTS for LLM responses
 */
void ethervox_voice_set_tts_enabled(ethervox_voice_engine_t* engine, bool enabled);

/**
 * Cleanup and free resources
 */
void ethervox_voice_cleanup(ethervox_voice_engine_t* engine);
```

## Governor Tool Integration

### Tool: voice_speak

```json
{
  "name": "voice_speak",
  "description": "Speak text aloud using text-to-speech. Use for important announcements, responses to voice queries, or to get user attention.",
  "parameters": {
    "text": {
      "type": "string",
      "description": "Text to speak aloud"
    },
    "interruptible": {
      "type": "boolean",
      "description": "Whether speech can be interrupted by user (default: true)",
      "default": true
    }
  },
  "required": ["text"]
}
```

**Example Usage**:
```xml
<tool_call name="voice_speak" text="Your build completed successfully!" />
```

### Tool: voice_listen

```json
{
  "name": "voice_listen",
  "description": "Listen to audio input and transcribe speech. Can detect multiple speakers. Returns timestamped transcript segments. Use when you need to capture spoken information, conduct voice interviews, or record meetings.",
  "parameters": {
    "duration_seconds": {
      "type": "integer",
      "description": "How long to listen in seconds (0 = continuous until user stops, 1-300 = fixed duration)",
      "minimum": 0,
      "maximum": 300,
      "default": 30
    },
    "detect_speakers": {
      "type": "boolean",
      "description": "Attempt to identify and separate different speakers",
      "default": true
    },
    "auto_summarize": {
      "type": "boolean",
      "description": "Automatically generate and store summary after transcription",
      "default": true
    }
  }
}
```

**Example Usage**:
```xml
<tool_call name="voice_listen" duration_seconds="60" detect_speakers="true" />
```

**Response Format**:
```json
{
  "success": true,
  "transcript": "Hello there. Hi, how are you? I'm doing well, thanks for asking.",
  "duration_actual": 8.5,
  "num_speakers": 2,
  "segments": [
    {
      "speaker": 0,
      "text": "Hello there.",
      "start": 0.5,
      "end": 1.8,
      "confidence": 0.95
    },
    {
      "speaker": 1,
      "text": "Hi, how are you?",
      "start": 2.1,
      "end": 3.5,
      "confidence": 0.92
    },
    {
      "speaker": 0,
      "text": "I'm doing well, thanks for asking.",
      "start": 3.8,
      "end": 5.9,
      "confidence": 0.94
    }
  ]
}
```

## User Commands (Current Implementation)

### /transcribe ✅ IMPLEMENTED
Start microphone capture and real-time transcription with speaker detection.

```bash
> /transcribe
Starting microphone capture... (use /stoptranscribe to end)
[Real-time transcription appears as you speak]
[Speaker 0] Hello, this is a test of the transcription system.
[Speaker 1] Yes, I can hear you clearly.
[Speaker 0] Great, the speaker detection seems to be working.
```

### /stoptranscribe ✅ IMPLEMENTED
Stop current transcription session and prompt for speaker identification.

```bash
> /stoptranscribe
Stopped transcription.

========================================
Speaker Identification
========================================
Detected 2 speaker(s) in the conversation.

Speaker 0 examples:
  1. "Hello, this is a test of the transcription system."
  2. "Great, the speaker detection seems to be working."

Speaker 1 examples:
  1. "Yes, I can hear you clearly."

Would you like to assign names to the speakers? [y/N]: y

Enter name for Speaker 0: Alice
Enter name for Speaker 1: Bob

Speaker assignments:
  Speaker 0 → Alice
  Speaker 1 → Bob

✓ Transcript updated with speaker names
  File: ~/.ethervox/memory/voice_transcript_1733356800.txt
```

### /translate ✅ IMPLEMENTED
Toggle translation on/off (multilingual models only).

```bash
> /translate
Translation: OFF (transcribing in original language)

> /translate
Translation: ON (will translate non-English to English)
```

### /testwhisper ✅ IMPLEMENTED
Test Whisper STT with sample audio file.

```bash
> /testwhisper
Testing Whisper STT backend...
[Processes test audio file]
Transcript: [Test audio transcription]
```

### /tts [on|off] ⏳ PLANNED
Enable or disable automatic text-to-speech for LLM responses.

```bash
> /tts on
TTS enabled - all responses will be spoken aloud

> /tts off
TTS disabled
```

## Memory Integration

### Transcript Storage

Voice transcripts are stored in the memory system with special formatting and tags:

```c
// Full transcript with timestamps and speaker labels
uint64_t transcript_id;
char transcript_text[ETHERVOX_MEMORY_MAX_TEXT_LEN];

snprintf(transcript_text, sizeof(transcript_text),
    "VOICE TRANSCRIPT [00:00-00:28] - %u speakers\n\n"
    "[Speaker 0, 00:00-00:02] Hello there\n"
    "[Speaker 1, 00:02-00:04] Hi how are you\n"
    "[Speaker 0, 00:05-00:15] I wanted to discuss the project timeline\n"
    "[Speaker 1, 00:16-00:25] Sure, what's your availability next week?\n"
    "[Speaker 0, 00:26-00:28] I'm free Tuesday afternoon",
    num_segments
);

const char* tags[] = {"voice", "transcript", "multi-speaker", "conversation"};
ethervox_memory_store_add(&memory_store, transcript_text, tags, 4, 
                         0.7, // Medium importance (raw data)
                         true, &transcript_id);
```

### Summary Storage

LLM generates and stores concise summary:

```c
// LLM uses memory_store tool after listening:
{
  "text": "Meeting summary: Two speakers discussed project timeline. 
           Speaker 0 requested availability for next week. 
           Speaker 1 asked about schedule. 
           Speaker 0 confirmed Tuesday afternoon availability.",
  "tags": ["voice", "summary", "meeting", "schedule"],
  "importance": 0.95, // High importance (processed insight)
  "is_user": false
}
```

### Cross-Referencing

Link summary to transcript:

```c
// Store relationship metadata
char metadata[256];
snprintf(metadata, sizeof(metadata),
    "Summary of transcript ID %llu", transcript_id);

// Append to summary text or use custom metadata field
```

## Implementation Phases

### Phase 1: STT Input ✅ COMPLETE
**Goal**: Capture and transcribe audio with speaker detection

**Completed Tasks**:
- ✅ Integrated whisper.cpp library (submodule at `external/whisper.cpp`)
- ✅ Implemented Whisper backend (`src/stt/whisper_backend.c`)
- ✅ Implemented audio capture (`src/audio/platform_macos.c` and platform-specific)
- ✅ Beam search configuration (size=5, no_context=true to prevent loops)
- ✅ Multi-language auto-detection (two-pass: detect then transcribe)
- ✅ Translation toggle (`/translate` command)
- ✅ Streaming with 200ms overlap buffer
- ✅ Quality thresholds (no_speech=0.6, entropy, logprob)
- ✅ Added `/transcribe` and `/stoptranscribe` commands
- ✅ Acoustic speaker detection (energy, pitch, pause analysis)
- ✅ Multi-factor speaker change confirmation (2+ factors)
- ✅ Speaker ID assignment and tracking
- ✅ Configuration system in `config.h`

**Deliverables**:
- ✅ `src/stt/whisper_backend.c` - Whisper integration with speaker detection
- ✅ `src/stt/stt_core.c` - STT runtime management
- ✅ `include/ethervox/stt.h` - Public STT API
- ✅ `include/ethervox/config.h` - Configurable thresholds
- ✅ `scripts/download-whisper-model.sh` - Model download script
- ✅ Multi-speaker transcript formatting

**Testing**:
```bash
> /transcribe
Starting microphone capture...
[User speaks: "Hello, this is a test"]
[Speaker 0] Hello, this is a test
> /stoptranscribe
Stopped transcription.

========================================
Speaker Identification
========================================
Detected 1 speaker(s) in the conversation.

Speaker 0 examples:
  1. "Hello, this is a test"
```

### Phase 2: Speaker Identification ✅ COMPLETE
**Goal**: Identify and name speakers with example quotes

**Completed Tasks**:
- ✅ Extract example quotes from transcript (up to 3 per speaker)
- ✅ Display examples before prompting for names
- ✅ Interactive speaker name assignment
- ✅ Update transcript file with named speakers ("[Speaker 0]" → "[Alice]")
- ✅ Configurable quote count (`ETHERVOX_SPEAKER_EXAMPLE_QUOTES`)
- ✅ Graceful handling of empty transcripts
- ✅ Anonymous speaker labels if user declines naming

**Deliverables**:
- ✅ `src/plugins/voice_tools/voice_tools.c` - Enhanced `assign_speaker_names()`
- ✅ Example quote extraction logic
- ✅ Transcript file update with name substitution
- ✅ User-friendly prompts and formatting

**Testing**:
```bash
========================================
Speaker Identification
========================================
Detected 2 speaker(s) in the conversation.

Speaker 0 examples:
  1. "Hello, how are you today?"
  2. "I think we should discuss the project."
  3. "That's a great point."

Speaker 1 examples:
  1. "I'm doing well, thanks for asking."
  2. "Yes, let's review it together."

Would you like to assign names to the speakers? [y/N]: y

Enter name for Speaker 0: Alice
Enter name for Speaker 1: Bob

Speaker assignments:
  Speaker 0 → Alice
  Speaker 1 → Bob

✓ Transcript updated with speaker names
```

### Phase 3: LLM-Controlled Listening 🚧 IN PROGRESS
**Goal**: LLM decides when and how long to listen

**Tasks**:
1. Implement continuous listening mode (duration=0)
2. Add background thread for audio capture
3. Implement user interrupt handling
4. Update governor tool parameters
5. Test LLM autonomous listening

**Deliverables**:
- Thread-safe audio capture
- Interrupt mechanism
- Updated tool schemas

**Testing**:
```
User: "I need to tell you something"
LLM: <tool_call name="voice_listen" duration_seconds="30" />
[Listens and transcribes]
LLM: "I understand you said..."
```

### Phase 4: TTS Output ⏳ PLANNED
**Goal**: LLM can speak responses

**Planned Tasks**:
1. Integrate Piper library (compile as dependency)
2. Implement `ethervox_voice_init()` with TTS-only mode
3. Implement `ethervox_voice_speak()`
4. Add `/tts on/off` command to main.c
5. Register `voice_speak` tool with governor
6. Auto-speak LLM responses when TTS enabled

**Planned Deliverables**:
- `src/voice/voice_tts.c` - TTS implementation
- `include/ethervox/voice.h` - Public API
- Piper model download script
- Documentation updates

**Expected Testing**:
```bash
> /tts on
> Hello, can you speak this?
[LLM responds with text AND speaks it]
```

### Phase 5: Memory Integration & Summaries 🚧 IN PROGRESS
**Goal**: Seamless voice data storage and LLM summarization

**Partial Completion**:
- ✅ Transcript file storage (JSONL format with timestamps)
- ✅ Speaker labels in stored transcripts
- 🚧 LLM automatic summarization
- ⏳ Cross-reference summaries with transcripts
- ⏳ Search/retrieve voice memories
- ⏳ Export voice transcripts

**Remaining Tasks**:
1. Automatic transcript storage in memory system
2. LLM generates summaries using `memory_store` tool
3. Cross-reference summaries with transcripts
4. Search/retrieve voice memories by speaker
5. Export formats (text, SRT subtitles)

**Planned Deliverables**:
- Voice memory templates
- Summary generation prompts
- Search by speaker functionality
- Export formats (text, SRT subtitles)

**Expected Testing**:
```bash
> /transcribe
[Long conversation with multiple speakers]
> /stoptranscribe
[Transcript stored and speaker names assigned]
> Can you summarize what we discussed?
[LLM searches voice memories, generates summary]
```

## Current Configuration Reference

All configuration values are in `include/ethervox/config.h` and can be overridden at compile time.

### Whisper STT Configuration

```c
// Beam search settings
#define ETHERVOX_WHISPER_BEAM_SIZE 5  // Number of beams (higher = more accurate but slower)

// Quality thresholds
#define ETHERVOX_WHISPER_NO_SPEECH_THRESHOLD 0.6f  // Noise/silence filtering (0.0-1.0)
#define ETHERVOX_WHISPER_LOGPROB_THRESHOLD -1.0f   // Confidence threshold
#define ETHERVOX_WHISPER_ENTROPY_THRESHOLD 2.4f    // Uncertainty filtering

// Temperature settings for decoding fallback
#define ETHERVOX_WHISPER_TEMPERATURE_START 0.0f        // Start greedy (deterministic)
#define ETHERVOX_WHISPER_TEMPERATURE_INCREMENT 0.2f    // Increase if decoding fails

// Streaming settings
#define ETHERVOX_WHISPER_CHUNK_SIZE 480000      // 30 seconds at 16kHz
#define ETHERVOX_WHISPER_OVERLAP_SIZE 3200      // 200ms at 16kHz
```

### Speaker Detection Configuration

```c
// Acoustic feature thresholds for speaker change detection
#define ETHERVOX_SPEAKER_ENERGY_CHANGE_THRESHOLD 0.3f   // 30% RMS energy change
#define ETHERVOX_SPEAKER_PITCH_CHANGE_THRESHOLD 0.15f   // 15% pitch change (ZCR)
#define ETHERVOX_SPEAKER_PAUSE_THRESHOLD 50             // 500ms pause (10ms units)
#define ETHERVOX_SPEAKER_CHANGE_MIN_FACTORS 2           // Min factors to confirm change

// Speaker tracking limits
#define ETHERVOX_SPEAKER_MAX_SPEAKERS 10                // Maximum speakers per session
#define ETHERVOX_SPEAKER_EXAMPLE_QUOTES 3               // Quotes shown during naming
```

### Tuning Guidelines

**For more sensitive speaker detection** (detects changes more frequently):
- Decrease `ETHERVOX_SPEAKER_ENERGY_CHANGE_THRESHOLD` to 0.2 (20%)
- Decrease `ETHERVOX_SPEAKER_PITCH_CHANGE_THRESHOLD` to 0.1 (10%)
- Decrease `ETHERVOX_SPEAKER_PAUSE_THRESHOLD` to 30 (300ms)
- Decrease `ETHERVOX_SPEAKER_CHANGE_MIN_FACTORS` to 1 (single factor)

**For more conservative speaker detection** (reduces false positives):
- Increase `ETHERVOX_SPEAKER_ENERGY_CHANGE_THRESHOLD` to 0.4 (40%)
- Increase `ETHERVOX_SPEAKER_PITCH_CHANGE_THRESHOLD` to 0.2 (20%)
- Increase `ETHERVOX_SPEAKER_PAUSE_THRESHOLD` to 70 (700ms)
- Keep `ETHERVOX_SPEAKER_CHANGE_MIN_FACTORS` at 2 or increase to 3

**For better transcription quality** (reduces noise and repetition):
- Increase `ETHERVOX_WHISPER_NO_SPEECH_THRESHOLD` to 0.7 or 0.8
- Increase `ETHERVOX_WHISPER_BEAM_SIZE` to 8 or 10 (slower but more accurate)
- Keep `ETHERVOX_WHISPER_TEMPERATURE_START` at 0.0 for deterministic output

**For faster processing** (may reduce accuracy):
- Decrease `ETHERVOX_WHISPER_BEAM_SIZE` to 3
- Use smaller Whisper model (tiny or base instead of small/medium)

## Platform-Specific Considerations

### macOS
- **Audio Input**: Core Audio (AudioQueue API)
- **Audio Output**: AVAudioEngine or play command
- **TTS Fallback**: `say` command (if Piper unavailable)
- **Permissions**: Microphone access (Info.plist entry)

### Linux
- **Audio Input**: ALSA or PulseAudio
- **Audio Output**: ALSA or PulseAudio
- **Dependencies**: `libasound2-dev`, `libpulse-dev`
- **Permissions**: User must be in `audio` group

### Windows
- **Audio Input**: WASAPI or WinMM
- **Audio Output**: WASAPI
- **TTS Fallback**: SAPI (if Piper unavailable)
- **Permissions**: Microphone access prompt

### Raspberry Pi
- **Audio Input**: ALSA (USB mic recommended)
- **Audio Output**: ALSA (3.5mm jack or HDMI)
- **Model Choice**: Whisper tiny (39MB) for CPU constraints
- **Performance**: ~2-3x real-time on RPi 4

## Resource Requirements

### Storage
- Piper voice model: 10-50 MB (per voice)
- Whisper base.en model: 74 MB
- Whisper tiny model: 39 MB (for RPi)
- Total minimum: ~100-150 MB

### Memory (RAM)
- Piper inference: ~50-100 MB
- Whisper inference: ~200-500 MB (base model)
- Audio buffers: ~10-50 MB
- Total minimum: ~500 MB dedicated

### CPU
- Piper TTS: 1-2 CPU seconds per sentence (real-time)
- Whisper STT: 3-5x real-time (base model, modern CPU)
- Speaker detection: Negligible overhead
- Total: 1-2 CPU cores recommended

### Recommended Hardware
- **Minimum**: Raspberry Pi 4 (4GB), Intel i5 (2015+), Apple M1
- **Optimal**: Any modern desktop/laptop (2020+)
- **Not Recommended**: Raspberry Pi 3, older ARM devices

## Security & Privacy

### Data Handling
- All audio processing happens **locally** (no network calls)
- No audio uploaded to cloud services
- Transcripts stored in local memory JSONL files
- User controls retention via `/clear` command

### Permissions
- Explicit microphone permission required
- User notified when listening starts
- Visual/audio indicator during recording
- Can disable voice features entirely

### Data Retention
- Transcripts: Stored in memory until explicitly cleared
- Audio buffers: Cleared immediately after transcription
- No raw audio files saved (unless user explicitly exports)
- Summaries: Retained per memory importance settings

## Testing Strategy

### Unit Tests
- TTS synthesis with various text inputs
- STT transcription accuracy benchmarks
- Speaker detection with synthetic test audio
- Memory storage and retrieval
- Command parsing

### Integration Tests
- Full voice conversation workflow
- LLM autonomous listening decisions
- Multi-speaker conversation handling
- Interrupt and resume scenarios
- Memory search across voice data

### Performance Tests
- Real-time factor measurement (RTF)
- Memory usage profiling
- CPU utilization monitoring
- Latency measurements (speak/listen)

### User Acceptance Tests
- Natural conversation flows
- Multi-speaker meeting scenarios
- Voice command accuracy
- TTS naturalness ratings
- Overall UX satisfaction

## Future Enhancements

### Post-MVP Features
1. **Voice Cloning**: Custom voice models (Piper supports training)
2. **Real-time Translation**: Whisper multi-language → English
3. **Emotion Detection**: Tone analysis in speech
4. **Wake Word**: "Hey Ethervox" activation
5. **Voice Commands**: "/remind me" spoken aloud
6. **Noise Cancellation**: Background noise filtering
7. **Echo Cancellation**: For speaker + mic scenarios
8. **Advanced Diarization**: pyannote.audio integration
9. **Voice Biometrics**: Speaker identification by voice print
10. **Streaming STT**: Real-time transcription display

### Research Areas
- On-device voice activity detection (WebRTC VAD)
- Low-latency streaming pipeline
- Custom Whisper fine-tuning for domain jargon
- Compressed voice models for embedded devices

## Dependencies & Licensing

### Direct Dependencies
| Library | License | Purpose | Commercial OK |
|---------|---------|---------|---------------|
| Piper | MIT | Text-to-speech | ✅ Yes |
| whisper.cpp | MIT | Speech-to-text | ✅ Yes |
| ggml | MIT | ML inference (Whisper) | ✅ Yes |
| onnxruntime | MIT | ML inference (Piper) | ✅ Yes |

### Transitive Dependencies
| Library | License | Purpose | Commercial OK |
|---------|---------|---------|---------------|
| ggml-metal | MIT | Metal acceleration | ✅ Yes |
| CoreAudio | Apple | macOS audio I/O | ✅ Yes |
| ALSA | LGPL | Linux audio (dynamic link) | ✅ Yes (if dynamic) |

### Optional Dependencies (User Installs)
| Library | License | Purpose | Commercial OK |
|---------|---------|---------|---------------|
| pyannote.audio | MPL 2.0 | Advanced speaker ID | ⚠️ Weak copyleft |

**License Summary**: Entire voice stack is MIT-licensed and commercially safe.

## References

### Documentation
- Piper GitHub: https://github.com/rhasspy/piper
- Piper Voice Samples: https://rhasspy.github.io/piper-samples/
- whisper.cpp: https://github.com/ggerganov/whisper.cpp
- Whisper Models: https://huggingface.co/ggerganov/whisper.cpp
- Speaker Diarization: https://github.com/pyannote/pyannote-audio

### Academic Papers
- VITS: Conditional Variational Autoencoder with Adversarial Learning for End-to-End Text-to-Speech (Kim et al., 2021)
- Robust Speech Recognition via Large-Scale Weak Supervision (Radford et al., 2022) - Whisper
- Neural Speaker Diarization with Speaker-Wise Chain Rule (Fujita et al., 2021)

### Model Downloads
- Piper voices: https://huggingface.co/rhasspy/piper-voices
- Whisper models: https://huggingface.co/ggerganov/whisper.cpp/tree/main

---

## Appendix A: Example Workflows

### Workflow 1: User Dictates Notes
```
User: /listen 60
System: Listening for 60 seconds...
User: [Speaks for 45 seconds about project ideas]
System: Stopped listening. Transcribing...

Transcript: [Full verbatim text]

Would you like me to summarize this?

User: Yes
LLM: <tool_call name="memory_store" text="Summary: Project brainstorming session. 
     Key ideas: 1) Focus on user privacy, 2) Local-first architecture, 
     3) Voice integration as differentiator" tags="notes,project,brainstorm" 
     importance="0.9" />

Summary stored. I've captured three main ideas from your dictation.
```

### Workflow 2: LLM Conducts Interview
```
User: Can you interview me about my project requirements?

LLM: I'd like to ask you some questions. May I listen to your responses?
<tool_call name="voice_listen" duration_seconds="0" detect_speakers="false" />

[Listening...]

LLM: What is the main problem you're trying to solve?
User: [Speaks answer]

LLM: <tool_call name="voice_listen" duration_seconds="0" />
[Continues asking and listening through multiple rounds]

LLM: Thank you. Let me summarize what I learned...
<tool_call name="memory_store" text="Requirements summary: ..." />
```

### Workflow 3: Meeting Transcription
```
User: /listen 1800
System: Listening for 30 minutes... (use /stop to end early)

[Two people have meeting]

User: /stop
System: Transcribing 28 minutes of audio...

Transcript:
[Speaker 0, 00:15] Good morning, let's discuss the roadmap
[Speaker 1, 00:18] Sounds good, I have the slides ready
...

LLM: I've transcribed your meeting. Would you like me to:
1. Generate action items
2. Create a summary
3. Identify key decisions

User: All three

LLM: <tool_call name="memory_store" text="Action items: 1) User to send slides by Friday..." />
<tool_call name="memory_store" text="Meeting summary: Roadmap discussion..." />
<tool_call name="memory_store" text="Decisions: Approved Q1 timeline..." />
```

---

**Document Version**: 1.0  
**Last Updated**: November 26, 2025  
**Author**: EthervoxAI Development Team  
**Status**: Design Complete - Ready for Implementation
