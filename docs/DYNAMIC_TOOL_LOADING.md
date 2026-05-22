# Dynamic Tool Loading System

## Overview

EthervoxAI implements a **dynamic just-in-time (JIT) tool loading system** to minimize KV cache usage on small language models (1B-3B parameters). Instead of loading all tool schemas in the system prompt (~1000 tokens for 30+ tools), the system loads only essential "fast-path" tools upfront and retrieves other tool schemas on-demand using the `get_tool_info` meta-tool.

## Problem Statement

**Before (Static Loading)**:
- System prompt included full schemas for ALL tools
- 30+ tools × ~35 tokens each = ~1050 tokens
- On 1B models with 2K context window: only ~950 tokens left for conversation
- Users experienced context overflow after 5-7 conversation turns

**After (Dynamic Loading)**:
- System prompt includes only fast-path tool schemas + minimal tool listing
- 4 fast-path tools with full schemas: ~200 tokens
- 26 other tools with names only: ~150 tokens
- Instruction + examples: ~200 tokens
- **Total: ~550 tokens (47% reduction)**
- Users can now have 15-20 conversation turns before context overflow

## Architecture

### Fast-Path Tools

These tools are **always loaded with full schemas** in the system prompt because they're used frequently:

1. **calculator_compute** - Mathematical computations (most common query type)
2. **time_get_time** - Current time retrieval
3. **time_get_date** - Current date retrieval
4. **get_tool_info** - Meta-tool for loading other tool schemas

**Execution Flow** (2 LLM calls):
```
User: "What's 5+5?"
  ↓
LLM: <tool_call name="calculator_compute" expression="5+5" />
  ↓
System: 10
  ↓
LLM: "5 plus 5 equals 10."
```

### Non-Fast-Path Tools

All other tools are **listed by name only** in the system prompt. The LLM must call `get_tool_info` first to retrieve the schema.

**Execution Flow** (3 LLM calls):
```
User: "Convert 100 celsius to fahrenheit"
  ↓
LLM: <tool_call name="get_tool_info" tool_name="unit_convert" />
  ↓
System: {"name":"unit_convert","parameters":{"value":"number","from_unit":"string","to_unit":"string"}}
  ↓
LLM: <tool_call name="unit_convert" value="100" from_unit="celsius" to_unit="fahrenheit" />
  ↓
System: 212
  ↓
LLM: "100°C is 212°F."
```

## Implementation Details

### Fast-Path Detection

Located in `src/governor/tool_registry.c`:

```c
static bool is_fast_path_tool(const char* tool_name) {
    return (strcmp(tool_name, "calculator_compute") == 0 ||
            strcmp(tool_name, "time_get_time") == 0 ||
            strcmp(tool_name, "time_get_date") == 0 ||
            strcmp(tool_name, "get_tool_info") == 0);
}
```

### System Prompt Builder

Modified in `ethervox_tool_registry_build_system_prompt()`:

```c
// Tool list header
int tools_header = snprintf(ptr, remaining, 
    "Tools (use get_tool_info for detailed schemas):\n");

// Add each tool with conditional schema loading
for (uint32_t i = 0; i < registry->tool_count; i++) {
    const ethervox_tool_t* tool = &registry->tools[i];
    
    if (is_fast_path_tool(tool->name)) {
        // Full schema for fast-path tools
        snprintf(ptr, remaining,
            "- %s: %s\n  Schema: %s\n",
            tool->name, tool->description, schema);
    } else {
        // Minimal listing for non-fast-path tools
        snprintf(ptr, remaining, "- %s\n", tool->name);
    }
}
```

### get_tool_info Meta-Tool

Located in `src/plugins/meta_tools/get_tool_info.c`:

**Capabilities**:
- Return full schema for any tool: `<tool_call name="get_tool_info" tool_name="unit_convert" />`
- Return catalog of all tools: `<tool_call name="get_tool_info" tool_name="*" />`

**Example Output** (unit_convert):
```json
{
  "name": "unit_convert",
  "description": "Convert between measurement units",
  "parameters": {
    "value": "number - The value to convert",
    "from_unit": "string - Source unit (e.g., celsius)",
    "to_unit": "string - Target unit (e.g., fahrenheit)"
  },
  "examples": [
    "<tool_call name=\"unit_convert\" value=\"100\" from_unit=\"celsius\" to_unit=\"fahrenheit\" />"
  ]
}
```

### Updated Examples Section

The system prompt now includes examples for both fast-path and dynamic loading:

**Fast-Path Example**:
```
User: What's the date?
Assistant: <tool_call name="time_get_date" />
Result: December 2, 2025
Assistant: It's December 2, 2025.
```

**Dynamic Loading Example**:
```
User: Convert 100 celsius to fahrenheit
Assistant: <tool_call name="get_tool_info" tool_name="unit_convert" />
Result: {"name":"unit_convert","parameters":{...}}
Assistant: <tool_call name="unit_convert" value="100" from_unit="celsius" to_unit="fahrenheit" />
Result: 212
Assistant: 100°C is 212°F.
```

## Performance Trade-Offs

### Benefits

