# Android JNI Architecture

**Document Created**: December 9, 2025  
**Purpose**: Explain Android initialization flow and how it mirrors desktop CLI architecture

## Overview

The Android JNI layer (`src/platform/ethervox_android_core.c`) provides Java Native Interface bindings for the EthervoxAI core library. As of December 2025, it has been refactored to use the same direct initialization path as the desktop CLI (`src/main.c`).

## Architecture Changes (December 2025)

### Before Refactoring
Android used a `dialogue_engine` wrapper that created unnecessary indirection:
```c
// OLD (dialogue_engine wrapper)
g_dialogue_engine->governor          // Indirect access
g_dialogue_engine->governor_tool_registry
g_dialogue_engine->manifest_registry
ethervox_dialogue_init(g_dialogue_engine, &config);  // Tool registration hidden inside
```

**Problems**:
- Tool registration duplicated in `dialogue_core.c`
- Android and desktop had different code paths
- Harder to add new tools (need to update 2 places)
- Extra layer of indirection for no benefit

### After Refactoring  
Android now mirrors desktop's direct initialization:
```c
// NEW (direct access, same as desktop)
g_governor           // Direct Governor instance
g_registry           // Direct tool registry
g_manifest_registry  // Direct manifest registry

// Same initialization sequence as main.c:
ethervox_tool_registry_init(&g_registry, 16);
ethervox_compute_tools_register_all(g_registry);
ethervox_memory_tools_register(g_registry, g_memory_store);
ethervox_governor_init(&g_governor, &config, g_registry);
```

**Benefits**:
- ✅ Single code path for all platforms
- ✅ Tools registered in one place
- ✅ Easier to maintain and debug
- ✅ Clearer ownership (JNI owns governor, not wrapper)

## Global State

### Core Instances
```c
static ethervox_governor_t* g_governor = NULL;          // LLM orchestrator
static ethervox_tool_registry_t* g_registry = NULL;     // Tool definitions
static tool_manifest_registry_t* g_manifest_registry = NULL;  // Binary manifest
static ethervox_memory_store_t* g_memory_store = NULL; // Conversation memory
```

### Platform-Specific
```c
static ethervox_platform_t* g_platform = NULL;          // Platform detection
static char g_android_files_dir[512] = {0};             // Android files directory
```

### Audio/STT (optional)
```c
static ethervox_audio_runtime_t* g_audio_runtime = NULL;
static ethervox_wake_runtime_t* g_wake_runtime = NULL;
static ethervox_stt_runtime_t* g_stt_runtime = NULL;
```

## Initialization Flow

### Java Side
```kotlin
// 1. Initialize platform + governor + tools
NativeLib.platformInit(filesDir.absolutePath)

// 2. Load LLM model into governor
NativeLib.loadGovernorModel(modelPath)

// 3. Ready to process queries
val response = NativeLib.processGovernorQuery(userInput)
```

### Native Side (C)

**Step 1: `platformInit()` - One-time setup**
```c
Java_com_droid_ethervox_1core_NativeLib_platformInit(JNIEnv* env, jobject thiz, jstring filesDir) {
    // 1. Store Android files directory
    strcpy(g_android_files_dir, files_dir_path);
    
    // 2. Initialize platform detection
    ethervox_platform_init(g_platform);
    
    // 3. Initialize memory store (in-memory until storage dir set)
    ethervox_memory_init(g_memory_store, NULL, NULL);
    
    // 4. Create and initialize tool registry
    ethervox_tool_registry_init(g_registry, 16);
    
    // 5. Register tools (compute, timer, memory, etc.)
    ethervox_compute_tools_register_all(g_registry);
    ethervox_memory_tools_register(g_registry, g_memory_store);
    // ... more tools
    
    // 6. Initialize Governor with tool registry
    ethervox_governor_init(&g_governor, &config, g_registry);
    
    return 0;  // Success
}
```

**Step 2: `loadGovernorModel()` - Load LLM**
```c
Java_com_droid_ethervox_1core_NativeLib_loadGovernorModel(JNIEnv* env, jobject thiz, jstring modelPath) {
    // 1. Load GGUF model into governor
    ethervox_governor_load_model(g_governor, model_path);
    
    // 2. Initialize manifest system (optimized prompts)
    ethervox_governor_setup_manifest(g_governor, model_path, &g_manifest_registry);
    
    return JNI_TRUE;
}
```

**Step 3: `processGovernorQuery()` - Execute query**
```c
Java_com_droid_ethervox_1core_NativeLib_processGovernorQuery(JNIEnv* env, jobject thiz, jstring query) {
    // Execute governor with manifest-optimized prompts
    ethervox_governor_execute(g_governor, query_text, &response, &error, &metrics, ...);
    
    return (*env)->NewStringUTF(env, response);
}
```

