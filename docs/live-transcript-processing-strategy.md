# Live Transcript Processing & Smart File Reading Strategy

**Date**: December 3, 2025  
**Branch**: feat/voice-tool  
**Goal**: Enable small LLM to comprehend large files and live transcripts efficiently

## Overview

Implement intelligent file chunking, live transcript processing, and post-transcription compression to enable small context window LLMs (2K-8K tokens) to work with large documents and long conversations.

## Architecture

### Core Principles
1. **LLM-driven processing**: LLM decides what's important, not automatic rules
2. **Progressive disclosure**: Show metadata first, read details on-demand
3. **Memory integration**: Store summaries/extracts in memory system
4. **Binary safety**: Detect and reject non-text content early
5. **Context-aware limits**: Respect remaining context space dynamically

### Three-Phase Transcript Processing

**Phase 1: Live Stream (During `/transcribe`)**
- LLM monitors incoming transcript chunks in real-time
- Extracts: dates, tasks, action items, technical terms, key topics
- Stores "dirty notes" in memory with `live_transcript` tag
- Optional: `--no-summary` flag disables live processing

**Phase 2: Post-Transcription Compression (After `/stoptranscribe`)**
- LLM re-reads full transcript in chunks (if needed)
- Generates clean compressed summary
- Extracts detailed notes: tasks, decisions, questions, technical details
- Stores compressed version in memory, archives full transcript

**Phase 3: Retrieval**
- LLM queries memory for transcript summaries by date/tags
- Can access full transcript chunks if specific details needed
- Compressed summaries fit in context easily

## Component Design

### 1. Smart File Reading Tools

#### 1.1 `file_preview` Tool
**Purpose**: Quick overview without consuming context  
**Implementation**: `src/plugins/file_tools/file_tools.c`

```c
int ethervox_file_preview(
    ethervox_file_tools_config_t* config,
    const char* file_path,
    ethervox_file_preview_result_t* result
);

typedef struct {
    char* first_lines;      // First 50 lines
    char* last_lines;       // Last 20 lines
    uint64_t total_size;    // Bytes
    uint32_t total_lines;   // Line count
    char file_type[32];     // .txt, .md, .c, etc.
    char encoding[32];      // UTF-8, ASCII, etc.
    char* structure_hints;  // Headers, functions, etc.
    bool is_binary;         // Binary content detected
} ethervox_file_preview_result_t;
```

**Returns**: JSON with all metadata + preview snippets (~2KB typical)  
**Binary detection**: Check for null bytes, high proportion of control chars

#### 1.2 `file_read_chunk` Tool
**Purpose**: Read specific portions of large files  
**Implementation**: `src/plugins/file_tools/file_tools.c`

```c
int ethervox_file_read_chunk(
    ethervox_file_tools_config_t* config,
    const char* file_path,
    uint32_t start_line,     // 1-indexed, 0 = from beginning
    uint32_t end_line,       // 0 = to end
    uint32_t max_chars,      // Default 6000 (~1500 tokens)
    ethervox_file_chunk_result_t* result
);

typedef struct {
    char* content;           // Chunk content
    uint32_t start_line;     // Actual start (may adjust for boundaries)
    uint32_t end_line;       // Actual end
    uint32_t total_lines;    // Total lines in file
    bool has_more_before;    // More content before this chunk
    bool has_more_after;     // More content after this chunk
    char chunk_type[32];     // "full_file", "chunk", "truncated"
} ethervox_file_chunk_result_t;
```

**Smart chunking by file type**:
- **Markdown**: Break on header boundaries (`#`, `##`, etc.)
- **Code (.c, .h)**: Break on function boundaries using simple parser
- **Logs/Transcripts**: Break on timestamp/speaker boundaries
- **Plain text**: Break on paragraph boundaries (double newline)
- **Fallback**: Hard line limits with word-boundary respect

**Function boundary protection** (C code):
- Scan for `{` and matching `}` to find function extents
- If function > max_chars: split into multiple chunks at statement boundaries (`;`)
- Add markers: `// ... (function continues, part 1/3)`

