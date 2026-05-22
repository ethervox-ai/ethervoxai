# Secret Mode / Privacy Mode Implementation

## Overview

Secret mode (privacy mode) prevents memory logging while maintaining normal LLM operation and tool execution. This is critical for:

1. **Startup prompts** - System initialization prompts should not clutter conversation history
2. **Sensitive queries** - User-triggered privacy mode for confidential conversations
3. **Testing/debugging** - Prevent test data from polluting memory stores

## Architecture

### Core Components

1. **Privacy Mode State** (`src/plugins/memory_tools/memory_registry.c`)
   - Global flag: `g_disable_memory_logging` (static)
   - Setter: `ethervox_memory_set_privacy_mode(bool)`
   - Getter: `ethervox_memory_get_privacy_mode(void)`

2. **Memory Interception** (`src/plugins/memory_tools/memory_registry.c:151`)
   ```c
   if (g_disable_memory_logging) {
       // Return success but skip actual storage
       return 0;
   }
   ```

3. **Startup Prompt Integration** (`src/main.c:1901-1924`)
   - Saves original privacy mode state
   - Enables privacy mode temporarily
   - Executes startup prompt
   - Restores original state

### API

```c
// Enable secret mode (disable memory logging)
void ethervox_memory_set_privacy_mode(bool disable_logging);

// Get current privacy mode state
bool ethervox_memory_get_privacy_mode(void);
```

## Implementation Details

### Startup Prompt Secret Mode

**Location**: `src/main.c` lines 1901-1924

**Behavior**:
- Automatically wraps startup prompt execution in secret mode
- Transparent to user (no UI indication required)
- Preserves any existing privacy mode state
- Works on desktop, ESP32, and Android (via shared code)

**Code Pattern**:
```c
// Save current privacy mode
bool original_privacy_mode = ethervox_memory_get_privacy_mode();

// Enable secret mode
ethervox_memory_set_privacy_mode(true);

// Execute startup prompt
ethervox_governor_execute(governor, startup_prompt, ...);

// Restore original state
ethervox_memory_set_privacy_mode(original_privacy_mode);
```

### Manual Secret Mode (CLI)

**Location**: `src/main.c` lines 906-918

**Usage**: `/secret` command toggles privacy mode
- Shows 🔒 icon in prompt when enabled
- User controls when to enable/disable
- State persists across queries until toggled again

### Android Secret Mode (JNI)

**Location**: `src/platform/ethervox_android_core.c` lines 1912-1924

**Method**: `Java_ai_ethervox_core_NativeCore_setSecretMode()`
- Android app can toggle privacy mode
- Same backend implementation as CLI
- No memory logging while enabled

## Testing

### Unit Tests

**File**: `tests/unit/test_secret_mode.c`

**Coverage**:
1. ✅ Privacy mode toggle (get/set)
2. ✅ Memory logging prevention
3. ✅ State restoration
4. ✅ Search behavior (reads work, writes blocked)
5. ✅ Tool execution (succeeds without logging)
6. ✅ Multiple rapid toggles

**Run Tests**:
```bash
cd build
ctest -R SecretMode -V
```

### Integration Testing

To verify startup prompt secret mode:
```bash
# Run with custom startup prompt
./build/ethervoxai --model path/to/model.gguf

# Check memory store after startup
cat ~/.ethervox/*.jsonl | grep "startup"
# Should NOT find the startup prompt in memory
```

## Key Behaviors

### What Secret Mode Does

✅ **Prevents**:
- Memory store writes (conversations not saved)
- Adaptive memory corrections (no pattern learning)
- Context markers (no "Context cleared" entries)

✅ **Allows**:
- Memory searches (reading existing memories)
- Tool execution (tools work normally)
- LLM inference (responses generated)
- Governor reasoning (multi-step tool calls)

### State Preservation

The implementation uses **save-restore pattern**:
```c
bool original = ethervox_memory_get_privacy_mode();
ethervox_memory_set_privacy_mode(true);
// ... do secret operation ...
ethervox_memory_set_privacy_mode(original);
```

This ensures:
- No side effects on global state
- Nested secret operations work correctly
- User's privacy preference preserved

## Use Cases

### 1. Startup Prompts (Automatic)

**Problem**: System initialization clutters conversation history

**Solution**: Wrap in secret mode automatically
```c
// In main.c startup sequence
bool original_privacy = ethervox_memory_get_privacy_mode();
ethervox_memory_set_privacy_mode(true);
ethervox_governor_execute(governor, startup_prompt, ...);
ethervox_memory_set_privacy_mode(original_privacy);
```

### 2. Sensitive Conversations (User-Triggered)

**Problem**: User discussing confidential information

**Solution**: Manual secret mode toggle
```bash
> /secret
🔒 SECRET MODE ON
> What's my password for example.com?
[Response not saved to memory]
> /secret
SECRET MODE OFF
```

### 3. Testing/Debugging (Developer)

**Problem**: Test queries pollute production memory

**Solution**: Enable secret mode during tests
```c
ethervox_memory_set_privacy_mode(true);
run_llm_tests();
ethervox_memory_set_privacy_mode(false);
```

## Platform Support

| Platform | Supported | Implementation |
|----------|-----------|----------------|
| **Desktop (Linux/macOS/Windows)** | ✅ | `src/main.c` |
| **ESP32** | ✅ | `esp32-project/src/main.c` |
| **Android** | ✅ | `src/platform/ethervox_android_core.c` |
| **iOS** | ⚠️ | Not yet implemented (would use same backend) |

## Security Notes

### What Secret Mode Is NOT

❌ **NOT encryption** - Messages still in RAM, just not persisted to disk
❌ **NOT authentication** - Anyone with access can toggle secret mode
❌ **NOT deletion** - Existing memories remain (only new writes blocked)

### Privacy Guarantees

✅ **Guarantees**:
- No disk writes to `.jsonl` memory files
- No adaptive memory pattern learning
- No conversation history accumulation

⚠️ **Limitations**:
- Responses still appear in terminal/UI
- Queries still processed by LLM (in RAM)
- Tool results still returned (not logged)

## Future Enhancements

### Potential Improvements

1. **Selective Secret Mode**
   - Mark individual queries as secret (parameter to `governor_execute()`)
   - More granular than global toggle

2. **Memory Scrubbing**
   - Command to delete memories containing specific keywords
   - Post-hoc privacy enforcement

3. **Temporary Secret Windows**
   - Auto-disable secret mode after N queries
   - Time-based privacy windows

4. **Audit Logging**
   - Track when secret mode was enabled/disabled
   - Transparency for security audits

## Changelog

**2025-12-09**:
- ✅ Added `ethervox_memory_get_privacy_mode()` getter
- ✅ Implemented startup prompt secret mode (automatic)
- ✅ Created comprehensive unit test suite
- ✅ Documented architecture and use cases
- ✅ Synced across desktop/ESP32 platforms

**Previous**:
- 2024: Initial secret mode implementation (`/secret` command)
- 2024: Android JNI secret mode binding

## References

- **Privacy Mode Implementation**: `src/plugins/memory_tools/memory_registry.c`
- **Startup Prompt Integration**: `src/main.c:1901-1924`
- **Unit Tests**: `tests/unit/test_secret_mode.c`
- **CLI Command**: `src/main.c:906-918` (`/secret`)
- **Android JNI**: `src/platform/ethervox_android_core.c:1912-1924`
