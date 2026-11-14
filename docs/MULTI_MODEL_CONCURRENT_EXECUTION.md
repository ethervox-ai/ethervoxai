# Multi-Model Concurrent Execution Architecture

## Overview

The ethervox_core backend supports loading and running multiple models simultaneously through a refcounted backend and per-model synchronization.

## Key Components

### 1. Global Backend (Refcounted)

**Location:** `src/llm/model_pool.c`

```c
static struct {
    mutex_t mutex;
    int refcount;
    bool initialized;
} g_backend = {0};
```

- **One `llama_backend_init()` per process** - called once, reference counted
- Each pool creation increments refcount
- Backend freed only when last pool is destroyed
- Thread-safe via global mutex

### 2. Model Pool

**Location:** `include/ethervox/model_pool.h`, `src/llm/model_pool.c`

```c
struct ethervox_model_pool {
    ethervox_paths_t paths;
    uint64_t budget_bytes;          // Total memory budget
    uint64_t used_bytes;            // Currently used
    mutex_t pool_mutex;             // Protects pool state
    ethervox_model_handle_t* models; // Linked list of loaded models
};
```

- Memory budget enforcement (file size + KV cache + overhead)
- `would_fit()` checks before loading
- Pool mutex protects model list and memory accounting
- Multiple models stored in linked list

### 3. Model Handles (Per-Model Locking)

```c
struct ethervox_model_handle {
    struct llama_model* model;
    struct llama_context* ctx;
    void* mtmd_ctx;              // For multimodal (vision/audio)
    mutex_t inference_mutex;     // Serializes inference on THIS model
    uint64_t memory_bytes;
    char role[64];               // "governor", "docling", "embed", etc.
    struct ethervox_model_handle* next;
};
```

- **Per-model inference_mutex**: Different models run concurrently
- Same model: inference serialized (one request at a time)
- Role field identifies purpose ("governor", "docling", "embed")

## Concurrent Execution Model

### Scenario: Governor + Granite-Docling Running Together

```
Process Memory Budget: 3 GB
├─ Global llama backend (initialized once, refcount=1)
├─ Model 1: granite-4.0-h-1b (governor, 1.5 GB)
│   └─ inference_mutex₁ → Thread A can run inference
└─ Model 2: granite-docling-258M (vision, 515 MB)
    └─ inference_mutex₂ → Thread B can run inference concurrently
```

**Flow:**

1. **Pool Creation**
   ```c
   ethervox_model_pool_create(&paths, 3GB, &pool);
   // → backend_init() called, g_backend.refcount = 1
   ```

2. **Load Governor Model**
   ```c
   config1 = {.model_path = "granite-4.0-h-1b.gguf", 
              .role = "governor", .context_size = 4096};
   ethervox_model_pool_load(pool, &config1, NULL, NULL, &governor_handle);
   // → pool->used_bytes += 1.5GB
   // → governor_handle->inference_mutex initialized
   ```

3. **Load Docling Model**
   ```c
   config2 = {.model_path = "granite-docling-258M/model.safetensors",
              .role = "docling", .context_size = 4096};
   ethervox_model_pool_load(pool, &config2, NULL, NULL, &docling_handle);
   // → would_fit() checks: 1.5GB + 515MB < 3GB ✓
   // → pool->used_bytes += 515MB
   // → docling_handle->inference_mutex initialized
   ```

4. **Concurrent Inference** (when session API is added)
   ```c
   // Thread A: Governor conversation
   MUTEX_LOCK(governor_handle->inference_mutex);
   llama_decode(governor_handle->ctx, ...);  // Chat inference
   MUTEX_UNLOCK(governor_handle->inference_mutex);
   
   // Thread B: Document processing (concurrent!)
   MUTEX_LOCK(docling_handle->inference_mutex);
   llama_decode(docling_handle->ctx, ...);   // Vision inference
   MUTEX_UNLOCK(docling_handle->inference_mutex);
   ```

## Memory Budget Enforcement

**Current Implementation (C3.1):**
- **Static budget**: Set at pool creation, enforced via `would_fit()` before loading
- **No automatic eviction**: Models stay loaded until explicitly unloaded
- **OOM prevention**: Accurate estimation prevents loading more than budget allows

**Estimation Formula** (from `estimate_memory()`):
```c
required = file_size + (context_size × 2048) + 128MB
           ↓              ↓                      ↓
        model weights   KV cache (F16)      overhead
```

**Example:**
- granite-docling-258M (safetensors): 515 MB
- Context 4096 tokens: 4096 × 2048 = 8 MB
- Overhead: 128 MB
- **Total: ~651 MB**

**Pending (C3.4 - Memory Pressure & LRU Eviction):**
- **Dynamic pressure callbacks**: `ethervox_model_pool_set_pressure_callback()`
- **LRU eviction**: `ethervox_model_pool_evict_lru(bytes, protected_roles)`
- **OS-driven**: Host responds to system memory warnings
- **Graceful degradation**: Handles land in `unloaded` state, never dangling
- **Protected roles**: Keep critical models (e.g., "governor") loaded during pressure