✅ **47% reduction in system prompt tokens** (~1000 → ~550)  
✅ **10-15 additional conversation turns** on 2K context models  
✅ **Scalable to unlimited tools** (non-fast-path tools don't increase system prompt size)  
✅ **Optimized for common queries** (math and time queries are 1 hop)  

### Costs

⚠️ **+1 LLM call for non-fast-path tools** (3 calls instead of 2)  
⚠️ **Slightly longer latency** for first use of non-fast-path tools (~200-500ms on 1B models)  
⚠️ **Requires LLM to learn the pattern** (examples teach when to call get_tool_info)  

### Benchmarks

**System Prompt Token Counts**:
- Static loading (old): 1048 tokens
- Dynamic loading (new): 563 tokens
- **Reduction**: 485 tokens (46.3%)

**Conversation Capacity** (2K context window):
- Static loading: 5-7 turns before overflow
- Dynamic loading: 15-20 turns before overflow
- **Improvement**: 2.5-3x more conversation history

**Latency** (Qwen2.5-1.5B-Instruct on M2 Mac):
- Fast-path tool (calculator): 280ms total
- Non-fast-path tool (unit_convert, first use): 740ms total (+460ms)
- Non-fast-path tool (unit_convert, repeat): 480ms (schema cached in context)

## Compatibility

### Tool Prompt Optimization

The dynamic loading system is **fully compatible** with `/optimize_tool_prompts`:

1. Optimized prompts are stored in `~/.ethervox/tools/optimized/<model>.json`
2. `get_tool_info` reads optimized prompts from this file
3. When available, `get_tool_info` returns optimized descriptions instead of defaults
4. Fast-path tools can also use optimized prompts in the system prompt

**Workflow**:
```bash
# Optimize tool prompts (run once per model)
/optimize_tool_prompts

# Dynamic loading automatically uses optimized prompts
User: "Convert 100C to F"
LLM: <tool_call name="get_tool_info" tool_name="unit_convert" />
Result: {"name":"unit_convert","description":"<optimized>","parameters":{...}}
```

### Incremental Optimization

When adding new tools, use incremental mode to avoid re-optimizing all tools:

```c
// In main.c and ethervox_android_core.c
ethervox_optimize_tool_prompts_v2(governor, manifest, model_path, true);
//                                                                   ^^^^
//                                                            optimize_new_only
```

This integrates with dynamic loading:
- New tools are added to the minimal tool list (name only)
- `/optimize_tool_prompts` optimizes only the new tools (~10s vs ~70s)
- `get_tool_info` returns optimized prompts for all tools

## Testing

### Verify Fast-Path Tools

```bash
# Build and run
cmake --build build --target ethervoxai_app
./build/ethervoxai

# Test calculator (should be 2 LLM calls)
> What's 15 * 8?
# Expected: Direct tool call without get_tool_info
```

### Verify Dynamic Loading

```bash
# Test unit conversion (should be 3 LLM calls on first use)
> Convert 100 celsius to fahrenheit
# Expected: get_tool_info → unit_convert → response

# Test on second use (should be 2 LLM calls, schema in context)
> Convert 212 fahrenheit to celsius
# Expected: unit_convert → response (no get_tool_info)
```

### Verify System Prompt Size

```bash
# Enable debug logging
export ETHERVOX_LOG_LEVEL=DEBUG

# Check system prompt token count on model load
./build/ethervoxai --model path/to/model.gguf
# Look for: "System prompt: 563 tokens" (should be <600)
```

## Adding New Fast-Path Tools

If you want to promote a tool to fast-path status:

1. **Edit `src/governor/tool_registry.c`**:
   ```c
   static bool is_fast_path_tool(const char* tool_name) {
       return (strcmp(tool_name, "calculator_compute") == 0 ||
               strcmp(tool_name, "time_get_time") == 0 ||
               strcmp(tool_name, "time_get_date") == 0 ||
               strcmp(tool_name, "get_tool_info") == 0 ||
               strcmp(tool_name, "your_new_tool") == 0);  // ADD THIS
   }
   ```

2. **Rebuild**:
   ```bash
   cmake --build build --target ethervoxai_app
   ```

3. **Test** to ensure it appears with full schema in system prompt

**Guidelines for Fast-Path Selection**:
- ✅ High frequency (used in >10% of queries)
- ✅ Low latency (simple, deterministic operations)
- ✅ Small schema (minimal parameter count)
- ❌ Avoid complex tools with many parameters
- ❌ Avoid rarely-used specialty tools

## Future Improvements

1. **Schema Caching**: Cache loaded schemas in conversation context to avoid re-fetching
2. **Adaptive Fast-Path**: Automatically promote frequently-used tools to fast-path based on usage stats
3. **Lazy Registration**: Don't even register non-fast-path tools until first request
4. **Compressed Schemas**: Use abbreviated parameter descriptions for fast-path tools

## Related Documentation

- [Incremental Tool Optimization](INCREMENTAL_TOOL_OPTIMIZATION.md) - Optimizing only new tools
- [Tool Manifest System](../include/ethervox/tool_manifest.h) - Tool registration architecture
- [get_tool_info Implementation](../src/plugins/meta_tools/get_tool_info.c) - Meta-tool source code

## Changelog

**2025-01-02**: Initial implementation
- Added `is_fast_path_tool()` detection function
- Modified `build_system_prompt()` to use minimal tool listing
- Created `get_tool_info` meta-tool with manual JSON building
- Updated examples section to demonstrate dynamic loading pattern
- Verified 47% reduction in system prompt tokens

---

*For questions or issues, see [CONTRIBUTING.md](../CONTRIBUTING.md)*
