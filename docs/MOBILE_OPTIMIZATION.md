# Mobile Optimization Features

**Status**: Production Ready  
**Version**: 1.0  
**Last Updated**: 2025-01-31

## Overview

This document describes mobile-specific optimizations for EthervoxAI, focusing on fast loading and privacy features essential for real-world deployment on resource-constrained devices.

## Features

### 1. Minimal System Prompt Mode

**Problem**: Full system prompt with all tools (~1200 tokens) takes significant time to load on mobile devices, especially when processing in 1024-token chunks. Additionally, loading system prompt + user prompt simultaneously can confuse the model.

**Solution**: Minimal prompt mode reduces system prompt to ~50 tokens (96% reduction) for ultra-fast startup.

#### Configuration

```c
ethervox_governor_config_t config = ethervox_governor_default_config();

// Enable minimal mode for fast mobile loading
config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_MINIMAL;

// Or keep full capabilities (default)
config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_FULL;
```

#### System Prompt Modes

| Mode | Tokens | Load Time | Tools Available | Use Case |
|------|--------|-----------|-----------------|----------|
| `FULL` | ~1200 | 2-5 seconds | ✅ All tools | Desktop, high-end mobile |
| `MINIMAL` | ~50 | <0.5 seconds | ❌ None | Budget mobile, quick interactions |

#### Minimal Mode Behavior

When `ETHERVOX_GOVERNOR_MODE_MINIMAL` is enabled:

1. **Brief System Prompt**: LLM receives concise instructions (~50 tokens):
   ```
   You are EthervoxAI, a helpful and concise voice assistant.
   Your tools are currently unavailable, so provide direct answers 
   based on your knowledge. Be brief, accurate, and conversational.
   ```

2. **Tools Disabled**: `governor->tools_available = false`
   - Tool registry is NOT built (skips expensive prompt construction)
   - LLM is explicitly informed tools are unavailable
   - Responses are direct knowledge-based answers

3. **Fast Loading**: Minimal tokens = minimal processing time
   - ~50 tokens vs ~1200 tokens = 96% reduction
   - Typical load time: 200-500ms vs 2-5 seconds

#### When to Use

**Use Minimal Mode**:
- Budget mobile devices (Android Go, low-end hardware)
- Quick voice queries without tool needs
- Low battery scenarios
- Testing/debugging model responses
- **macOS/Linux/Windows**: Use `--minimal` flag for fast startup

**Use Full Mode**:
- Desktop/laptop deployments
- High-end mobile devices
- When tools are essential (file ops, memory, voice, timers)
- Power-user scenarios

### CLI Usage (macOS/Linux/Windows)

**Fast Loading Mode**:
```bash
# Start with minimal mode
./ethervoxai --minimal

# With specific model
./ethervoxai --minimal --model path/to/model.gguf

# Combine with other flags
./ethervoxai --minimal --debug --no-persist
```

**Performance Comparison**:
```
Normal:  ./ethervoxai --model model.gguf
         -> 4.2 seconds to load, all tools available

Minimal: ./ethervoxai --minimal --model model.gguf
         -> 0.4 seconds to load, tools disabled (90% faster!)
```

### 2. Secret Mode (Privacy)

**Problem**: All conversations are logged to memory by default via `memory_store` tool. No way to disable logging for sensitive/private conversations.

**Solution**: Secret mode disables memory logging while keeping the LLM functional.

#### Configuration

```c
ethervox_governor_config_t config = ethervox_governor_default_config();

// Enable secret mode (no memory logging)
config.disable_memory_logging = true;

// Or normal mode (default)
config.disable_memory_logging = false;
```

#### Secret Mode Behavior

When `disable_memory_logging = true`:

1. **Memory Store Bypass**: 
   - `memory_store` tool returns success immediately
   - No actual data written to disk
   - Response: `{"success":true,"memory_id":0,"note":"Secret mode - memory not saved"}`

2. **Tool Transparency**:
   - LLM still believes memory was stored (no disruption to flow)
   - No error messages or unusual behavior
   - Conversation continues normally

3. **Privacy Guarantee**:
   - No `.jsonl` files created
   - No conversation history persisted
   - Memory searches return empty (no data stored)

#### Privacy Implications

**What is NOT logged** (secret mode enabled):
- User queries
- Assistant responses
- Tool calls and results
- Conversation metadata

**What STILL works**:
- LLM reasoning and responses
- Tool execution (except memory storage)
- Voice transcription
- File operations (if enabled)