#### 1.3 Enhanced `file_read` Tool
**Purpose**: Backwards compatible full-file read with safety  
**Modifications**: `src/plugins/file_tools/file_tools.c:269-336`

**New behavior**:
```c
// Before reading full file:
1. Estimate tokens: file_size / 4
2. If estimated_tokens > 1500:
   - Return error JSON instead of content
   - Suggest using file_preview + file_read_chunk
3. If file > 10MB: reject (existing behavior)
4. Detect binary content: reject with clear error
5. Read and return (existing behavior)
```

**Error response format**:
```json
{
  "error": "file_too_large",
  "file_path": "/path/to/file",
  "size_bytes": 50000,
  "size_kb": 48,
  "estimated_tokens": 12500,
  "total_lines": 1234,
  "suggestion": "File is too large for context window. Use file_preview to see structure, then file_read_chunk with line ranges.",
  "recommended_chunk_size": 200
}
```

### 2. Live Transcript Processing

#### 2.1 Live Transcript File Writing (IMPLEMENTED ✅)
**Status**: Complete and functional as of Dec 3, 2025  
**Implementation**: `src/plugins/voice_tools/voice_tools.c`

**How it works**:
1. **File created immediately on `/transcribe`**:
   - Creates file at `~/.ethervox/transcripts/transcript_YYYYMMDD_HHMMSS.txt`
   - Writes initial header with "Status: RECORDING (live updates)"
   - File path stored in `session->last_transcript_file`

2. **Real-time segment writing** in `audio_capture_thread()`:
   ```c
   // After each STT result
   if (session->last_transcript_file[0] != '\0') {
       FILE* f = fopen(session->last_transcript_file, "a");
       if (f) {
           fprintf(f, "%s ", result.text);
           fflush(f);  // Force immediate disk write
           fclose(f);
       }
   }
   ```

3. **File finalization on `/stoptranscribe`**:
   - Updates header to "Status: COMPLETE"
   - Adds final duration and segment count statistics
   - Preserves all transcript content

**Benefits**:
- LLM can read the file **during** recording using `file_read` tool
- No need for special `transcript_monitor` API - standard file reading works
- Transcript persists even if process crashes
- Human-readable format allows manual inspection during recording

**LLM workflow during live transcription**:
1. User runs `/transcribe`
2. File created at `~/.ethervox/transcripts/transcript_20251203_143000.txt`
3. **LLM can now use `file_read` tool to monitor progress**:
   ```
   User: "What have I said so far?"
   LLM: [Calls file_read on the transcript file]
   LLM: "You've discussed project timelines, mentioned API changes..."
   ```
4. Segments append in real-time as user speaks
5. User runs `/stoptranscribe` - file header updated to COMPLETE

#### 2.2 No-Summary Mode
**Command**: `/transcribe --no-summary`  
**Implementation**: `src/main.c` + `voice_tools.c`

```c
typedef struct {
    // ... existing fields ...
    bool live_summary_enabled;  // NEW: Default true
    uint32_t live_summary_interval_sec;  // NEW: Default 30
} ethervox_voice_session_t;
```

**Behavior**:
- `--no-summary`: Disables automatic LLM invocations during recording
- Still records transcript to file
- LLM only processes after `/stoptranscribe`

#### 2.3 Post-Transcription Compression
**Triggered by**: `/stoptranscribe` (automatic)  
**Implementation**: New function in `voice_tools.c`

```c
int ethervox_voice_tools_compress_transcript(
    ethervox_voice_session_t* session,
    ethervox_governor_t* governor,
    uint64_t* summary_memory_id
);
```

**Process**:
1. Read full transcript file in chunks (using `file_read_chunk`)
2. For each chunk: call Governor with compression prompt
3. Governor extracts:
   - **Tasks/Action Items**: Store with tag `["task", "transcript", date]`
   - **Decisions**: Store with tag `["decision", "transcript", date]`
   - **Technical Details**: Store with tag `["technical", topic, date]`
   - **Questions**: Store with tag `["question", "transcript", date]`
   - **Timeline**: Store with tag `["timeline", date]`
