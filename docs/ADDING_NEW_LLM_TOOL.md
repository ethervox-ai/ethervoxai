# Adding a New LLM Tool to EthervoxAI

This document provides a complete guide for implementing new tools that the LLM Governor can use. Tools are registered functions that the LLM can invoke to interact with the system, files, memory, or external resources.

## Overview

An LLM tool implementation requires changes across multiple layers:

1. **Core Implementation** - The actual functionality (e.g., file operations, memory access)
2. **Tool Wrapper** - JSON parameter parsing and result formatting
3. **Tool Registration** - Schema definition and registry integration
4. **Unit Tests** - Isolated testing of the tool functionality
5. **Integration Tests** - Testing within the live application
6. **LLM Prompt Optimization** - Teaching the Governor to use the tool effectively
7. **Documentation** - Usage examples and guidelines

## Implementation Checklist

### Core Implementation
- [ ] Core function implemented in appropriate module (e.g., `src/plugins/file_tools/`)
- [ ] Function declared in public header (e.g., `include/ethervox/file_tools.h`)
- [ ] Error handling with descriptive messages
- [ ] Platform compatibility checked (macOS, Linux, Windows, Android, ESP32)
- [ ] Logging added for debugging

### Tool Integration
- [ ] Tool wrapper function created (e.g., `tool_file_append_wrapper`)
- [ ] JSON parameter parsing implemented
- [ ] JSON result/error formatting implemented
- [ ] Tool struct defined with all required fields
- [ ] Tool registered in registry (e.g., `ethervox_file_tools_register`)
- [ ] Access control configured (read-only vs read-write)

### Testing
- [ ] Unit test file created (e.g., `tests/unit/test_file_append.c`)
- [ ] Test cases cover success scenarios
- [ ] Test cases cover error scenarios
- [ ] Test cases cover edge cases
- [ ] Integration test added to `/test` command
- [ ] CMakeLists.txt updated with new test executable
- [ ] Tests pass on all target platforms

### LLM Integration
- [ ] Tool added to LLM prompt optimization tests
- [ ] Example usage documented in tool description
- [ ] Parameter descriptions are clear and detailed
- [ ] Tool behavior documented (deterministic, stateful, etc.)
- [ ] Confirmation requirements specified

### Documentation
- [ ] Tool usage documented in README or module docs
- [ ] Examples provided
- [ ] Limitations and platform notes documented
- [ ] This checklist updated with lessons learned

## Step-by-Step Process

### Step 1: Core Implementation

**Example**: `src/plugins/file_tools/file_operations.c`

Implement the core functionality as a reusable function:

```c
int ethervox_file_append(
    const char* file_path,
    const char* content,
    char** error_message
) {
    // Validate inputs
    if (!file_path || !content) {
        if (error_message) {
            *error_message = strdup("Missing file_path or content");
        }
        return -1;
    }
    
    // Open file in append mode
    FILE* f = fopen(file_path, "a");
    if (!f) {
        if (error_message) {
            *error_message = strdup("Failed to open file for appending");
        }
        ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                    "Cannot append to %s: %s", file_path, strerror(errno));
        return -1;
    }
    
    // Write content
    fprintf(f, "%s", content);
    fclose(f);
    
    ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
                "Appended %zu bytes to %s", strlen(content), file_path);
    return 0;
}
```

**Key Principles**:
- Return 0 on success, negative on error
- Use `char** error_message` for detailed error reporting
- Log all important operations
- Validate all inputs
- Handle platform differences with `#ifdef`

### Step 2: Tool Wrapper Function

**Example**: `src/plugins/file_tools/file_registry.c`

Create a wrapper that bridges JSON parameters to your core function:

