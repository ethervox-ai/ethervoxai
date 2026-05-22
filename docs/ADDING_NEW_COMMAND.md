# Adding a New Slash Command to EthervoxAI

This document demonstrates the complete process of adding a new command to the system using `/archive` as an example.

## Overview

A complete slash command implementation requires changes across multiple layers:

1. **Core Implementation** - The actual functionality (in this case, memory archiving)
2. **Main Application** - Command parsing and invocation
3. **Unit Tests** - Automated testing of the core functionality
4. **Integration Tests** - Testing within the live application
5. **LLM Tool Prompts** - Teaching the Governor about the new capability (optional for slash commands)
6. **Cross-Platform Support** - Ensuring it works on all target platforms

## Implementation Checklist

- [x] Core function implementation (`.c` file)
- [x] Header declaration (`.h` file)
- [x] Command handler in `main.c`
- [x] Help text updated
- [x] Command autocomplete list updated
- [x] Unit tests created
- [x] Integration test added
- [x] CMakeLists.txt updated
- [x] Documentation created

## Step-by-Step Process

### Step 1: Core Implementation

**File**: `src/plugins/memory_tools/memory_archive.c`

The core implementation should:
- Be platform-agnostic where possible
- Use platform abstraction for file operations
- Handle errors gracefully
- Log operations for debugging
- Return standardized error codes

**Key Principles**:
- Single Responsibility: Do one thing well
- Platform Independence: Use `#ifdef` for platform-specific code
- Error Handling: Return -1 on error, 0 on success
- Logging: Use `ethervox_log()` for all important operations

**Example**:
```c
int ethervox_memory_archive_sessions(
    ethervox_memory_store_t* store,
    uint32_t* files_archived
) {
    if (!store || !store->is_initialized) {
        return -1;
    }
    
    // Platform-specific logic with fallbacks
    #if defined(ETHERVOX_PLATFORM_ESP32)
        ethervox_log(LOG_WARN, "Archive not supported on ESP32");
        return -1;
    #else
        // Desktop/Android implementation
        mkdir(archive_dir, 0755);
    #endif
    
    // ... archiving logic ...
    
    ethervox_log(LOG_INFO, "Archived %u files", count);
    return 0;
}
```

### Step 2: Header Declaration

**File**: `include/ethervox/memory_tools.h`

Add the function declaration to the public API:
```c
/**
 * Archive all previous session files
 * 
 * Moves all .jsonl files (except current session) to an archive subdirectory.
 * Useful for cleanup while preserving history.
 * 
 * @param store Memory store (must be initialized)
 * @param files_archived Output: number of files archived
 * @return 0 on success, negative on error
 */
int ethervox_memory_archive_sessions(
    ethervox_memory_store_t* store,
    uint32_t* files_archived
);
```

**Documentation Requirements**:
- Brief description
- Behavior explanation
- Parameter documentation
- Return value documentation
- Platform notes (if applicable)

### Step 3: Command Handler in main.c

**File**: `src/main.c`

Add command parsing in the main input loop:
```c
if (strcmp(line, "/archive") == 0) {
    uint32_t archived = 0;
    if (ethervox_memory_archive_sessions(&memory, &archived) == 0) {
        printf("✓ Archived %u session file(s)\n", archived);
    } else {
        printf("✗ Failed to archive sessions\n");
    }
    return;
}
```

**Also update**:
1. Help text (in `print_help()`)
2. Command autocomplete list (static `commands[]` array)
3. Slash command list (static `slash_commands[]` array if exists)

### Step 4: Unit Tests

**File**: `tests/unit/test_memory_archive.c`

Create comprehensive unit tests:
- Test successful operation
- Test edge cases (empty directory, no storage)
- Test error conditions
- Test idempotence (multiple runs)

**Test Structure**:
```c
static void test_archive_basic(void);
static void test_archive_empty(void);
static void test_archive_errors(void);

int main(void) {
    printf("Running memory_archive tests...\n");
    test_archive_basic();
    test_archive_empty();
    test_archive_errors();
    printf("All tests passed!\n");
    return 0;
}
```

