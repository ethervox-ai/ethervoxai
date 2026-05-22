# JNI Signature Update: optimizeToolPrompts

## ⚠️ Breaking Change

The JNI function `optimizeToolPrompts` has been updated to support incremental optimization mode.

## Old Signature (Java/Kotlin)

```kotlin
external fun optimizeToolPrompts(modelPath: String): Int
```

## New Signature (Java/Kotlin)

```kotlin
external fun optimizeToolPrompts(modelPath: String, optimizeNewOnly: Boolean): Int
```

## Native Side (C)

```c
JNIEXPORT jint JNICALL
Java_com_droid_ethervox_1core_NativeLib_optimizeToolPrompts(
    JNIEnv* env, 
    jobject thiz, 
    jstring modelPath, 
    jboolean optimizeNewOnly  // NEW PARAMETER
);
```

## Parameter Details

- **`modelPath`** (String): Path to the LLM model file (unchanged)
- **`optimizeNewOnly`** (Boolean): **NEW**
  - `true`: Only optimize tools not already in the JSON cache (incremental mode) - **RECOMMENDED**
  - `false`: Re-optimize all tools (full rebuild mode)

## Migration Guide

### Update NativeLib Declaration

**Before:**
```kotlin
// In NativeLib.kt or NativeLib.java
external fun optimizeToolPrompts(modelPath: String): Int
```

**After:**
```kotlin
// In NativeLib.kt or NativeLib.java
external fun optimizeToolPrompts(modelPath: String, optimizeNewOnly: Boolean): Int
```

### Update Call Sites

**Before:**
```kotlin
val result = NativeLib.optimizeToolPrompts(modelPath)
```

**After (Recommended - Incremental):**
```kotlin
val result = NativeLib.optimizeToolPrompts(modelPath, optimizeNewOnly = true)
```

**After (Full Re-optimization):**
```kotlin
val result = NativeLib.optimizeToolPrompts(modelPath, optimizeNewOnly = false)
```

## Recommended Usage

For best user experience, use **incremental mode** (`optimizeNewOnly = true`):

```kotlin
// ✅ RECOMMENDED: Only optimize new tools
val result = NativeLib.optimizeToolPrompts(modelPath, optimizeNewOnly = true)
```

### Benefits of Incremental Mode

- **~86% faster** when adding a few tools to existing optimizations
- **~94% fewer LLM API calls** (saves resources)
- Preserves existing optimizations
- No wasted processing time

### When to Use Full Mode

Use `optimizeNewOnly = false` only when:
- First-time optimization (no existing cache)
- Tool prompts need to be regenerated
- JSON cache is corrupted

## Performance Comparison

### Scenario: 30 existing tools + 2 new tools

| Mode | Tools Optimized | Time | LLM Calls |
|------|----------------|------|-----------|
| Full (`false`) | 32 | ~70 sec | 32 |
| Incremental (`true`) | 2 | ~10 sec | 2 |

**Time Saved**: 60 seconds (86% reduction)

## UI Considerations

### Progress Dialog Text

**Incremental Mode:**
```kotlin
if (optimizeNewOnly) {
    progressDialog.message = "Optimizing new tools only..."
} else {
    progressDialog.message = "Optimizing all tools (this may take 1-2 minutes)..."
}
```

### Settings Toggle

Consider adding a setting:
```kotlin
// In SettingsActivity or similar
SwitchPreference {
    title = "Incremental Tool Optimization"
    summary = "Only optimize new tools (faster)"
    defaultValue = true
    key = "incremental_optimization"
}
```

Then use it:
```kotlin
val useIncremental = sharedPrefs.getBoolean("incremental_optimization", true)
val result = NativeLib.optimizeToolPrompts(modelPath, useIncremental)
```

## Error Handling

Return codes remain unchanged:
```kotlin
when (result) {
    0 -> {
        // Success
        showToast("Optimization complete!")
    }
    -1 -> {
        // Governor not loaded
        showError("Governor not initialized")
    }
    -2 -> {
        // Manifest registry not initialized
        showError("Tool registry not ready")
    }
    -3 -> {
        // Failed to get model path
        showError("Invalid model path")
    }
    -4 -> {
        // Thread allocation failed
        showError("Optimization failed to start")
    }
    else -> {
        // Optimization failed
        showError("Optimization failed (code: $result)")
    }
}
```

## Thread Safety

The optimization runs on a background thread (unchanged behavior):
- Does not block the UI
- Safe to show progress dialog
- Result available via callback or polling

## Related Files

- Native: `src/platform/ethervox_android_core.c`
- Header: `include/ethervox/tool_prompt_optimizer.h`
- Implementation: `src/governor/tool_prompt_optimizer_v2.c`
- Documentation: `docs/INCREMENTAL_TOOL_OPTIMIZATION.md`

## Testing Checklist

- [ ] Update `NativeLib` declaration with new parameter
- [ ] Update all call sites to pass `optimizeNewOnly` parameter
- [ ] Test incremental mode (add 1-2 tools, verify only those optimized)
- [ ] Test full mode (verify all tools re-optimized)
- [ ] Test with no existing JSON cache (should work like full mode)
- [ ] Verify UI doesn't freeze during optimization
- [ ] Check log output shows "Already optimized: X, To optimize: Y"

## Example Implementation

```kotlin
class ToolOptimizationManager(private val context: Context) {
    
    fun optimizeTools(
        modelPath: String, 
        incremental: Boolean = true,
        callback: (Int) -> Unit
    ) {
        lifecycleScope.launch {
            val progressDialog = ProgressDialog(context).apply {
                message = if (incremental) {
                    "Optimizing new tools only..."
                } else {
                    "Optimizing all tools (1-2 minutes)..."
                }
                isIndeterminate = true
                setCancelable(false)
                show()
            }
            
            try {
                val result = withContext(Dispatchers.IO) {
                    NativeLib.optimizeToolPrompts(modelPath, incremental)
                }
                
                progressDialog.dismiss()
                callback(result)
                
                if (result == 0) {
                    Toast.makeText(
                        context, 
                        "Optimization complete!", 
                        Toast.LENGTH_SHORT
                    ).show()
                } else {
                    showErrorDialog("Optimization failed: $result")
                }
            } catch (e: Exception) {
                progressDialog.dismiss()
                Log.e("ToolOptimization", "Error", e)
                showErrorDialog("Optimization error: ${e.message}")
            }
        }
    }
}
```

## Questions?

See `docs/INCREMENTAL_TOOL_OPTIMIZATION.md` for full technical details about the incremental optimization feature.