```c
// Tool wrapper: file_append
static int tool_file_append_wrapper(
    const char* args_json,
    char** result,
    char** error
) {
    ethervox_file_tools_config_t* config = g_file_config;
    
    char file_path[ETHERVOX_FILE_MAX_PATH];
    
    if (!config) {
        *error = strdup("File tools not initialized");
        return -1;
    }
    
    // Parse file_path parameter (support multiple parameter names for compatibility)
    if (parse_json_string(args_json, "file_path", file_path, sizeof(file_path)) != 0) {
        if (parse_json_string(args_json, "path", file_path, sizeof(file_path)) != 0) {
            *error = strdup("Missing 'file_path' or 'path' parameter");
            return -1;
        }
    }
    
    // Extract content parameter
    const char* content_key = "\"content\":\"";
    const char* content_start = strstr(args_json, content_key);
    if (!content_start) {
        content_key = "\"content\": \"";
        content_start = strstr(args_json, content_key);
        if (!content_start) {
            *error = strdup("Missing 'content' parameter");
            return -1;
        }
    }
    
    content_start += strlen(content_key);
    
    // Find closing quote and extract content
    const char* content_end = content_start;
    while (*content_end) {
        if (*content_end == '\\' && *(content_end + 1)) {
            content_end += 2;  // Skip escaped character
        } else if (*content_end == '"') {
            break;
        } else {
            content_end++;
        }
    }
    
    size_t content_len = content_end - content_start;
    char* content = malloc(content_len + 1);
    if (!content) {
        *error = strdup("Memory allocation failed");
        return -1;
    }
    
    strncpy(content, content_start, content_len);
    content[content_len] = '\0';
    
    // Unescape JSON (handle \n, \r, \t, \", \\)
    char* unescaped = unescape_json_string(content);
    free(content);
    
    if (!unescaped) {
        *error = strdup("Failed to unescape content");
        return -1;
    }
    
    // Call core function
    char* err_msg = NULL;
    int ret = ethervox_file_append(file_path, unescaped, &err_msg);
    free(unescaped);
    
    if (ret != 0) {
        *error = err_msg ? err_msg : strdup("Unknown error");
        return -1;
    }
    
    // Format success response
    *result = strdup("{\"success\":true}");
    return 0;
}
```

**Wrapper Requirements**:
- Parse all parameters from JSON string
- Validate parameters
- Call core function
- Format result as JSON string
- Handle errors gracefully
- Free all allocated memory

### Step 3: Tool Registration

**Example**: `src/plugins/file_tools/file_registry.c`

Define the tool schema and register it:

```c
int ethervox_file_tools_register(
    void* registry_ptr,
    ethervox_file_tools_config_t* config
) {
    ethervox_tool_registry_t* registry = (ethervox_tool_registry_t*)registry_ptr;
    
    // ... other registrations ...
    
    // Register file_append tool (only if write access enabled)
    if (config->access_mode == ETHERVOX_FILE_ACCESS_READ_WRITE) {
        ethervox_tool_t tool_append = {
            .name = "file_append",
            .description = "Append content to the end of an existing file. "
                          "Useful for adding to notes, logs, or summaries without overwriting. "
                          "Always provide both path and content.",
            .parameters_json_schema =
                "{\"type\":\"object\",\"properties\":{"
                "\"path\":{\"type\":\"string\",\"description\":\"Path to file to append to\"},"
                "\"file_path\":{\"type\":\"string\",\"description\":\"Path to file to append to (alternative to 'path')\"},"
                "\"content\":{\"type\":\"string\",\"description\":\"The text content to append to the file. Can include newlines.\"}"
                "},\"required\":[\"content\"]}",
            .execute = tool_file_append_wrapper,
            .is_deterministic = false,        // File state can change
            .requires_confirmation = false,    // Silent append for summarization
            .is_stateful = true,              // Modifies file system
            .estimated_latency_ms = 100.0f
        };
        
        ret |= ethervox_tool_registry_add(registry, &tool_append);
    }
    
    return ret;
}
```

**Tool Metadata Fields**:
- `name`: Unique identifier (e.g., "file_append")
- `description`: Clear explanation with usage hints
- `parameters_json_schema`: JSON Schema for parameters
- `execute`: Function pointer to wrapper
- `is_deterministic`: true if same inputs always produce same outputs
- `requires_confirmation`: true if user should approve before execution
- `is_stateful`: true if tool modifies system state
- `estimated_latency_ms`: Expected execution time

**Parameter Schema Best Practices**:
- Use clear, descriptive parameter names
- Provide detailed descriptions
- Support alternative parameter names for LLM optimizer compatibility
- Mark required parameters in `"required"` array
- Use appropriate JSON Schema types (string, number, boolean, object, array)

### Step 4: Unit Tests

**Example**: `tests/unit/test_file_append.c`

Create comprehensive unit tests:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ethervox/file_tools.h"

#define TEST_FILE "test_append.txt"

