# Memory Tools - Android Integration Guide

## Overview

The Memory Tools plugin is fully compatible with Android. The core C implementation uses standard POSIX APIs that work on Android, but requires platform-specific storage paths.

## Android-Specific Considerations

### 1. Storage Paths

**Important:** Do NOT use `/tmp` on Android - it doesn't exist. Instead, use the app's internal storage directory.

#### From Java/Kotlin:
```java
// Get app's internal files directory
File memoryDir = new File(context.getFilesDir(), "memory");
if (!memoryDir.exists()) {
    memoryDir.mkdirs();
}

String storagePath = memoryDir.getAbsolutePath();
```

#### Pass to JNI:
```java
public native void initMemory(String storagePath);
```

#### In JNI C code:
```c
JNIEXPORT void JNICALL
Java_com_ethervoxai_MainActivity_initMemory(JNIEnv *env, jobject thiz, jstring storage_path) {
    const char *path = (*env)->GetStringUTFChars(env, storage_path, NULL);
    
    ethervox_memory_store_t *store = malloc(sizeof(ethervox_memory_store_t));
    ethervox_memory_init(store, NULL, path);
    
    (*env)->ReleaseStringUTFChars(env, storage_path, path);
}
```

### 2. Permissions

Android 6.0+ (API 23+) requires runtime permissions for external storage. However, **internal storage** (used above) does NOT require permissions.

**Recommended:** Use internal storage only:
- ✅ No permissions needed
- ✅ Automatically cleaned up when app is uninstalled
- ✅ Private to your app
- ❌ Not accessible to user or other apps

**If you need external storage:**
```xml
<!-- AndroidManifest.xml -->
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
```

### 3. Memory-Only Mode

If you don't want file persistence (testing, privacy, etc.), pass `NULL` as the storage directory:

```c
// No persistence - keeps everything in RAM only
ethervox_memory_init(store, NULL, NULL);
```

This is useful for:
- Testing
- Privacy-focused apps
- Reducing storage usage
- Embedded devices with limited flash

### 4. Build Configuration

The memory plugin is automatically included when building for Android. Ensure your `CMakeLists.txt` includes:

```cmake
if(ANDROID OR TARGET_PLATFORM STREQUAL "ANDROID")
    # Memory tools are included in ETHERVOXAI_CORE_SOURCES
    # No additional configuration needed
endif()
```

### 5. Governor Integration on Android

To register memory tools with the Governor on Android:

```c
// In your Android JNI initialization
#include "ethervox/governor.h"
#include "ethervox/memory_tools.h"

void init_governor_with_memory(const char* storage_path) {
    // Initialize Governor
    ethervox_governor_t governor;
    ethervox_governor_init(&governor);
    
    // Initialize memory store
    ethervox_memory_store_t memory;
    ethervox_memory_init(&memory, NULL, storage_path);
    
    // Register memory tools
    ethervox_memory_tools_register(&governor, &memory);
    
    // Now Governor can use memory_store, memory_search, etc.
}
```

## Example Android App Integration

### MainActivity.java
```java
public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("ethervoxai");
    }
    
    private native long initEthervox(String memoryPath);
    private native void storeMemory(long handle, String text, String[] tags);
    private native String searchMemory(long handle, String query);
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Set up memory storage directory
        File memoryDir = new File(getFilesDir(), "memory");
        memoryDir.mkdirs();
        
        long handle = initEthervox(memoryDir.getAbsolutePath());
    }
}
```

### native-lib.c
```c
#include <jni.h>
#include "ethervox/memory_tools.h"

static ethervox_memory_store_t g_memory_store;

JNIEXPORT jlong JNICALL
Java_com_ethervoxai_MainActivity_initEthervox(
    JNIEnv *env, jobject thiz, jstring memory_path) {
    
    const char *path = (*env)->GetStringUTFChars(env, memory_path, NULL);
    
    if (ethervox_memory_init(&g_memory_store, NULL, path) != 0) {
        (*env)->ReleaseStringUTFChars(env, memory_path, path);
        return 0;
    }
    
    (*env)->ReleaseStringUTFChars(env, memory_path, path);
    return (jlong)&g_memory_store;
}

JNIEXPORT void JNICALL
Java_com_ethervoxai_MainActivity_storeMemory(
    JNIEnv *env, jobject thiz, jlong handle, 
    jstring text, jobjectArray tags) {
    
    ethervox_memory_store_t *store = (ethervox_memory_store_t*)handle;
    
    const char *text_str = (*env)->GetStringUTFChars(env, text, NULL);
    
    // Convert Java String[] to C char*[]
    int tag_count = (*env)->GetArrayLength(env, tags);
    const char **tag_array = malloc(tag_count * sizeof(char*));
    
    for (int i = 0; i < tag_count; i++) {
        jstring tag = (*env)->GetObjectArrayElement(env, tags, i);
        tag_array[i] = (*env)->GetStringUTFChars(env, tag, NULL);
    }
    
    uint64_t id;
    ethervox_memory_store_add(store, text_str, tag_array, tag_count, 
                              0.8f, true, &id);
    
    // Clean up
    for (int i = 0; i < tag_count; i++) {
        jstring tag = (*env)->GetObjectArrayElement(env, tags, i);
        (*env)->ReleaseStringUTFChars(env, tag, tag_array[i]);
    }
    free(tag_array);
    (*env)->ReleaseStringUTFChars(env, text, text_str);
}
```

## Testing on Android

### 1. Memory-Only Testing (No File I/O)
```c
// Quick test without files
ethervox_memory_init(&store, NULL, NULL);
```

### 2. Full Integration Testing
```c
// Use actual app storage
const char* test_path = "/data/data/com.ethervoxai/files/memory";
ethervox_memory_init(&store, NULL, test_path);
```

### 3. Check Logs
```bash
adb logcat | grep MEMORY
```

## Known Limitations on Android

1. **No external SD card access** - Use internal storage only for simplicity
2. **File paths must be absolute** - Relative paths won't work reliably
3. **Background restrictions** - File writes may be delayed/batched by OS
4. **Storage quotas** - Android may limit total app storage

## Best Practices

1. ✅ Always use `context.getFilesDir()` for storage paths
2. ✅ Create storage directory before calling `ethervox_memory_init()`
3. ✅ Clean up memory on app termination: `ethervox_memory_cleanup()`
4. ✅ Test memory-only mode first, then add persistence
5. ✅ Monitor storage usage - implement memory pruning if needed
6. ❌ Don't use `/tmp`, `/var`, or other Unix paths
7. ❌ Don't assume write operations are synchronous

## Performance Notes

- **File I/O**: Android uses flash storage - writes are fast but should be batched
- **Memory footprint**: Each entry is ~9KB, plan accordingly
- **Tag indexing**: O(1) lookup, efficient even on low-end devices
- **Search**: O(n) text similarity, acceptable for <10K entries

## Conclusion

The Memory Tools plugin works seamlessly on Android with proper storage path configuration. No Android-specific code changes are needed in the core implementation - just pass the correct storage directory via JNI.
