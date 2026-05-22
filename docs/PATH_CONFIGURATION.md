# Path Configuration and Safe Mode

This document describes the user path configuration system and safe mode for file access control.

## Overview

EthervoxAI provides two complementary systems for safe and convenient file access:

1. **Path Configuration** - Remember user's important directories (Notes, Documents, etc.)
2. **Safe Mode** - LLM-controlled file access restrictions (like "plan mode")

## Path Configuration

### User Commands

**`/paths`** - List all configured paths
```
> /paths

╭─────────────────────────────────────────────╮
│          Configured User Paths              │
├─────────────────────────────────────────────┤
│ ✓ Documents       │
│   /Users/user/Documents
│   User documents folder
├─────────────────────────────────────────────┤
│ ✓ Downloads       │
│   /Users/user/Downloads
│   Downloaded files
├─────────────────────────────────────────────┤
│ ✗ Notes           │
│   /Users/user/Notes
│   Personal notes (doesn't exist)
╰─────────────────────────────────────────────╯
```

**`/setpath <label> <path> [description]`** - Configure a path
```
> /setpath Notes "/Users/user/My Notes" "Personal note-taking"
✓ Path configured: Notes -> /Users/user/My Notes
```

### LLM Tools

The Governor LLM has access to these tools:

**`path_list`** - Get all configured paths
```json
{
  "paths": [
    {
      "label": "Documents",
      "path": "/Users/user/Documents",
      "description": "User documents folder",
      "verified": true
    }
  ]
}
```

**`path_get`** - Retrieve specific path by label
```json
{
  "label": "Notes"
}
// Returns: {"path": "/Users/user/Notes", "verified": true}
```