static void test_append_basic(void) {
    printf("Test 1: Basic append...\n");
    
    // Create initial file
    FILE* f = fopen(TEST_FILE, "w");
    fprintf(f, "Line 1\n");
    fclose(f);
    
    // Append content
    char* error = NULL;
    int ret = ethervox_file_append(TEST_FILE, "Line 2\n", &error);
    
    if (ret != 0) {
        printf("✗ Failed to append: %s\n", error ? error : "unknown");
        free(error);
        exit(1);
    }
    
    // Verify content
    f = fopen(TEST_FILE, "r");
    char buffer[256];
    fgets(buffer, sizeof(buffer), f);
    if (strcmp(buffer, "Line 1\n") != 0) {
        printf("✗ First line corrupted\n");
        exit(1);
    }
    fgets(buffer, sizeof(buffer), f);
    if (strcmp(buffer, "Line 2\n") != 0) {
        printf("✗ Second line not appended\n");
        exit(1);
    }
    fclose(f);
    
    unlink(TEST_FILE);
    printf("  ✓ Basic append works\n");
}

static void test_append_new_file(void) {
    printf("Test 2: Append to new file...\n");
    
    unlink(TEST_FILE);  // Ensure file doesn't exist
    
    char* error = NULL;
    int ret = ethervox_file_append(TEST_FILE, "New content\n", &error);
    
    if (ret != 0) {
        printf("✗ Failed to create and append: %s\n", error ? error : "unknown");
        free(error);
        exit(1);
    }
    
    FILE* f = fopen(TEST_FILE, "r");
    char buffer[256];
    fgets(buffer, sizeof(buffer), f);
    fclose(f);
    
    if (strcmp(buffer, "New content\n") != 0) {
        printf("✗ Content not written correctly\n");
        exit(1);
    }
    
    unlink(TEST_FILE);
    printf("  ✓ Append to new file works\n");
}

static void test_append_errors(void) {
    printf("Test 3: Error handling...\n");
    
    char* error = NULL;
    
    // NULL path
    int ret = ethervox_file_append(NULL, "content", &error);
    if (ret == 0) {
        printf("✗ Should reject NULL path\n");
        exit(1);
    }
    free(error);
    error = NULL;
    
    // NULL content
    ret = ethervox_file_append(TEST_FILE, NULL, &error);
    if (ret == 0) {
        printf("✗ Should reject NULL content\n");
        exit(1);
    }
    free(error);
    error = NULL;
    
    // Invalid path
    ret = ethervox_file_append("/nonexistent/directory/file.txt", "content", &error);
    if (ret == 0) {
        printf("✗ Should reject invalid path\n");
        exit(1);
    }
    free(error);
    
    printf("  ✓ Error handling works\n");
}

static void test_append_multiple(void) {
    printf("Test 4: Multiple appends...\n");
    
    unlink(TEST_FILE);
    
    char* error = NULL;
    ethervox_file_append(TEST_FILE, "Line 1\n", &error);
    ethervox_file_append(TEST_FILE, "Line 2\n", &error);
    ethervox_file_append(TEST_FILE, "Line 3\n", &error);
    
    FILE* f = fopen(TEST_FILE, "r");
    int line_count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), f)) {
        line_count++;
    }
    fclose(f);
    
    if (line_count != 3) {
        printf("✗ Expected 3 lines, got %d\n", line_count);
        exit(1);
    }
    
    unlink(TEST_FILE);
    printf("  ✓ Multiple appends work\n");
}

