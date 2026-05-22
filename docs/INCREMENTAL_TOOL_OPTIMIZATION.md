# Incremental Tool Optimization

## Overview

The tool prompt optimizer v2 now supports incremental optimization mode, which only optimizes newly added tools instead of re-optimizing all tools every time.

## Problem Statement

Previously, when running `/optimize_tool_prompts`:
- All tools would be re-optimized, even if they were already optimized
- Adding 1 new tool to a project with 30 existing tools meant ~5 minutes of re-optimization
- Wasted LLM API calls and processing time

## Solution

The optimizer now accepts a `bool optimize_new_only` parameter:
- `optimize_new_only = true`: Parse existing JSON, only optimize tools not already optimized
- `optimize_new_only = false`: Re-optimize all tools (full rebuild)

## Implementation Details

### Function Signature

```c
int ethervox_optimize_tool_prompts_v2(
    ethervox_governor_t* governor,
    const char* model_path,
    tool_manifest_registry_t* manifest_registry,
    bool optimize_new_only  // NEW PARAMETER
);
```

### Incremental Mode Logic

1. **Parse existing JSON file** (`~/.ethervox/tools/optimized/<model>.json`)
   - Extract tool names, optimized prompts, and token counts
   - Store in `optimized_tool_entry_t` array

2. **Count tools to optimize**
   - Iterate through manifest registry
   - Check if each tool exists in existing optimizations
   - Calculate: `tools_to_optimize = total - already_optimized`

3. **Early exit optimization**
   - If `tools_to_optimize == 0`, exit without running LLM
   - Print: "All tools already optimized!"

4. **Preserve existing optimizations**
   - Write existing entries to JSON file first
   - Only run LLM optimization on new tools
   - Append new optimizations after existing ones

5. **Skip logic in tool loop**
   - For each tool, check `is_tool_optimized(tool_name, existing_entries, ...)`
   - If optimized, skip with message: "↻ tool_name: already optimized, skipping"
   - Otherwise, run LLM optimization

### Example Output

```
Tool Prompt Optimization v2
═══════════════════════════════════════════════════════════════

Model: ~/.ethervox/models/governor-model.gguf

Status: Already optimized: 28, To optimize: 2

Processing tool batch 1/1 (5 tools):
  ↻ timer_create: already optimized, skipping
  ↻ timer_list: already optimized, skipping
  ✓ unit_convert: optimized (12 tokens)
  ✓ weather_query: optimized (14 tokens)
  ↻ memory_store: already optimized, skipping

✓ Optimization complete!
  Tools processed: 2/30
  Output file: ~/.ethervox/tools/optimized/governor-model.json
```

## Performance Impact

### Before (Full Re-optimization)

- **30 existing tools + 2 new tools = 32 tools**
- **Batches**: 7 batches (5 tools per batch)
- **Time**: ~70 seconds (10 sec/batch)
- **LLM Calls**: 32 optimization queries

### After (Incremental Optimization)

- **30 existing tools + 2 new tools**
- **Only optimize**: 2 new tools
- **Batches**: 1 batch (2 tools)
- **Time**: ~10 seconds
- **LLM Calls**: 2 optimization queries

**Time Saved**: 60 seconds (~86% reduction)  
**API Calls Saved**: 30 queries (~94% reduction)

## Default Behavior

All callers currently default to **incremental mode** (`optimize_new_only = true`):
- `src/main.c`: CLI `/optimize_tool_prompts` command
- `src/platform/ethervox_android_core.c`: Android optimization

This provides the best user experience:
- Fast iteration when adding new tools
- No wasted processing time
- Preserves existing optimizations

## Force Full Re-optimization

To force re-optimization of all tools (useful if prompts need updating):

1. Delete the JSON file:
   ```bash
   rm ~/.ethervox/tools/optimized/<model>.json
   ```

2. Run optimization:
   ```
   /optimize_tool_prompts
   ```

All tools will be re-optimized since no existing file exists.

## JSON Format Preserved

The output JSON format remains unchanged:
```json
{
  "version": 1,
  "model": "governor-model.gguf",
  "tools": [
    {"name": "timer_create", "optimized_prompt": "...", "token_count": 12},
    {"name": "timer_list", "optimized_prompt": "...", "token_count": 10},
    {"name": "unit_convert", "optimized_prompt": "...", "token_count": 12},
    ...
  ]
}
```

Existing optimizations appear first, followed by newly optimized tools.

## Memory Management

The implementation properly manages memory:
- Allocates `optimized_tool_entry_t` array when parsing JSON
- Frees array before function returns
- No memory leaks

## Future Enhancements

Potential improvements:
1. **Command-line flag**: `/optimize_tool_prompts --all` to force full re-optimization
2. **Checksum validation**: Detect if tool schema changed and re-optimize automatically
3. **Batch size optimization**: Adjust batch size based on tools_to_optimize count
4. **Progress percentage**: Show "2/30 tools need optimization (7%)"

## Testing

To test incremental optimization:

1. **Initial optimization** (30 tools):
   ```
   /optimize_tool_prompts
   ```
   Output: "To optimize: 30"

2. **Add 2 new tools** (e.g., `unit_conversion`, `weather`)

3. **Re-run optimization**:
   ```
   /optimize_tool_prompts
   ```
   Output: "Already optimized: 30, To optimize: 2"

4. **Verify**:
   - Only 2 tools should be optimized (~10 seconds)
   - JSON file should contain all 32 tools
   - Existing 30 tools preserve original prompts/token_counts

## Related Files

- `src/governor/tool_prompt_optimizer_v2.c`: Implementation
- `include/ethervox/tool_prompt_optimizer.h`: Function declaration
- `src/main.c`: CLI caller (2 locations)
- `src/platform/ethervox_android_core.c`: Android caller

## See Also

- `docs/ADDING_NEW_LLM_TOOL.md`: How to add new tools (Step 6: Optimization)
- `GOVERNOR_IMPLEMENTATION.md`: Tool manifest system architecture
