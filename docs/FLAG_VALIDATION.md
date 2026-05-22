# Command-Line Flag Validation

## Overview

EthervoxAI validates command-line flag combinations to prevent incompatible or contradictory configurations. This document describes the validation rules and their rationale.

## Validation Rules

### 1. Hard Errors (Incompatible Combinations)

These combinations are **not allowed** and will cause the program to exit with an error:

#### `--minimal` + `--testllm`
```bash
./ethervoxai --minimal --testllm  # ERROR
```
**Reason**: `--minimal` disables all tools for fast loading, but `--testllm` requires tools to test.

**Error Message**:
```
Error: --minimal and --testllm are incompatible
  --minimal disables tools, but --testllm requires tools to test
  Please use one or the other.
```

#### `--minimal` + `--optimize_tool_prompts`
```bash
./ethervoxai --minimal --optimize_tool_prompts  # ERROR
```
**Reason**: `--minimal` disables all tools, but `--optimize_tool_prompts` requires tools to optimize.

**Error Message**:
```
Error: --minimal and --optimize_tool_prompts are incompatible
  --minimal disables tools, but --optimize_tool_prompts requires tools
  Please use one or the other.
```

### 2. Warnings (Unusual but Allowed)

These combinations are **allowed** but will display a warning:

#### `--minimal` + `-engineering`
```bash
./ethervoxai --minimal -engineering  # WARNING (allowed)
```
**Reason**: These flags have different philosophies:
- `--minimal`: Fast loading, tools disabled, brief system prompt
- `-engineering`: Debug mode, verbose logging, startup prompt skipped

**Warning Message**:
```
⚠️  Warning: Combining --minimal (fast mode) with -engineering (debug mode)
   Minimal mode: tools disabled, brief system prompt
   Engineering mode: verbose logging, startup prompt skipped
   Both will be applied - you'll get fast loading with debug output
```

**Behavior**: Both are applied - you get minimal loading speed with engineering debug output.

#### `--debug` + `--quiet`
```bash
./ethervoxai --debug --quiet  # WARNING (last wins)
```
**Reason**: Direct contradiction in logging level.

**Warning Message**:
```
⚠️  Warning: --debug and --quiet are contradictory
   Last flag wins: using quiet mode
```

**Behavior**: **Last flag wins** - the order matters:
- `--debug --quiet` → quiet mode (logging off)
- `--quiet --debug` → debug mode (logging on)

## Flag Compatibility Matrix

| Flag 1                    | Flag 2      | Result  | Message                           |
|---------------------------|-------------|---------|-----------------------------------|
| `--minimal`               | `--testllm` | ERROR   | Tools required for testing        |
| `--minimal`               | `--optimize_tool_prompts` | ERROR | Tools required for optimization |
| `--minimal`               | `-engineering` | WARNING | Speed + debug (both applied)   |
| `--debug`                 | `--quiet`   | WARNING | Last flag wins                    |
| `-engineering`            | `--testllm` | OK      | Compatible (both enable debug)    |
| `-engineering`            | `--optimize_tool_prompts` | OK | Compatible (both enable debug) |
| `--no-startup-prompt`     | `--startup-prompt <file>` | OK | Explicit file takes precedence |

## Implementation Details

### Error Handling Strategy

1. **Parse all flags first** - allows detecting all conflicts
2. **Check incompatible combinations** - exit with error code 1
3. **Display warnings** - inform user of unusual combinations
4. **Apply all compatible flags** - last flag wins for contradictions

### Code Location

Flag validation is implemented in `src/main.c`:
- **Flag parsing**: Lines ~1378-1440
- **Validation logic**: Lines ~1453-1475
- **Tracking variables**: `debug_flag_set`, `quiet_flag_set`

### Testing

Test all combinations:

```bash
# Hard errors
./ethervoxai --minimal --testllm                    # Should exit with error
./ethervoxai --minimal --optimize_tool_prompts      # Should exit with error

# Warnings (allowed)
./ethervoxai --minimal -engineering --noautoload    # Should warn, then run
./ethervoxai --debug --quiet --noautoload           # Should warn, quiet wins
./ethervoxai --quiet --debug --noautoload           # Should warn, debug wins

# Valid combinations
./ethervoxai --minimal --noautoload                 # Fast loading
./ethervoxai -engineering --testllm                 # Debug + LLM tests
```

## Design Rationale

### Why Allow `--minimal` + `-engineering`?

While unusual, this combination has legitimate use cases:
- **Mobile debugging**: Fast loading on mobile with debug output for troubleshooting
- **Performance profiling**: Measure minimal mode performance with verbose logging
- **Development**: Test minimal mode behavior while seeing debug information

### Why "Last Flag Wins" for `--debug`/`--quiet`?

This follows Unix conventions where:
- Flags override each other in order
- Last specification takes precedence
- Allows override scripts: `./run-script.sh --quiet --debug` can override quiet

### Why Hard Error for Tools?

`--minimal` fundamentally disables the tool system to achieve fast loading. Tool-based features like `--testllm` cannot function without tools, so we error immediately rather than silently failing later.

## Future Considerations

Potential additions:
- `--force` flag to bypass warnings (for scripts)
- Precedence rules instead of "last wins" (e.g., `--minimal` always takes precedence)
- More granular tool control (e.g., `--minimal-tools=compute,memory` to enable specific tools)

## See Also

- [MOBILE_OPTIMIZATION.md](MOBILE_OPTIMIZATION.md) - Details on `--minimal` and secret mode
- [QUICK_REFERENCE_MOBILE.md](QUICK_REFERENCE_MOBILE.md) - Quick reference for mobile features
- [README.md](../README.md) - Full command-line documentation