**`path_set`** - Configure new path (LLM can remember user's directories)
```json
{
  "label": "Projects",
  "path": "/Users/user/dev/projects",
  "description": "Software development projects"
}
```

**`path_check_unverified`** - Find paths that need configuration
```json
// Returns list of unverified paths so LLM can ask user for corrections
{
  "unverified": [
    {
      "label": "Notes",
      "expected_path": "/Users/user/Notes",
      "description": "Personal notes"
    }
  ],
  "count": 1,
  "message": "Some default paths don't exist. Consider asking the user..."
}
```

### Behavior

**Automatic Discovery**:
- On first run, system checks for standard paths (Documents, Downloads, Desktop, Notes)
- Paths are marked ✓ (verified) if they exist, ✗ if not found
- User is prompted to configure missing paths

**Persistence**:
- Paths are stored in memory with tag `user_path`
- Format: `USER_PATH:label|description|/full/path`
- Survives across sessions via memory persistence

**LLM Workflow**:
1. LLM uses `path_check_unverified` to find missing paths
2. Asks user: "I notice you don't have a Notes directory configured. Where do you keep your notes?"
3. Uses `path_set` to remember the location
4. Future requests use `path_get` to access the correct path

## Safe Mode

Safe mode allows the LLM to restrict its own file write access, similar to "plan mode" in other AI assistants.

### User Commands

**`/safemode`** - Toggle safe mode on/off
```
> /safemode
🔒 Safe Mode: ENABLED (read-only)
   LLM can explore and read files but cannot modify them.
   This is similar to 'plan mode' - use for safe exploration.

> /safemode
🔓 Safe Mode: DISABLED (read-write)
   LLM can read and write files when needed.
   ⚠️  Exercise caution with file modifications.
```

### LLM Tool

**`file_set_safe_mode`** - LLM can enable/disable safe mode itself
```json
{
  "enable": true
}
// Response:
{
  "safe_mode": true,
  "access_mode": "read_only",
  "message": "File access is now restricted to read-only (safe mode). I can explore and read files but cannot modify them."
}
```

### Recommended LLM Behavior

The LLM should:

1. **Enable safe mode by default** when exploring user files
   ```
   User: "What's in my Documents folder?"
   LLM: [Calls file_set_safe_mode(enable=true)]
   LLM: [Uses file_list to explore]
   ```

2. **Ask permission before disabling** safe mode
   ```
   User: "Create a new file called notes.txt"
   LLM: "I'll need to enable write access to create files. Is that okay?"
   User: "Yes"
   LLM: [Calls file_set_safe_mode(enable=false)]
   LLM: [Calls file_write]
   ```

3. **Re-enable safe mode** after write operations
   ```
   LLM: [After writing file]
   LLM: [Calls file_set_safe_mode(enable=true)]
   ```

### Visual Indicators

**Startup Message**:
```
File Tools: Registered with Governor (🔓 read-write: .txt/.md/.org/.c/.cpp/.h/.sh)
```
or
```
File Tools: Registered with Governor (🔒 read-only (safe mode): .txt/.md/.org/.c/.cpp/.h/.sh)
            LLM can use file_set_safe_mode tool to enable writes when needed
```

**Command Response**:
- 🔒 indicates read-only (safe mode active)
- 🔓 indicates read-write (safe mode disabled)

## Platform Support

### Desktop (macOS, Linux, Windows)
- Full path configuration support
- Default paths automatically detected from environment
- Safe mode fully functional

### Android
- Path configuration uses app's internal storage context
- Default paths adjusted for Android filesystem
- Safe mode functional within app sandbox

### ESP32
- Limited filesystem (SPIFFS/LittleFS)
- Path configuration may have reduced functionality
- Safe mode works but limited by filesystem capabilities

## Implementation Details

### Path Storage Format

Paths are stored as memory entries:
```
Text: "USER_PATH:Notes|Personal note-taking|/Users/user/Notes"
Tags: ["user_path"]
```

### Safe Mode Implementation

Safe mode modifies the `ethervox_file_tools_config_t` structure:
```c
typedef enum {
    ETHERVOX_FILE_ACCESS_READ_ONLY = 0,
    ETHERVOX_FILE_ACCESS_READ_WRITE = 1
} ethervox_file_access_mode_t;

// In config:
config->access_mode = ETHERVOX_FILE_ACCESS_READ_ONLY;  // Safe mode
```

When `file_write` is called in safe mode, it returns an error.

## Security Considerations

1. **Path Validation** - All paths are validated before storage
2. **Base Path Restrictions** - File access is limited to configured base paths
3. **Extension Filtering** - Only allowed file types (.txt, .md, etc.) can be accessed
4. **LLM Self-Restriction** - LLM can proactively enable safe mode for cautious exploration
5. **User Control** - Users can override with `/safemode` command at any time

## Examples

### Example 1: LLM Discovers Missing Path

```
User: "Show me my notes"

LLM: [Calls path_check_unverified]
LLM: "I see that the default Notes path (/Users/user/Notes) doesn't exist on your system. 
      Where do you keep your notes?"

User: "They're in /Users/user/Dropbox/Notes"

LLM: [Calls path_set(label="Notes", path="/Users/user/Dropbox/Notes")]
LLM: "Got it! I've saved your Notes location. Let me show you what's there..."
LLM: [Calls file_list using the configured path]
```

### Example 2: Safe Exploration Then Modification

```
User: "Look through my documents and fix any TODO comments"

LLM: [Calls file_set_safe_mode(enable=true)]
LLM: [Calls path_get(label="Documents")]
LLM: [Calls file_list to explore]
LLM: [Calls file_read on several files]
LLM: "I found 3 files with TODO comments. Would you like me to update them?"

User: "Yes, please"

LLM: [Calls file_set_safe_mode(enable=false)]
LLM: [Calls file_write to update files]
LLM: [Calls file_set_safe_mode(enable=true)]
LLM: "Done! I've updated all 3 files and re-enabled safe mode."
```

## See Also

- `docs/ADDING_NEW_COMMAND.md` - How to add new commands
- `docs/file-tools-plugin.md` - File tools plugin documentation
- `include/ethervox/file_tools.h` - API reference