### Step 5: Integration Tests

**File**: `src/dialogue/integration_tests.c`

Add test to the `/test` command suite:
```c
static void test_memory_archive(void) {
    TEST_HEADER("Test 7: Memory Archiving");
    
    // Create test files
    // Run archive
    // Verify results
    
    TEST_PASS("Archived %u files", count);
    g_tests_passed++;
}
```

Update `run_integration_tests()` to call your new test.

### Step 6: Build System Integration

**File**: `tests/CMakeLists.txt`

Add unit test executable:
```cmake
add_executable(test_memory_archive unit/test_memory_archive.c)
target_link_libraries(test_memory_archive ethervoxai)
target_include_directories(test_memory_archive PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME MemoryArchive COMMAND test_memory_archive)
set_tests_properties(MemoryArchive PROPERTIES TIMEOUT 30 LABELS "unit")
```

### Step 7: Cross-Platform Considerations

Different platforms have different file system capabilities:

**Desktop (macOS/Linux/Windows)**:
- Full filesystem access
- Directory creation with `mkdir()` or `CreateDirectory()`
- File moving with `rename()` or `MoveFile()`

**Android**:
- Use app's internal storage context
- May need Java JNI calls for complex operations
- Respect Android storage permissions

**ESP32**:
- Limited SPIFFS/LittleFS filesystem
- May not support directories
- Gracefully degrade or return error

**Implementation Pattern**:
```c
#if defined(ETHERVOX_PLATFORM_ESP32)
    // ESP32 may not support directories
    ethervox_log(LOG_WARN, "Archive not supported on ESP32");
    return -1;
#elif defined(ETHERVOX_PLATFORM_ANDROID)
    // Android-specific implementation
    mkdir(archive_dir, 0750);
#else
    // Desktop platforms
    mkdir(archive_dir, 0755);
#endif
```

## Testing Results

### Unit Tests
```
━━━ Memory Archive Unit Tests ━━━

Test 1: Basic archiving...
  ✓ Basic archiving works
Test 2: Archive with no old files...
  ✓ Empty archive handled correctly
Test 3: Archive with no storage directory...
  ✓ No-storage case handled correctly
Test 4: Multiple archive runs (idempotence)...
  ✓ Multiple runs are idempotent

━━━ All tests passed! ━━━
```

### Integration Test
```
=== Test 7: Memory Archiving ===
  ✓ Archived 3 session files
  ✓ Archive directory created and files moved
```

### Live Usage
```
> /archive
✓ Archived 4 session file(s) to archive/ subdirectory

> /help
  /archive           Move old session files to archive/
```

## Common Pitfalls

1. **Forgetting to update CMakeLists.txt** - New files won't be compiled
2. **Not handling platform differences** - Code works on desktop but fails on ESP32
3. **Missing error checking** - Silent failures are hard to debug
4. **Poor documentation** - Users won't know how to use the feature
5. **No tests** - Regressions will occur
6. **Inconsistent naming** - Follow existing conventions (e.g., `ethervox_memory_*`)
7. **Not updating help text** - Feature is invisible to users
8. **Forgetting autocomplete** - Poor user experience

## Files Modified for /archive

1. `src/plugins/memory_tools/memory_archive.c` (NEW)
2. `include/ethervox/memory_tools.h` (header declaration added)
3. `src/main.c` (command handler, help text, autocomplete)
4. `src/dialogue/integration_tests.c` (integration test added)
5. `tests/unit/test_memory_archive.c` (NEW)
6. `tests/CMakeLists.txt` (test executable added)
7. `docs/ADDING_NEW_COMMAND.md` (this documentation)

## Next Steps

When adding your own command:

1. Copy this document as a template
2. Replace "archive" with your command name
3. Follow each step systematically
4. Run tests frequently (`cmake --build build && ./build/tests/test_yourfeature`)
5. Test on target platforms before committing
6. Update this document with lessons learned

## See Also

- `docs/modules/` - Module-specific documentation
- `README.md` - Project overview and build instructions
- `CONTRIBUTING.md` - Contribution guidelines