int main(void) {
    printf("━━━ File Append Unit Tests ━━━\n\n");
    
    test_append_basic();
    test_append_new_file();
    test_append_errors();
    test_append_multiple();
    
    printf("\n━━━ All tests passed! ━━━\n");
    return 0;
}
```

**Test Coverage Requirements**:
- Success cases (normal operation)
- Edge cases (empty content, new file)
- Error cases (NULL parameters, invalid paths)
- Multiple operations (idempotence, state consistency)
- Platform-specific behavior

### Step 5: Integration Tests

**Example**: `src/dialogue/integration_tests.c`

Add a test to the `/test` command suite:

```c
static void test_file_append_tool(void) {
    TEST_HEADER("Test X: File Append Tool");
    
    // Create test file
    const char* test_file = "./test_append_integration.txt";
    FILE* f = fopen(test_file, "w");
    fprintf(f, "Initial content\n");
    fclose(f);
    
    // Test tool wrapper
    const char* args = "{\"path\":\"./test_append_integration.txt\","
                      "\"content\":\"\\n--- Summary ---\\nThis is appended\\n\"}";
    char* result = NULL;
    char* error = NULL;
    
    int ret = tool_file_append_wrapper(args, &result, &error);
    
    if (ret != 0) {
        TEST_FAIL("Tool returned error: %s", error ? error : "unknown");
        free(error);
        unlink(test_file);
        return;
    }
    
    // Verify file content
    f = fopen(test_file, "r");
    char buffer[1024];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
    
    if (!strstr(buffer, "Initial content") || !strstr(buffer, "Summary")) {
        TEST_FAIL("Content not appended correctly");
        unlink(test_file);
        return;
    }
    
    unlink(test_file);
    free(result);
    
    TEST_PASS("File append tool works correctly");
    g_tests_passed++;
}
```

Update `run_integration_tests()`:
```c
void run_integration_tests(void) {
    // ... existing tests ...
    test_file_append_tool();
    // ... more tests ...
}
```

### Step 6: CMakeLists.txt Update

**Example**: `tests/CMakeLists.txt`

Add the new test executable:

```cmake
# File append unit test
add_executable(test_file_append unit/test_file_append.c)
target_link_libraries(test_file_append ethervoxai)
target_include_directories(test_file_append PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME FileAppend COMMAND test_file_append)
set_tests_properties(FileAppend PROPERTIES 
    TIMEOUT 30 
    LABELS "unit;file_tools"
)
```

### Step 7: Dialogue System Registration (IMPORTANT)

**Example**: `src/dialogue/dialogue_core.c`

For tools to be available in the dialogue system (Android app, interactive CLI), you **must** register them in `dialogue_core.c`:

```c
// At the top of the file, add include:
#include "ethervox/unit_conversion.h" // Unit conversion tools

// In the tool registry initialization section (around line 670-690):
    // Register timer tools
    ethervox_tool_registry_add(registry, ethervox_tool_timer_create());
    ethervox_tool_registry_add(registry, ethervox_tool_timer_cancel());
    ethervox_tool_registry_add(registry, ethervox_tool_timer_list());
    ethervox_tool_registry_add(registry, ethervox_tool_alarm_create());
    tool_count += 4;
    
    // Register unit conversion tool
    if (ethervox_unit_conversion_register(registry) == 0) {
      tool_count++;
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGI("Registered unit conversion tool with Governor");
#else
      printf("Registered unit conversion tool with Governor\\n");
#endif
    }
    
    // Register memory tools if memory store is available...
```

**Critical Points**:
- Tools registered in `src/main.c` are only available in the CLI executable
- Tools registered in `dialogue_core.c` are available in both CLI and Android
- Always register platform-independent utility tools in `dialogue_core.c`
- Use `#ifdef ETHERVOX_PLATFORM_ANDROID` for different logging on Android vs desktop

### Step 8: LLM Prompt Optimization

**Example**: Update LLM system prompt or tool usage examples

Add examples of the tool in action to help the LLM learn effective usage:

```c
// In src/governor/governor_prompts.c or similar

const char* FILE_APPEND_EXAMPLES = 
    "Example 1: Appending a summary to a transcript\n"
    "<tool_call name=\"file_append\" path=\"./transcript.txt\">\n"
    "  <content>\n"
    "\n"
    "--- AI Summary ---\n"
    "This conversation covered three main topics:\n"
    "1. Implementation of speaker detection\n"
    "2. Automatic summarization features\n"
    "3. LLM tool optimization\n"
    "  </content>\n"
    "</tool_call>\n"
    "\n"
    "Example 2: Adding to a todo list\n"
    "<tool_call name=\"file_append\" path=\"./todo.md\">\n"
    "  <content>\n"
    "- [ ] Test new file_append tool\n"
    "- [ ] Update documentation\n"
    "  </content>\n"
    "</tool_call>";
```

**LLM Optimization Strategies**:
- Provide clear, concise tool descriptions
- Include usage examples in tool description
- Document when to use vs when NOT to use
- Explain parameter formats (especially for complex types)
- Note common mistakes and how to avoid them

## Tool Categories

