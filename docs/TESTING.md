# EthervoxAI Testing Guide

## Overview

This document describes the test infrastructure for EthervoxAI voice conversation system.

## Test Suites

### 1. Wake Word Detection Tests (`test_wake_word`)

**Location**: `tests/unit/test_wake_word.c`

**Coverage**:
- Wake word initialization and cleanup
- Default configuration validation
- Silence detection (should not trigger)
- Noise rejection (should not trigger)
- Error handling (NULL pointers)
- Template recording

**Run**:
```bash
cd build
./tests/test_wake_word
# or via CTest
ctest -R WakeWord --output-on-failure
```

### 2. Voice Conversation Tests (`test_voice_conversation`)

**Location**: `tests/unit/test_voice_conversation.c`

**Coverage**:
- Session configuration
- Session initialization and lifecycle
- State machine transitions (IDLE → LISTENING → PROCESSING)
- Error handling (trigger before start, NULL pointers)
- Resource cleanup
- Multiple independent sessions
- Timeout behavior (listen timeout, conversation timeout)

**Run**:
```bash
cd build
./tests/test_voice_conversation
# or via CTest
ctest -R VoiceConversation --output-on-failure
```

### 3. Audio Integration Tests (`test_audio_integration`)

**Location**: `tests/unit/test_audio_integration.c`

**Coverage**:
- Audio configuration
- Driver registration and initialization
- Full capture lifecycle (start → read → stop → cleanup)
- Buffer management
- Multiple sequential reads (20ms chunks)
- Device busy handling (graceful failure when mic is in use)

**Run**:
```bash
cd build
./tests/test_audio_integration
# or via CTest
ctest -R AudioIntegration --output-on-failure
```

## Running All Tests

### All Voice Tests
```bash
cd build
ctest -R "WakeWord|VoiceConversation|AudioIntegration" --output-on-failure
```

### All Project Tests
```bash
cd build
ctest --output-on-failure
```

## Known Issues

### Audio Driver Cleanup
**Issue**: The CoreAudio driver cleanup can segfault when called on partially initialized runtimes.

**Impact**: The `test_audio_errors` test is currently skipped.

**Workaround**: Test returns immediately with "SKIPPED" status.

**TODO**: Fix driver cleanup to safely handle partial initialization states.

### Microphone Busy
**Issue**: Audio tests may fail if the microphone is already in use by another application (e.g., EthervoxAI itself).

**Impact**: Tests will fail with "Permission denied or device busy" message.

**Workaround**: 
1. Close any applications using the microphone
2. Run tests again
3. The `test_audio_device_busy` test specifically checks for graceful handling of this case

**Error Message**:
```
Failed to create audio input queue: Permission denied or device busy (error -50)
Tip: Make sure microphone permissions are granted and no other app is using the mic
```

## Test Statistics

- **Total Test Functions**: 19
- **Lines of Test Code**: ~750
- **Test Execution Time**: ~7-15 seconds (depends on Whisper model initialization)
- **Pass Rate**: 100% (1 test skipped)

## Adding New Tests

### 1. Create Test File
```c
// tests/unit/test_new_feature.c
#include <stdio.h>
#include <assert.h>
#include "ethervox/new_feature.h"

static int test_basic_functionality(void) {
    printf("  - test_basic_functionality... ");
    
    // Test code here
    
    printf("PASS\n");
    return 0;
}

int main(void) {
    printf("\n=== New Feature Tests ===\n\n");
    
    int failed = 0;
    failed += test_basic_functionality();
    
    printf("\n");
    if (failed == 0) {
        printf("✓ All tests passed!\n\n");
        return 0;
    } else {
        printf("✗ %d test(s) failed\n\n", failed);
        return 1;
    }
}
```

### 2. Add to CMake
Edit `tests/CMakeLists.txt`:
```cmake
add_executable(test_new_feature unit/test_new_feature.c)
target_link_libraries(test_new_feature ethervoxai)
target_include_directories(test_new_feature PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME NewFeature COMMAND test_new_feature)
set_tests_properties(NewFeature PROPERTIES TIMEOUT 30 LABELS "unit;new_feature")
```

### 3. Build and Run
```bash
cmake --build build --target test_new_feature
./build/tests/test_new_feature
```

## CI/CD Integration

Tests are designed to run in CI environments:
- Fast execution (< 30 seconds per suite)
- Clear pass/fail output
- Graceful handling of missing resources (models, audio devices)
- Return codes: 0 = pass, non-zero = fail

### GitHub Actions Example
```yaml
- name: Run Voice Tests
  run: |
    cd build
    ctest -R "WakeWord|VoiceConversation|AudioIntegration" --output-on-failure
```

## Debugging Failed Tests

### Enable Verbose Output
```bash
cd build
ctest -R TestName -V
```

### Run Directly
```bash
cd build/tests
./test_wake_word
./test_voice_conversation
./test_audio_integration
```

### Check Microphone Permissions
```bash
# macOS
tccutil reset Microphone com.apple.Terminal
# Then grant permission when prompted
```

### Verify Models Exist
```bash
ls ~/.ethervox/models/whisper/
# Should contain base.bin or similar
```

## Test Coverage

### Currently Covered
- ✅ Wake word detection (6 tests)
- ✅ Voice conversation management (7 tests)
- ✅ Audio capture and processing (6 tests)
- ✅ Error handling
- ✅ Resource cleanup
- ✅ Timeout behavior

### TODO: Additional Coverage Needed
- ⏳ Governor LLM integration in conversation
- ⏳ Piper TTS backend
- ⏳ Audio playback
- ⏳ End-to-end conversation flow
- ⏳ Multi-threaded stress tests
- ⏳ Memory leak detection (valgrind)
- ⏳ Performance benchmarks

## Contact

For questions about testing, see:
- `CONTRIBUTING.md` - Contribution guidelines
- `docs/` - Additional documentation
- GitHub Issues - Report test failures