## Desktop vs Android Comparison

| Aspect | Desktop (main.c) | Android (JNI) | Same? |
|--------|-----------------|---------------|-------|
| **Governor init** | `ethervox_governor_init()` | `ethervox_governor_init()` | ✅ Yes |
| **Tool registration** | Direct `ethervox_*_register_all()` | Direct `ethervox_*_register_all()` | ✅ Yes |
| **Manifest setup** | `ethervox_governor_setup_manifest()` | `ethervox_governor_setup_manifest()` | ✅ Yes |
| **Model loading** | `ethervox_governor_load_model()` | `ethervox_governor_load_model()` | ✅ Yes |
| **Query execution** | `ethervox_governor_execute()` | `ethervox_governor_execute()` | ✅ Yes |
| **Memory persistence** | `~/.ethervox/memory/` | `context.getFilesDir()/memory/` | ❌ Paths differ |
| **Logging** | `printf()` | `__android_log_print()` | ❌ Platform-specific |

## Platform-Specific Differences

### File Paths
**Desktop**:
```c
const char* home = getenv("HOME");
snprintf(path, sizeof(path), "%s/.ethervox/models/", home);
```

**Android**:
```c
// Android files directory passed from Java
const char* files_dir = ethervox_android_get_files_dir();  // e.g., "/data/data/com.app/files"
snprintf(path, sizeof(path), "%s/models/", files_dir);
```

### Logging
**Desktop**:
```c
printf("Governor initialized\n");
```

**Android**:
```c
LOGI("Governor initialized");  // Maps to __android_log_print(ANDROID_LOG_INFO, ...)
```

### JNI Method Signatures
All Android bindings follow JNI conventions:
```c
JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_methodName(
    JNIEnv* env,      // JNI environment (for string conversion, etc.)
    jobject thiz,     // 'this' object (usually unused)
    jstring param     // JNI types (jstring, jint, jfloat, etc.)
) {
    // 1. Convert JNI types to C types
    const char* c_string = (*env)->GetStringUTFChars(env, param, NULL);
    
    // 2. Call C backend functions
    int result = ethervox_some_function(c_string);
    
    // 3. Clean up JNI references
    (*env)->ReleaseStringUTFChars(env, param, c_string);
    
    // 4. Return JNI type
    return result == 0 ? JNI_TRUE : JNI_FALSE;
}
```

## Key JNI Methods

### Lifecycle
- `platformInit(filesDir)` - Initialize governor + tools (called once on app start)
- `platformCleanup()` - Free all resources (called on app exit)

### Model Management
- `loadGovernorModel(modelPath)` - Load GGUF model into governor
- `unloadGovernorModel()` - Unload model to free memory (Android memory management)
- `reloadGovernorModel()` - Reload previously loaded model
- `loadGovernorModelMinimal(modelPath)` - Fast loading mode (no tools, brief prompt)

### Query Processing
- `processGovernorQuery(query)` - Synchronous query execution
- `processGovernorQueryAsync(query, callback)` - Async query with streaming

### Tool Optimization
- `optimizeToolPrompts(modelPath, optimizeNewOnly)` - Optimize tool prompts for minimal KV cache
  - `optimizeNewOnly = true`: Incremental mode - only optimize new tools (~10s for 1-2 tools)
  - `optimizeNewOnly = false`: Full mode - re-optimize all tools (~60s for 30+ tools)

### Tool Inspection
- `getToolsJSON()` - Get JSON list of registered tools
- `getModelInfo()` - Get loaded model metadata

### Memory/Privacy
- `setMemoryStorageDir(dirPath)` - Set persistence directory
- `setPrivacyMode(enabled)` - Enable/disable memory logging (secret mode)

## Manifest System Integration

Both desktop and Android use the same 4-level fallback system:

**Level 0 (Optimal)**: Optimized JSON prompts (~150 tokens)
```c
// Created via /optimize_tool_prompts command
~/.ethervox/tools/optimized/qwen2.5-3b-instruct.json
```

**Level 1 (Good)**: Binary manifest with one-liners (~500 tokens)
```c
~/.ethervox/tools/tools.bin  // Exported from tool registry
```

**Level 2 (Degraded)**: LLM-only mode (0 tokens for tools, inject on-demand)

**Level 3 (Emergency)**: Slash commands only (/help, /quit, etc.)

### Initialization
```c
// 1. Export manifest from runtime registry
ethervox_tool_registry_export_manifest(g_registry, "tools.bin");

// 2. Load binary manifest + optimized prompts (if available)
ethervox_governor_setup_manifest(g_governor, model_path, &g_manifest_registry);

// 3. Build minimal system prompt
ethervox_governor_build_system_prompt_with_manifest(g_manifest_registry, prompt, sizeof(prompt));
```

## Tool Prompt Optimization (Incremental Mode)