**Security Note**: Secret mode only affects memory logging. Other tools (e.g., file_write) may still persist data if called by the LLM.

### 3. API Functions

#### Set Privacy Mode Programmatically

```c
#include "ethervox/memory_tools.h"

// Enable secret mode
ethervox_memory_set_privacy_mode(true);

// Disable secret mode (return to normal logging)
ethervox_memory_set_privacy_mode(false);
```

**When to call**:
- After model loading, before first query
- When user toggles "private mode" in UI
- Based on conversation sensitivity detection

**Example Android Integration**:

```kotlin
// User toggles privacy switch in UI
privacySwitch.setOnCheckedChangeListener { _, isChecked ->
    NativeLib.setPrivacyMode(isChecked)
    if (isChecked) {
        showToast("Secret mode enabled - nothing will be remembered")
    } else {
        showToast("Normal mode - conversations will be saved")
    }
}
```

## Android JNI Integration

### Minimal Mode Example

```c
// In ethervox_android_core.c
JNIEXPORT jint JNICALL
Java_com_droid_ethervox_1core_NativeLib_loadGovernorModelMinimal(
    JNIEnv* env,
    jobject thiz,
    jstring model_path_jstr
) {
    const char* model_path = (*env)->GetStringUTFChars(env, model_path_jstr, NULL);
    
    // Configure for minimal mode (fast mobile loading)
    ethervox_governor_config_t config = ethervox_governor_default_config();
    config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_MINIMAL;
    config.disable_memory_logging = false;  // Normal privacy
    
    int result = ethervox_governor_load_model(g_governor, model_path);
    
    (*env)->ReleaseStringUTFChars(env, model_path_jstr, model_path);
    return result;
}
```

### Secret Mode Example

```kotlin
// Kotlin wrapper
class EthervoxCore {
    external fun setPrivacyMode(enabled: Boolean)
    
    fun enableSecretMode() {
        setPrivacyMode(true)
        Log.i(TAG, "Secret mode enabled - conversations will not be saved")
    }
    
    fun disableSecretMode() {
        setPrivacyMode(false)
        Log.i(TAG, "Normal mode - conversations will be saved to memory")
    }
}
```

## Performance Benchmarks

### Minimal vs Full Mode Loading

**Test Device**: Mid-range Android (Snapdragon 778G, 6GB RAM)  
**Model**: Qwen2.5-3B-Instruct Q4_K_M (2.1GB)

| Mode | System Prompt Tokens | Load Time | First Response |
|------|---------------------|-----------|----------------|
| **Full** | 1,187 tokens | 4.2 seconds | 6.8 seconds |
| **Minimal** | 49 tokens | 0.4 seconds | 2.1 seconds |
| **Improvement** | **96% reduction** | **90% faster** | **69% faster** |

### Memory Impact

**Secret Mode Storage**:

| Mode | Session Duration | Memory Files Created | Disk Usage |
|------|------------------|---------------------|------------|
| **Normal** | 30 minutes | 1 session file | ~45KB |
| **Secret** | 30 minutes | 0 files | 0KB |

## Use Cases

### Minimal Mode Use Cases

1. **Budget Device Onboarding**: Fast first impression on low-end hardware
2. **Quick Queries**: "What's the weather?" (no tools needed)
3. **Battery Saver**: Reduced processing = less battery drain
4. **Development Testing**: Fast iteration when testing model responses

### Secret Mode Use Cases

1. **Sensitive Conversations**: Medical, financial, personal topics
2. **Temporary Queries**: One-off questions user doesn't want logged
3. **Public Demos**: Show functionality without storing data
4. **Testing**: Avoid polluting memory database during development

## Implementation Details

### System Prompt Mode Enum

```c
typedef enum {
    ETHERVOX_GOVERNOR_MODE_FULL,     // Full prompt with all tools
    ETHERVOX_GOVERNOR_MODE_MINIMAL   // Brief prompt without tools
} ethervox_governor_system_prompt_mode_t;
```

### Governor Config Struct

```c
typedef struct {
    // ... existing fields ...
    
    // Mobile optimization and privacy features
    ethervox_governor_system_prompt_mode_t system_prompt_mode;
    bool disable_memory_logging;
} ethervox_governor_config_t;
```

### Default Values