4. Generate final compressed summary
5. Store compressed summary with tag `["transcript_summary", date, "final"]`
6. Archive original transcript (keep file, mark memory entry as archived)

**Compression prompt template**:
```
You are compressing a voice transcript. Extract:
1. Action items (tasks with owners and deadlines)
2. Decisions made (what was decided and why)
3. Technical details (APIs, architectures, algorithms discussed)
4. Open questions (unanswered items)
5. Timeline (when events occurred or are scheduled)

Be concise. Use bullet points. Preserve critical details.

Transcript chunk [X of Y]:
<chunk content>
```

### 3. File-Specific Chunking Strategies

#### 3.1 Markdown Files
**Strategy**: Break on header boundaries
```c
// Scan for lines starting with # ## ### etc.
// Chunk includes header + content until next header
// Max chunk size: 6000 chars
// If section > 6000: split on paragraph boundaries
```

**Example chunk markers**:
```
━━━ Section: ## Project Overview (lines 10-45 of 200) ━━━
<content>
━━━ Continue reading: /file_read_chunk path.md 46 80 ━━━
```

#### 3.2 C/C++ Code Files
**Strategy**: Break on function boundaries with overflow protection
```c
// Parse for function definitions:
//   - return_type function_name(...) {
// Track brace depth to find function end
// If function > 6000 chars:
//   - Split at statement boundaries (semicolons)
//   - Preserve function signature in each chunk
//   - Add continuation markers
```

**Overflow protection**:
```c
// For 10,000 char function split into 2 chunks:

// Chunk 1:
int huge_function(args) {
    // ... first 5500 chars ...
    // ... (continued in next chunk - part 1/2)
}

// Chunk 2:
// ... (continuation of huge_function - part 2/2)
int huge_function(args) {  // (signature reminder)
    // ... (previous code omitted)
    // ... remaining 4500 chars ...
}
```

#### 3.3 Transcript Files
**Strategy**: Break on speaker change boundaries
```c
// Look for "[Speaker N, timestamp]" markers
// Chunk includes continuous speaker segments
// Max chunk: 6000 chars or 50 speaker turns
// Preserve timestamp context
```

#### 3.4 Log Files
**Strategy**: Break on timestamp boundaries
```c
// Detect timestamp formats: [YYYY-MM-DD HH:MM:SS], ISO8601, etc.
// Chunk on time boundaries (e.g., 1-minute chunks)
// Preserve timestamp in chunk metadata
```

### 4. Memory Integration

#### 4.1 File Metadata Storage
**When**: Any file first accessed (preview, read, or chunk)  
**Storage**: Memory entry with tags `["file_metadata", filename]`

```json
{
  "path": "/full/path/to/file.txt",
  "size_bytes": 50000,
  "line_count": 1234,
  "file_type": "txt",
  "last_modified": 1733270400,
  "last_accessed": 1733270500,
  "has_summary": false,
  "summary_memory_id": null,
  "encoding": "UTF-8",
  "is_binary": false
}
```

#### 4.2 Transcript Summary Storage
**Structure**: Hierarchical memory entries

```
transcript_summary (parent)
├── task (child, importance 0.9)
├── task (child, importance 0.9)
├── decision (child, importance 0.8)
├── technical (child, importance 0.7)
└── timeline (child, importance 0.6)
```

**Tags**: Each entry tagged with date, type, and relevant keywords

### 5. User Interface

#### 5.1 Command: `/transcribe`
**Syntax**: `/transcribe [--no-summary] [--interval N]`

**Options**:
- `--no-summary`: Disable live LLM processing
- `--interval N`: Set live summary interval in seconds (default 30)

**Examples**:
```
/transcribe                    # Start with live summaries (30sec interval)
/transcribe --no-summary       # Record only, process after
/transcribe --interval 60      # Live summaries every 60 seconds
```