**Spec (§12):**
> "Host drives from OS pressure signals; handles land in a defined `unloaded` state, never dangling"

**Future behavior:**
```c
// Host receives OS memory warning
on_memory_pressure(MEMORY_PRESSURE_WARNING) {
    // Evict least-recently-used models, protect governor
    ethervox_model_pool_evict_lru(pool, 
        500 * 1024 * 1024,  // Free 500 MB
        (const char*[]){"governor"},  // Keep this role loaded
        1);
}

// Handles remain valid but in unloaded state
result = ethervox_session_generate(session_using_evicted_model, ...);
// → Returns ETHERVOX_ERROR_MODEL_UNLOADED (graceful failure)
```

## Multimodal Support (Vision/Audio)

**Location:** `src/llm/media.c`

```c
// Model handle tracks optional mtmd context
handle->mtmd_ctx = mtmd_init_from_file(mmproj_path, model, params);

// Vision capabilities query
ethervox_model_capabilities(docling_handle, &caps);
// → caps.supports_vision = true
// → caps.supports_audio = false (docling is vision-only)
```

- **Granite-Docling**: Unified VLM, no separate mmproj (vision built-in)
- **Granite Speech**: Requires mmproj for audio encoder
- Capabilities queried per model handle

## Usage Pattern for Document Processing

**Current (C3.1 - Static Budget):**
```c
// 1. Create pool with budget for both models
ethervox_model_pool_create(&paths, 3GB, &pool);

// 2. Load governor for chat
ethervox_model_pool_load(pool, &governor_config, NULL, NULL, &gov);

// 3. Load docling for document understanding
ethervox_model_pool_load(pool, &docling_config, NULL, NULL, &doc);
// → Fails if budget exceeded

// 4. Query capabilities
ethervox_model_capabilities(doc, &caps);
assert(caps.supports_vision);  // ✓

// 5. Process document page (concurrent with governor chat)
ethervox_media_t page = {
    .kind = ETHERVOX_MEDIA_IMAGE_PNG,
    .data = page_render,
    .data_size = render_size
};
// → When session API is added, this would run concurrently

// 6. Cleanup (automatic refcount management)
ethervox_model_pool_destroy(pool);
// → Unloads both models
// → Decrements g_backend.refcount
// → Calls llama_backend_free() when last pool is destroyed
```

**Future (C3.4 - Dynamic Pressure Management):**
```c
// Host registers pressure callback
void on_pressure(ethervox_memory_pressure_level_t level, void* ud) {
    ethervox_model_pool_t* pool = (ethervox_model_pool_t*)ud;
    
    if (level == ETHERVOX_PRESSURE_WARNING) {
        // Evict LRU models, protect governor
        ethervox_model_pool_evict_lru(pool, 
            500 * 1024 * 1024,              // Free 500 MB
            (const char*[]){"governor"},    // Protected roles
            1);
        // → Docling evicted if least recently used
        // → Handle remains valid but model unloaded
    }
}

ethervox_model_pool_set_pressure_callback(pool, on_pressure, pool);

// Later: App can reload on-demand
if (docling_needed && !is_loaded(doc_handle)) {
    ethervox_model_pool_reload(pool, doc_handle);
}
```

## Governor vs Model Pool

**Current State:**
- **Governor API** (`governor.c`): Single-model, blocking, global init flag (no refcount)
- **Model Pool API** (`model_pool.c`): Multi-model, properly refcounted, concurrent-ready

**Spec (§11):**
> "The existing single-model governor API stays, reimplemented on the pool."

**Future:** Governor will be refactored to use model pool internally, maintaining backward compatibility.

## Thread Safety Summary

| Component | Mutex | Protects | Concurrent Access |
|-----------|-------|----------|-------------------|
| Global backend | `g_backend.mutex` | Init/cleanup refcount | ✓ Thread-safe |
| Model pool | `pool->pool_mutex` | Model list, memory accounting | ✓ Thread-safe |
| Model handle | `handle->inference_mutex` | Inference on that model | ⚠️ Same model serialized, different models concurrent |

## Current Implementation Status (C3.1)

✅ **Complete:**
- Global refcounted backend
- Memory budget enforcement (static)
- Per-model handle allocation
- Multimodal (mtmd) integration
- Capability queries

⏳ **Pending (C3.2 - Session Forking):**
- Session API that uses inference_mutex
- Actual concurrent inference execution
- Session-level media attachment
- Batch throughput optimization (≥4× via KV cache reuse)

⏳ **Pending (C3.4 - Memory Pressure & LRU):**
- Dynamic memory pressure callbacks
- LRU eviction with role protection
- Graceful model unloading (handles stay valid in unloaded state)
- OS-driven memory management

## Testing

**Unit Test:** `tests/unit/test_media.c` - Media API validation
**Integration Test:** `tests/integration/test_docling_integration.c` - Granite-Docling loading

**Acceptance Test (C3.1):**
> "Drive `granite-docling-258M` on a page render and receive parseable DocTags."

Status: Infrastructure ready, full inference integration requires C3.2 (sessions).