```c
ethervox_governor_config_t default_config = {
    .confidence_threshold = 0.85f,
    .max_iterations = 5,
    .max_tool_calls_per_iteration = 10,
    .timeout_seconds = 30,
    .max_tokens_per_response = 2048,
    .system_prompt_mode = ETHERVOX_GOVERNOR_MODE_FULL,  // Full by default
    .disable_memory_logging = false  // Normal logging by default
};
```

## Logging

### Minimal Mode Logs

```
[Governor] Building MINIMAL system prompt (fast mobile mode, tools unavailable)
[Governor] Minimal system prompt (155 chars) - tools disabled
[Governor] Processing 49 system prompt tokens in chunks...
```

### Secret Mode Logs

```
[Memory] SECRET MODE enabled - memory logging disabled for privacy
[Memory] memory_store called but skipped (secret mode active)
```

## Testing

### Test Minimal Mode

```c
// Create config with minimal mode
ethervox_governor_config_t config = ethervox_governor_default_config();
config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_MINIMAL;

ethervox_governor_init(&governor, &config, tool_registry);
ethervox_governor_load_model(governor, "model.gguf");

// Verify tools_available flag
assert(governor->tools_available == false);

// Ask a question (should get knowledge-based answer, no tool calls)
ethervox_governor_execute(governor, "What is the capital of France?", ...);
```

### Test Secret Mode

```c
// Enable secret mode
ethervox_governor_config_t config = ethervox_governor_default_config();
config.disable_memory_logging = true;

ethervox_governor_init(&governor, &config, tool_registry);
ethervox_memory_set_privacy_mode(true);

// Have a conversation
ethervox_governor_execute(governor, "Remember my birthday is May 5th", ...);

// Export memory (should be empty)
char* export_data;
ethervox_memory_export_jsonl(memory_store, &export_data);
assert(strlen(export_data) == 0 || strstr(export_data, "May 5th") == NULL);
```

## Migration Guide

### Existing Deployments

**Before** (standard loading):
```c
ethervox_governor_config_t config = ethervox_governor_default_config();
ethervox_governor_init(&governor, &config, tool_registry);
ethervox_governor_load_model(governor, model_path);
```

**After** (minimal mode for mobile):
```c
ethervox_governor_config_t config = ethervox_governor_default_config();
config.system_prompt_mode = ETHERVOX_GOVERNOR_MODE_MINIMAL;  // NEW
ethervox_governor_init(&governor, &config, tool_registry);
ethervox_governor_load_model(governor, model_path);
```

**After** (secret mode):
```c
ethervox_governor_config_t config = ethervox_governor_default_config();
config.disable_memory_logging = true;  // NEW
ethervox_governor_init(&governor, &config, tool_registry);
ethervox_governor_load_model(governor, model_path);
ethervox_memory_set_privacy_mode(true);  // NEW (set global flag)
```

## Limitations

### Minimal Mode

- **No tool execution**: LLM cannot invoke tools (memory, files, voice, etc.)
- **Knowledge cutoff**: Responses limited to model's training data
- **No adaptive learning**: Pattern correction and memory search unavailable
- **Static responses**: Cannot access real-time data or perform computations

### Secret Mode

- **No memory recall**: LLM cannot search past conversations
- **No learning**: Patterns and corrections not stored
- **Session-only**: Context resets when app closes (no persistence)
- **Other tools still work**: File writes, voice commands may persist data

## Future Enhancements

### Planned Features

1. **Hybrid Mode**: Start minimal, upgrade to full on-demand
   - Fast initial load
   - Enable tools when needed (e.g., user says "set a reminder")

2. **Selective Privacy**: Fine-grained control over what gets logged
   - Disable specific tools (e.g., memory but allow timers)
   - Tag-based filtering (e.g., allow "general" but block "personal")

3. **Auto Mode Detection**: Automatically choose mode based on:
   - Device capabilities (RAM, CPU)
   - Battery level
   - Query complexity (simple questions → minimal, complex tasks → full)

## See Also

- [GOVERNOR_ARCHITECTURE.md](../GOVERNOR_ARCHITECTURE.md) - Governor system design
- [RELIGHT_SYSTEM.md](RELIGHT_SYSTEM.md) - System prompt recovery
- [CONTEXT_MANAGEMENT_UI_INTEGRATION.md](./CONTEXT_MANAGEMENT_UI_INTEGRATION.md) - Android UI integration
- [errorhandling.md](./errorhandling.md) - Error handling patterns

---

**Copyright (c) 2024-2025 EthervoxAI Team**  
**License**: CC-BY-NC-SA-4.0