As of December 2025, the tool prompt optimizer supports incremental optimization to avoid re-optimizing tools that are already cached.

### JNI Signature
```c
JNIEXPORT jint JNICALL
Java_com_droid_ethervox_1core_NativeLib_optimizeToolPrompts(
    JNIEnv* env, 
    jobject thiz, 
    jstring modelPath, 
    jboolean optimizeNewOnly  // NEW: Controls incremental vs full optimization
);
```

### Kotlin Usage
```kotlin
// Recommended: Incremental mode (only optimize new tools)
val result = NativeLib.optimizeToolPrompts(modelPath, optimizeNewOnly = true)

// Force full re-optimization (rarely needed)
val result = NativeLib.optimizeToolPrompts(modelPath, optimizeNewOnly = false)
```

### Implementation Details
The function runs optimization in a background thread to avoid blocking the UI:

```c
// 1. Create thread data with optimize_new_only flag
thread_data->optimize_new_only = (optimizeNewOnly == JNI_TRUE);

// 2. Run optimizer in background thread
pthread_create(&thread, NULL, optimization_thread_func, thread_data);

// 3. Optimizer parses existing JSON cache
if (optimize_new_only) {
    parse_existing_optimizations(output_path, &existing_entries, &existing_count);
    // Only optimize tools not in existing_entries
}
```

### Performance Benefits
- **Incremental mode** (`optimizeNewOnly = true`):
  - 30 existing tools + 2 new tools → ~10 seconds (only optimize 2)
  - Time saved: ~86% reduction
  - API calls saved: ~94% reduction
  
- **Full mode** (`optimizeNewOnly = false`):
  - 32 tools → ~70 seconds (optimize all)
  - Use when: first-time optimization, cache corrupted, prompts need regeneration

### Background Thread Safety
The optimization runs asynchronously:
```kotlin
lifecycleScope.launch {
    val progressDialog = showProgressDialog("Optimizing tools...")
    
    val result = withContext(Dispatchers.IO) {
        NativeLib.optimizeToolPrompts(modelPath, optimizeNewOnly = true)
    }
    
    progressDialog.dismiss()
    handleResult(result)
}
```

### See Also
- `docs/INCREMENTAL_TOOL_OPTIMIZATION.md` - Full technical details
- `docs/JNI_SIGNATURE_UPDATE_OPTIMIZE_TOOL_PROMPTS.md` - Migration guide for Android developers

## Common Patterns

### String Conversion (Java ↔ C)
```c
// Java String → C string
const char* c_str = (*env)->GetStringUTFChars(env, java_string, NULL);
// ... use c_str ...
(*env)->ReleaseStringUTFChars(env, java_string, c_str);

// C string → Java String
jstring java_str = (*env)->NewStringUTFChars(env, c_str);
return java_str;
```

### Error Handling
```c
if (!g_governor) {
    LOGE("Governor not initialized");
    return JNI_FALSE;  // Return error to Java
}

int result = ethervox_some_function();
if (result != 0) {
    LOGE("Operation failed: %d", result);
    return NULL;  // Or JNI_FALSE for jboolean
}
```

### Threading (for async operations)
```c
pthread_t thread;
pthread_create(&thread, NULL, worker_function, thread_data);
pthread_detach(thread);  // Don't wait for completion
```

## Debugging Tips

### Enable Debug Logging
```kotlin
NativeLib.setDebugMode(true)  // Enables verbose LOGI/LOGD output
```

### Check Logcat
```bash
adb logcat | grep "EthervoxCore"
```

### Common Issues

**"Governor not initialized"**
- Call `platformInit()` before any other methods
- Check that `platformInit()` returned success

**"Model load failed"**
- Verify model file exists and is accessible
- Check model is valid GGUF format
- Ensure enough RAM (3B model needs ~2GB)

**"Tools not available"**
- Manifest initialization may have failed (graceful fallback to Level 2)
- Check `~/.ethervox/tools/` directory exists
- Run `/optimize_tool_prompts` on desktop to create Level 0 cache

## Future you: Where to Look

- **Add new JNI method**: Add to this file, follow existing patterns
- **Add new tool**: Register in `platformInit()`, same as desktop
- **Change initialization order**: Update both `platformInit()` and desktop `main.c`
- **Android-specific behavior**: Use `#ifdef ETHERVOX_PLATFORM_ANDROID`
- **File paths**: Use `ethervox_android_get_files_dir()` instead of `getenv("HOME")`

## See Also

- `src/main.c` - Desktop CLI (reference implementation)
- `docs/CODE_DUPLICATION_AUDIT.md` - Refactoring history
- `docs/INTEGRATION_GUIDE.md` - How to integrate into new platforms
- `include/ethervox/governor.h` - Governor API documentation
- `include/ethervox/tool_manifest.h` - Manifest system API
