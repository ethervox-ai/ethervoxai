# Memory Tools Plugin for EthervoxAI Governor

A structured conversation memory system that allows the Governor LLM to maintain long-term context and efficiently retrieve relevant information from past interactions.

## Overview

The Memory Tools plugin provides the Governor with the ability to:
- **Store** conversation turns with tags and importance scores
- **Search** memories by semantic similarity and tag filtering
- **Summarize** recent conversation windows
- **Export/Import** sessions in JSON or Markdown format
- **Prune** old or low-importance memories

Unlike AI summarization (which compresses and loses detail), this plugin maintains **lossless storage** with **structured indexing** for fast retrieval.

## Architecture

### Core Components

```
include/ethervox/memory_tools.h          # Public API
src/plugins/memory_tools/
  ├── memory_core.c                      # Init, cleanup, storage
  ├── memory_search.c                    # Search and retrieval
  ├── memory_export.c                    # JSON/Markdown export
  └── memory_registry.c                  # Governor tool registration
```

### Data Storage

**In-memory:** Dynamic array with tag-based indexing for O(1) tag lookups

**On-disk:** Append-only JSONL format for persistence
```jsonl
{"id":0,"turn":0,"ts":1732483200,"user":true,"imp":0.80,"text":"Can you help me?","tags":["question","help"]}
{"id":1,"turn":0,"ts":1732483201,"user":false,"imp":0.80,"text":"Of course!","tags":["response","help"]}
```

## Tools Registered with Governor

### 1. `memory_store`
Save a fact or event to conversation memory.

**Parameters:**
```json
{
  "text": "Content to remember",
  "tags": ["category1", "category2"],
  "importance": 0.85,
  "is_user": true
}
```

**Returns:**
```json
{
  "success": true,
  "memory_id": 42
}
```

### 2. `memory_search`
Query memories by text similarity and tag filtering.

**Parameters:**
```json
{
  "query": "llama.cpp compilation",
  "tag_filter": ["build", "error"],
  "limit": 10
}
```

**Returns:**
```json
{
  "results": [
    {
      "text": "The build is failing with C++ header errors",
      "relevance": 0.89,
      "importance": 0.95
    }
  ],
  "count": 1
}
```

### 3. `memory_summarize`
Generate summary of recent conversation.

**Parameters:**
```json
{
  "window_size": 10,
  "focus_topic": "llama.cpp"
}
```

**Returns:**
```json
{
  "summary": "Conversation summary (last 10 turns):\n- User: Can you help...\n- Assistant: I'll help..."
}
```

### 4. `memory_export`
Export conversation to file.

**Parameters:**
```json
{
  "filepath": "/path/to/export.json",
  "format": "json"
}
```

**Formats:** `"json"` or `"markdown"`

**Returns:**
```json
{
  "success": true,
  "bytes_written": 4096
}
```

### 5. `memory_forget`
Prune old or low-importance memories.

**Parameters:**
```json
{
  "older_than_seconds": 86400,
  "importance_threshold": 0.3
}
```

**Returns:**
```json
{
  "success": true,
  "items_pruned": 15
}
```

## C API Usage

### Basic Example

```c
#include "ethervox/memory_tools.h"
#include "ethervox/governor.h"

// Initialize memory store
ethervox_memory_store_t memory;
ethervox_memory_init(&memory, NULL, "./memory_data");

// Store a memory
const char* tags[] = {"setup", "macos"};
uint64_t memory_id;
ethervox_memory_store_add(&memory, 
                         "Installed cmake via Homebrew",
                         tags, 2,
                         0.8f,   // importance
                         true,   // is_user_message
                         &memory_id);

// Search memories
ethervox_memory_search_result_t* results;
uint32_t result_count;
ethervox_memory_search(&memory, "cmake installation",
                      NULL, 0, 10,
                      &results, &result_count);

for (uint32_t i = 0; i < result_count; i++) {
    printf("[%.2f] %s\n", results[i].relevance, results[i].entry.text);
}
free(results);

// Export session
uint64_t bytes;
ethervox_memory_export(&memory, "session.json", "json", &bytes);

// Cleanup
ethervox_memory_cleanup(&memory);
```

### Integration with Governor

```c
#include "ethervox/memory_tools.h"
#include "ethervox/governor.h"

// Initialize memory and governor
ethervox_memory_store_t memory;
ethervox_memory_init(&memory, NULL, "./memory");

ethervox_tool_registry_t registry;
ethervox_tool_registry_init(&registry);

// Register memory tools
ethervox_memory_tools_register(&registry, &memory);

// Now Governor can call memory_store, memory_search, etc.
```

## Running the Example

```bash
# Build the project
cmake -B build -DWITH_LLAMA=ON
cmake --build build

# Run memory example
./build/examples/memory_example

# Check outputs
cat memory_data/session_*.jsonl        # Append-only log
cat conversation_export.json           # Full export
cat conversation_export.md             # Human-readable
```

## Configuration

### Memory Limits

Defined in `include/ethervox/memory_tools.h`:

```c
#define ETHERVOX_MEMORY_MAX_TEXT_LEN 8192      // Max text per entry
#define ETHERVOX_MEMORY_MAX_TAGS 16            // Max tags per entry
#define ETHERVOX_MEMORY_MAX_ENTRIES 10000      // Max entries in memory
#define ETHERVOX_MEMORY_INDEX_BUCKETS 256      // Tag index size
```

### Storage Location

By default, stores to `/tmp/session_*.jsonl`. Specify custom directory:

```c
ethervox_memory_init(&memory, "my-session-id", "/var/ethervox/sessions");
```

## Performance Characteristics

| Operation | Complexity | Typical Time |
|-----------|-----------|--------------|
| Store | O(1) + disk append | ~5ms |
| Search (tag-based) | O(n * t) where t=tags | ~10ms |
| Search (text similarity) | O(n * m) where m=words | ~20ms |
| Export | O(n) | ~50ms |
| Forget | O(n) | ~15ms |

## Comparison: Memory Plugin vs AI Summarization

| Feature | AI Summary | Memory Plugin |
|---------|-----------|---------------|
| Storage | Lossy compression | Lossless storage |
| Retrieval | Re-read summary | Query by tag/similarity |
| Persistence | Ephemeral | Persistent across sessions |
| Access Pattern | Linear scan | Indexed lookup |
| LLM Integration | Passive (context) | Active (tool calls) |
| Cost | Token overhead | Disk space |

## Future Enhancements

- [ ] Semantic embeddings for better similarity search
- [ ] PostgreSQL/SQLite backend option
- [ ] Automatic importance scoring based on LLM confidence
- [ ] Cross-session memory sharing
- [ ] Memory consolidation (merge similar memories)
- [ ] Vector search integration (FAISS, Annoy)

## License

Copyright (c) 2024-2025 EthervoxAI Team  
SPDX-License-Identifier: CC-BY-NC-SA-4.0

This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