#### 5.2 Command: `/stoptranscribe`
**Enhanced behavior**:
```
/stoptranscribe [--no-compress]
```

**Process**:
1. Stop recording (existing)
2. Save transcript file (existing)
3. **NEW**: If not `--no-compress`, automatically trigger compression:
   - Show progress: "🔄 Compressing transcript (chunk 1/5)..."
   - Extract tasks, decisions, technical details
   - Generate final summary
   - Display summary with memory IDs
4. Display file path and usage instructions (existing)

**Output**:
```
📝 Transcript saved to: ~/.ethervox/transcripts/transcript_20251203_143022.txt

🔄 Compressing transcript (this may take 30-60 seconds)...
   [█████████████░░░░░░░] 65% - Processing chunk 3/5

✅ Compression complete!

📊 Extracted:
   • 5 tasks (memory IDs: 1001-1005)
   • 3 decisions (memory IDs: 1006-1008)
   • 7 technical details (memory IDs: 1009-1015)
   • 2 open questions (memory IDs: 1016-1017)

💾 Final summary saved (memory ID: 1018)

💡 Try:
   /search task           - Find extracted tasks
   /recall 1018          - Read full summary
   /read <transcript>    - Access original transcript
```

#### 5.3 Command: `/files`
**Purpose**: Manage accessed files and summaries  
**Implementation**: `src/main.c`

**Syntax**: 
```
/files [list|clear|info <path>]
```

**Examples**:
```
/files                          # List recently accessed files
/files info ~/Notes/meeting.md  # Show file metadata
/files clear                    # Clear file cache
```

**Output**:
```
📁 Recently Accessed Files:

1. ~/.ethervox/transcripts/transcript_20251203_143022.txt
   Size: 48 KB | Lines: 1,234 | Modified: 2 hours ago
   Summary: ✅ (memory ID: 1018)

2. ~/Documents/project_spec.md
   Size: 125 KB | Lines: 3,456 | Modified: 1 day ago
   Summary: ❌ (too large, use /file_read_chunk)

3. ~/Code/ethervox/main.c
   Size: 68 KB | Lines: 1,802 | Modified: 3 hours ago
   Summary: ❌

💡 Use /search file_metadata to find all accessed files
```

## Implementation Plan

### Phase 1: Binary Detection & Enhanced file_read ✅ COMPLETE
**Status**: Implemented and tested Dec 3, 2025  
**Files changed**: `file_core.c`, `file_registry.c`

**Completed**:
1. ✅ Added `is_binary_content()` function - detects null bytes and control char ratios
2. ✅ Enhanced `file_read` to return error code -2 for binary files
3. ✅ Token estimation in `tool_file_read_wrapper()` - estimates before reading (1 token ≈ 4 chars)
4. ✅ Helpful error JSON for oversized files (>1500 tokens):
   ```json
   {
     "error": "file_too_large",
     "size_bytes": 50000,
     "estimated_tokens": 12500,
     "total_lines": 1234,
     "suggestion": "Use file_preview to see structure, then file_read_chunk with line ranges."
   }
   ```
5. ✅ Binary file error JSON with clear messaging
6. ✅ Build verified - compiles successfully

### Phase 1.5: Live Transcript File Writing ✅ COMPLETE
**Status**: Implemented and tested Dec 3, 2025  
**Files changed**: `voice_tools.c`, `main.c`

**Completed**:
1. ✅ Transcript file created immediately on `/transcribe` start
2. ✅ Real-time segment appending with `fflush()` for immediate writes
3. ✅ File header updates on `/stoptranscribe` (RECORDING → COMPLETE)
4. ✅ LLM can monitor live transcripts using standard `file_read` tool
5. ✅ User messaging updated to indicate live file updates
6. ✅ Build verified - compiles successfully

**Next**: Continue with Phase 2 (file_preview tool)

### Phase 2: file_preview Tool (3-4 hours)
1. Add `ethervox_file_preview_result_t` struct to `file_tools.h`
2. Implement `ethervox_file_preview()` in `file_tools.c`
3. Add structure detection (markdown headers, C functions)
4. Implement `tool_file_preview_wrapper()` with JSON output
5. Register with Governor
6. Build and test with various file types

