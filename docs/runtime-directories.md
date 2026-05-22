# EthervoxAI Runtime Directory Structure

EthervoxAI uses a standardized directory structure at `~/.ethervox/` for all runtime data and configuration.

## Directory Layout

```
~/.ethervox/
├── models/              # GGUF model files (recommended location)
├── memory/              # Conversation memory (.jsonl files)
├── tests/               # Test reports and crash logs
├── startup_prompt.txt   # Custom startup instruction
└── tool_prompts_*.json  # Optimized per-model tool descriptions
```

## Directory Details

### `~/.ethervox/models/`
**Purpose**: Recommended location for GGUF model files

**Usage**:
- Store your downloaded GGUF models here
- Example: `~/.ethervox/models/granite-4.1-3b-Q6_K.gguf`
- Load with: `--model ~/.ethervox/models/granite-4.1-3b-Q6_K.gguf`

### `~/.ethervox/memory/`
**Purpose**: Persistent conversation memory storage

**Contents**:
- Session-based `.jsonl` files (JSON Lines format)
- Example: `~/.ethervox/memory/default-2025-12-03.jsonl`
- Each line is a memory entry with ID, content, tags, timestamp, etc.

**Auto-generated**: Created automatically when memory persistence is enabled

**Default**: Used when `--memory` flag is not specified and persistence is enabled

### `~/.ethervox/tests/`
**Purpose**: Test logs and crash reports

**Contents**:
- `llm_test_report_<timestamp>.log` - Test execution reports from `/testllm`
- `llm_test_crash_<timestamp>.log` - Crash reports if tests fail unexpectedly

**Auto-generated**: Created automatically when running `/testllm` command

### `startup_prompt.txt`
**Purpose**: Custom startup instruction executed when model loads

**Format**: Plain text instruction that tells the model what to do at startup

**Example**:
```
Please greet me. Check what time and date it is. Search your memory for any reminders, notes, or important information I should know about today. If you find anything important, let me know.
```

**Management Commands**:
- `/startup edit` - Edit in $EDITOR (nano/vim)
- `/startup show` - Display current prompt
- `/startup reset` - Remove custom prompt (use default)

**Auto-generated**: Created by `/optimize_tool_prompts` Phase 4

### `tool_prompts_<model>.json`
**Purpose**: Model-specific optimized tool descriptions

**Format**: JSON file with per-tool "when" and "example" guidance

**Example**: `tool_prompts_granite.json`

**Contents**:
```json
{
  "model_path": "~/.ethervox/models/granite-4.1-3b-Q6_K.gguf",
  "generated_at": 1733201234,
  "preferences": "Model's preferred instruction style...",
  "tools": [
    {
      "name": "memory_store",
      "when": "Call memory_store when the user shares important information...",
      "example": "User: My favorite color is blue\nAssistant: <tool_call..."
    }
  ]
}
```

**Auto-generated**: Created by `/optimize_tool_prompts` command

**Auto-loaded**: Automatically loaded at startup if file exists for current model

## Platform Differences

### macOS/Linux
- Base: `~/.ethervox/` (user home directory)
- All directories created automatically on first run

### Windows
- Base: `%USERPROFILE%\.ethervox\` (Windows user profile)
- Directory creation uses `_mkdir()` for Windows compatibility

### Android
- Memory: Application-specific storage (not `~/.ethervox`)
- Models: Bundled or downloaded to app storage
- See `docs/ANDROID_INTEGRATION.md` for details

### ESP32
- Memory: Optional persistence to `/spiffs/memory`
- Models: Flash storage or external SD card
- Limited by available flash/RAM

## Migration from Old Paths

If you have existing data in old locations:

**Old memory location**: `./memory_data/` → **New**: `~/.ethervox/memory/`
```bash
mv ./memory_data/*.jsonl ~/.ethervox/memory/
```

**Old startup prompt**: `./.ethervox_startup_prompt.txt` → **New**: `~/.ethervox/startup_prompt.txt`
```bash
mv ./.ethervox_startup_prompt.txt ~/.ethervox/startup_prompt.txt
```

**Old tool prompts**: `.ethervox_tool_prompts_*.json` → **New**: `~/.ethervox/tool_prompts_*.json`
```bash
mv .ethervox_tool_prompts_*.json ~/.ethervox/
```

**Old test reports**: `./test_reports/` → **New**: `~/.ethervox/tests/`
```bash
mv ./test_reports/*.log ~/.ethervox/tests/
```

## Initialization

The directory structure is created automatically on application startup via `ensure_ethervox_directories()` in `src/main.c`.

If `$HOME` is not set, the app falls back to current directory (`./.ethervox/`).

## Cleanup

To reset EthervoxAI to a clean state:
```bash
rm -rf ~/.ethervox/memory/*.jsonl    # Clear memory
rm ~/.ethervox/startup_prompt.txt    # Reset startup prompt
rm ~/.ethervox/tool_prompts_*.json   # Remove optimized prompts
```

To completely remove all EthervoxAI data:
```bash
rm -rf ~/.ethervox/
```

Note: Models are preserved unless explicitly deleted.
