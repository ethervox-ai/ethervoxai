# File Tools Plugin

## Overview

The File Tools plugin provides controlled file system access for the Governor LLM, allowing it to read and optionally write text documents with comprehensive security controls.

## Features

### Security Controls

- **Path Sandboxing**: Only allowed base directories can be accessed
- **Extension Filtering**: Whitelist of allowed file extensions (.txt, .md, .org by default)
- **Size Limits**: Maximum 10MB per file to prevent memory issues
- **Read-Only Default**: Write access is disabled by default and must be explicitly enabled
- **Path Validation**: Uses `realpath()` to prevent directory traversal attacks

### Available Tools

#### 1. `file_list`
Lists files and directories in a given path.

**Parameters:**
- `directory` (string, required): Directory path to list
- `recursive` (boolean, optional): Recurse into subdirectories

**Returns:**
```json
{
  "entries": [
    {
      "name": "notes.org",
      "path": "/home/user/Documents/notes.org",
      "is_directory": false,
      "size": 4096
    }
  ],
  "count": 1
}
```

#### 2. `file_read`
Reads the contents of a text file.

**Parameters:**
- `file_path` (string, required): Path to file to read

**Constraints:**
- File must have allowed extension (.txt, .md, .org)
- File size must be ≤ 10MB
- Path must be within allowed base directories

**Returns:**
```json
{
  "content": "File content here...",
  "size": 1234
}
```

#### 3. `file_search`
Searches for text patterns in all allowed files within a directory.

**Parameters:**
- `directory` (string, required): Directory to search in
- `pattern` (string, required): Text pattern to search for

**Returns:**
```json
{
  "matches": [
    "/path/to/file1.txt",
    "/path/to/file2.md"
  ],
  "count": 2
}
```

#### 4. `file_write` (Optional)
Writes content to a file. Only available if write access is enabled.

**Parameters:**
- `file_path` (string, required): Path to file to write
- `content` (string, required): Content to write

**Security:**
- Requires `ETHERVOX_FILE_ACCESS_READ_WRITE` mode
- Tool is marked `requires_confirmation = true`
- Same path/extension restrictions as read operations

## Configuration

### Initialization

```c
ethervox_file_tools_config_t config;

// Define allowed base paths
const char* base_paths[] = {
    "/home/user/Documents",
    "/home/user/notes",
    NULL  // Null-terminated
};

// Initialize (read-only by default)
ethervox_file_tools_init(&config, base_paths, ETHERVOX_FILE_ACCESS_READ_ONLY);

// Add allowed file extensions
ethervox_file_tools_add_filter(&config, ".txt");
ethervox_file_tools_add_filter(&config, ".md");
ethervox_file_tools_add_filter(&config, ".org");

// Register with Governor
ethervox_file_tools_register(&registry, &config);
```

### Access Modes

- `ETHERVOX_FILE_ACCESS_READ_ONLY`: Only read operations (default, secure)
- `ETHERVOX_FILE_ACCESS_READ_WRITE`: Allow write operations (use with caution)

### File Extension Filters

By default, no extensions are allowed. You must explicitly add each allowed extension:

```c
ethervox_file_tools_add_filter(&config, ".txt");   // Plain text
ethervox_file_tools_add_filter(&config, ".md");    // Markdown
ethervox_file_tools_add_filter(&config, ".org");   // Org-mode
```

Maximum of 16 file extensions can be registered.

## Platform-Aware System Prompts

The Governor system prompt automatically adapts based on platform:

### Desktop Platform (macOS, Linux, Windows)
- Emphasizes memory and file tools as "[KEY]" capabilities
- Provides comprehensive examples including:
  - Memory storage and recall
  - File reading and searching
  - Document summarization
- Encourages contextual, detailed responses
- Longer system prompt with full tool usage patterns

### Mobile Platform (Android, iOS)
- Concise, voice-optimized prompts
- Brief examples focused on common voice queries
- Emphasis on quick, actionable responses (1-2 sentences max)
- Voice-friendly formatting
- Shorter system prompt to conserve memory

Platform detection is automatic via compile-time macros.

## Security Best Practices

1. **Minimize Base Paths**: Only allow directories that the LLM genuinely needs
2. **Read-Only by Default**: Only enable write access when absolutely necessary
3. **Extension Whitelist**: Only allow file types the LLM should process
4. **Path Validation**: The plugin uses `realpath()` to prevent `../` attacks
5. **Size Limits**: 10MB maximum prevents memory exhaustion
6. **No Hidden Files**: Files starting with `.` are excluded by default

## Integration with Main CLI

The main CLI (`src/main.c`) automatically initializes file tools with:

- **Allowed paths**: `$HOME`, `$HOME/Documents`, and current working directory
- **Access mode**: Read-only
- **Extensions**: `.txt`, `.md`, `.org`

To enable write access, modify the initialization in `main.c`:

```c
ethervox_file_tools_init(&file_config, base_paths, ETHERVOX_FILE_ACCESS_READ_WRITE);
```

## Example Usage

**User**: "Read my notes.org file"

**Governor**:
```xml
<tool_call name="file_read" file_path="/home/user/Documents/notes.org" />
```

**User**: `<tool_result>{"content": "* TODO Complete file tools\n* DONE Add security", "size": 48}</tool_result>`

**Governor**: "Your notes.org has two tasks: one pending (Complete file tools) and one completed (Add security)."

---

**User**: "Find files about meetings"

**Governor**:
```xml
<tool_call name="file_search" directory="/home/user/Documents" pattern="meeting" />
```

**User**: `<tool_result>{"matches": ["meeting-2024.txt", "team-meetings.md"], "count": 2}</tool_result>`

**Governor**: "Found 2 files about meetings: meeting-2024.txt and team-meetings.md in your Documents folder."

## Implementation Files

- **Header**: `include/ethervox/file_tools.h` - Public API
- **Core**: `src/plugins/file_tools/file_core.c` - File operations with security
- **Registry**: `src/plugins/file_tools/file_registry.c` - Governor integration

## Build Integration

Added to `CMakeLists.txt`:

```cmake
# Add file tools plugin sources
file(GLOB FILE_TOOLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/file_tools/*.c")
list(APPEND ETHERVOXAI_CORE_SOURCES ${FILE_TOOLS_SOURCES})
message(STATUS "File tools plugin sources included")
```

## Testing

1. Build with file tools:
   ```bash
   npm run build
   ```

2. Run CLI without auto-loaded Governor:
   ```bash
   ./build/ethervoxai --noautoload
   ```

3. Test file reading:
   ```
   > Read my test.txt file
   ```

4. Test file search:
   ```
   > Find files containing "TODO"
   ```

## Future Enhancements

- File watching for real-time updates
- Directory creation (with write access)
- File metadata queries (last modified, permissions)
- Binary file detection and rejection
- Configurable size limits per file type
- Extension-based content preview limits