### File Tools
- Read/write/append files
- Search file contents
- List directories
- Path configuration

**Access Control**: Read-only vs read-write modes

### Memory Tools
- Store/retrieve memories
- Archive sessions
- Search conversation history
- Context overflow management

**State Management**: Session-scoped vs persistent

### System Tools
- Platform detection
- Resource monitoring
- Configuration management
- Error reporting

**Platform Compatibility**: Desktop, mobile, embedded

### External Tools
- Web search (future)
- API calls (future)
- Database access (future)

**Security**: Sandboxing, rate limiting, authentication

## Common Pitfalls

1. **Poor Parameter Validation**
   - Always validate inputs before use
   - Return clear error messages
   - Don't crash on bad input

2. **Memory Leaks**
   - Free all allocated memory
   - Handle error paths correctly
   - Use tools like valgrind for testing

3. **Platform Assumptions**
   - Test on all target platforms
   - Use `#ifdef` for platform-specific code
   - Provide graceful degradation

4. **Unclear Tool Descriptions**
   - LLM won't use tools it doesn't understand
   - Be specific about what the tool does
   - Include examples in description

5. **Missing Tests**
   - Every tool needs unit tests
   - Every tool needs integration tests
   - Test error cases, not just success

6. **Inconsistent Naming**
   - Follow existing conventions
   - Use module prefixes (e.g., `ethervox_file_*`)
   - Match parameter names across similar tools

7. **JSON Parsing Errors**
   - Handle escaped characters properly
   - Support flexible parameter names
   - Test with complex JSON inputs

8. **Access Control Ignored**
   - Respect read-only vs read-write modes
   - Check permissions before operations
   - Log security-relevant events

## Testing Strategy

### Unit Tests (Isolated)
```bash
./build/tests/test_file_append
```
- Fast execution
- No dependencies
- Specific edge cases

### Integration Tests (System)
```bash
echo "/test" | ./build/ethervoxai
```
- Full system context
- Real tool registry
- End-to-end validation

### LLM Tests (Behavioral)
```bash
echo "Please append 'test' to notes.txt" | ./build/ethervoxai
```
- Natural language interaction
- Tool selection validation
- Result verification

## Files Modified for file_append

1. `src/plugins/file_tools/file_registry.c` (wrapper + registration)
2. `include/ethervox/file_tools.h` (if core function added)
3. `tests/unit/test_file_append.c` (NEW - unit tests)
4. `tests/CMakeLists.txt` (test executable)
5. `src/dialogue/integration_tests.c` (integration test)
6. `src/dialogue/dialogue_core.c` (register tool in dialogue system)
7. `docs/ADDING_NEW_LLM_TOOL.md` (this documentation)

**Note**: For tools that should be available in the dialogue system (Android, interactive CLI), you must also register them in `src/dialogue/dialogue_core.c` in the tool registry initialization section. See the unit_conversion tool registration as an example.

## Performance Considerations

### Tool Latency
- `estimated_latency_ms` helps LLM planning
- Fast tools (<100ms): file operations, memory access
- Medium tools (100ms-1s): search, computation
- Slow tools (>1s): external APIs, downloads

### Resource Usage
- Limit memory allocation
- Stream large results when possible
- Use timeouts for external calls
- Clean up resources promptly

### Concurrency
- Tools may be called in parallel
- Avoid shared mutable state
- Use locks for critical sections
- Document thread-safety

## Security Checklist

- [ ] Input validation prevents injection attacks
- [ ] Path traversal prevented (no `../` escapes)
- [ ] File extension whitelist enforced
- [ ] Size limits prevent resource exhaustion
- [ ] Error messages don't leak sensitive info
- [ ] Permissions checked before operations
- [ ] Audit logging for sensitive operations

## Next Steps

When adding your own tool:

1. Copy this document as a template
2. Identify the tool category and module
3. Follow each step systematically
4. Write tests FIRST (TDD approach)
5. Test on all platforms
6. Update this document with lessons learned

## See Also

- `docs/ADDING_NEW_COMMAND.md` - Adding slash commands
- `docs/modules/` - Module-specific documentation
- `README.md` - Project overview
- `GOVERNOR_ARCHITECTURE.md` - LLM Governor design
- `GOVERNOR_IMPLEMENTATION.md` - Governor implementation details