### Phase 3: file_read_chunk Tool (4-5 hours)
1. Add `ethervox_file_chunk_result_t` struct to `file_tools.h`
2. Implement smart chunking strategies:
   - Markdown header boundaries
   - C function boundaries with overflow protection
   - Transcript speaker boundaries
   - Log timestamp boundaries
3. Implement `ethervox_file_read_chunk()` in `file_tools.c`
4. Add `tool_file_read_chunk_wrapper()` with JSON output
5. Register with Governor
6. Build and test with edge cases (huge functions, nested code)

### Phase 4: Live Transcript Monitoring (3-4 hours)
1. Add `transcript_monitor` tool to `voice_tools.c`
2. Modify `ethervox_voice_session_t` to track segment state
3. Implement `ethervox_voice_tools_get_live_transcript()`
4. Add automatic Governor invocation during `/transcribe`
5. Implement `--no-summary` flag parsing in `main.c`
6. Build and test live monitoring

### Phase 5: Post-Transcription Compression (4-5 hours)
1. Implement `ethervox_voice_tools_compress_transcript()` in `voice_tools.c`
2. Add compression prompt templates
3. Integrate with Governor tool invocation
4. Implement hierarchical memory storage (tasks, decisions, etc.)
5. Add progress indicators during compression
**File Changes Summary**

**New files**:
- `docs/live-transcript-processing-strategy.md` (this document)

**Modified files (Phase 1 & 1.5 - COMPLETE)**:
- ✅ `src/plugins/file_tools/file_core.c` - Added `is_binary_content()`, enhanced file_read
- ✅ `src/plugins/file_tools/file_registry.c` - Token estimation, helpful error JSONs
- ✅ `src/plugins/voice_tools/voice_tools.c` - Live file writing, real-time appending
- ✅ `src/main.c` - Updated `/stoptranscribe` messaging, fixed `/transcribe` command

**Pending modifications**:
- `include/ethervox/file_tools.h` - Add new structs for preview/chunk tools
- `include/ethervox/voice_tools.h` - Add compression-related fields
- `src/main.c` - Add `/files` command

**Current status**: Phase 1 & 1.5 complete (~300 lines added)  
**Estimated remaining**: ~1200-1700 lines for Phases 2-7igate
3. Test edge cases: binary files, huge functions, empty files
4. Test memory integration: retrieval, search, persistence
5. Performance testing: 100KB transcript, 1MB code file

## Success Criteria

1. **Binary safety**: System rejects binary files before consuming context
2. **Large file handling**: Can navigate 10MB files via chunking without context overflow
3. **Live transcript processing**: LLM extracts key info during recording
4. **Post-processing quality**: Compressed summaries preserve critical details
5. **Memory integration**: All extracted data searchable and retrievable
6. **User experience**: Clear progress indicators, helpful error messages
7. **Code chunking**: 5000+ line functions split intelligently without breaking logic
8. **Performance**: Compression of 50KB transcript completes in < 60 seconds

## File Changes Summary

**New files**:
- (this document)

**Modified files**:
- `include/ethervox/file_tools.h` - Add new structs and function declarations
- `src/plugins/file_tools/file_tools.c` - Implement new tools
- `include/ethervox/voice_tools.h` - Add live monitoring fields
- `src/plugins/voice_tools/voice_tools.c` - Add monitoring and compression
- `src/main.c` - Add `/files` command, modify `/transcribe` and `/stoptranscribe`

**Estimated total**: ~1500-2000 lines of new code

## Notes

- All chunking strategies include "continuation markers" to help LLM navigate
- Memory tagging strategy enables precise retrieval (e.g., "find tasks from yesterday")
- Binary detection prevents garbage input to LLM
- Progress indicators keep user informed during long operations
- Hierarchical memory structure (parent summary + child details) enables both quick overview and deep dive
