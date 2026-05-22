/**
 * @file device_profile.c
 * @brief Device capability detection and performance profiling
 * 
 * Automatically detects hardware capabilities and selects optimal
 * settings for batch size, threads, KV cache type, etc.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include "ethervox/device_profile.h"
#include "ethervox/logging.h"
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#include <sys/sysinfo.h>
#endif

#include <unistd.h>

typedef enum {
  DEVICE_TIER_LOW,      // Budget devices: <4GB RAM, <4 cores
  DEVICE_TIER_MEDIUM,   // Mid-range: 4-6GB RAM, 4-6 cores
  DEVICE_TIER_HIGH,     // Flagship: 6-8GB RAM, 6-8 cores
  DEVICE_TIER_ULTRA     // Premium: >8GB RAM, >8 cores
} device_tier_t;

typedef struct {
  int cpu_cores;
  long total_ram_mb;
  long available_ram_mb;
  bool has_neon;
  bool has_gpu;
  device_tier_t tier;
  char soc_name[64];
} device_capabilities_t;

static device_capabilities_t g_device_caps = {0};
static bool g_caps_initialized = false;

/**
 * Detect number of CPU cores available to the process
 */
static int detect_cpu_cores(void) {
#ifdef _SC_NPROCESSORS_ONLN
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (cores > 0) {
    return (int)cores;
  }
#endif
  return 4;  // Safe fallback
}

/**
 * Detect total and available RAM
 * 
 * NOTE: On Android, sysinfo.freeram only shows truly unused RAM (very low!).
 * Android intentionally uses most RAM for caching. We estimate "usable RAM"
 * as 50% of total RAM, which accounts for system, apps, and our model.
 */
static void detect_memory(long* total_mb, long* available_mb) {
#ifdef __ANDROID__
  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    *total_mb = info.totalram / (1024 * 1024);
    
    // Android memory management: Don't trust freeram (it's too conservative)
    // Estimate usable RAM as 50% of total (accounts for system + other apps)
    // This gives us a realistic estimate of what we can safely use
    *available_mb = (*total_mb) / 2;
    
    ETHERVOX_LOG_DEBUG("[Device Profile] RAM: total=%ld MB, freeram=%ld MB, usable=%ld MB",
                       *total_mb, info.freeram / (1024 * 1024), *available_mb);
    return;
  }
#endif
  
  // Fallback
  *total_mb = 4096;  // Assume 4GB
  *available_mb = 2048;
}

/**
 * Detect ARM NEON support (SIMD acceleration)
 */
static bool detect_neon_support(void) {
#ifdef __aarch64__
  return true;  // All ARM64 has NEON
#elif defined(__ARM_NEON)
  return true;
#else
  return false;
#endif
}

/**
 * Detect SoC name from Android properties
 */
static void detect_soc_name(char* name, size_t len) {
#ifdef __ANDROID__
  char prop_value[PROP_VALUE_MAX];
  
  // Try hardware name first
  if (__system_property_get("ro.hardware", prop_value) > 0) {
    snprintf(name, len, "%s", prop_value);
    return;
  }
  
  // Try board platform
  if (__system_property_get("ro.board.platform", prop_value) > 0) {
    snprintf(name, len, "%s", prop_value);
    return;
  }
#endif
  
  snprintf(name, len, "unknown");
}

/**
 * Classify device tier based on capabilities
 */
static device_tier_t classify_device_tier(int cores, long ram_mb) {
  // Ultra tier: Premium flagship (8+ cores, 8GB+ RAM)
  if (cores >= 8 && ram_mb >= 8192) {
    return DEVICE_TIER_ULTRA;
  }
  
  // High tier: Flagship (6-8 cores, 6-8GB RAM)
  if (cores >= 6 && ram_mb >= 6144) {
    return DEVICE_TIER_HIGH;
  }
  
  // Medium tier: Mid-range (4-6 cores, 4-6GB RAM)
  if (cores >= 4 && ram_mb >= 4096) {
    return DEVICE_TIER_MEDIUM;
  }
  
  // Low tier: Budget (<4 cores or <4GB RAM)
  return DEVICE_TIER_LOW;
}

/**
 * Initialize device capability detection
 */
void ethervox_device_profile_init(void) {
  if (g_caps_initialized) {
    return;
  }
  
  g_device_caps.cpu_cores = detect_cpu_cores();
  detect_memory(&g_device_caps.total_ram_mb, &g_device_caps.available_ram_mb);
  g_device_caps.has_neon = detect_neon_support();
  detect_soc_name(g_device_caps.soc_name, sizeof(g_device_caps.soc_name));
  g_device_caps.tier = classify_device_tier(g_device_caps.cpu_cores, g_device_caps.total_ram_mb);
  
  // TODO: GPU detection via Vulkan/OpenCL
  g_device_caps.has_gpu = false;
  
  g_caps_initialized = true;
  
  const char* tier_names[] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
  ETHERVOX_LOG_INFO("[Device Profile] Initialized:");
  ETHERVOX_LOG_INFO("  SoC: %s", g_device_caps.soc_name);
  ETHERVOX_LOG_INFO("  CPU Cores: %d", g_device_caps.cpu_cores);
  ETHERVOX_LOG_INFO("  Total RAM: %ld MB", g_device_caps.total_ram_mb);
  ETHERVOX_LOG_INFO("  Usable RAM: %ld MB (50%% estimate for safety)", g_device_caps.available_ram_mb);
  ETHERVOX_LOG_INFO("  NEON Support: %s", g_device_caps.has_neon ? "YES" : "NO");
  ETHERVOX_LOG_INFO("  Device Tier: %s", tier_names[g_device_caps.tier]);
}

/**
 * Get optimal thread count for this device
 */
int ethervox_device_profile_get_optimal_threads(void) {
  if (!g_caps_initialized) {
    ethervox_device_profile_init();
  }
  
  switch (g_device_caps.tier) {
    case DEVICE_TIER_ULTRA:
      // Use 6 threads on ultra devices (leave headroom for system)
      return 6;
      
    case DEVICE_TIER_HIGH:
      // Use 4 threads on flagship
      return 4;
      
    case DEVICE_TIER_MEDIUM:
      // Use 4 threads on mid-range (often have 4-6 cores)
      return 4;
      
    case DEVICE_TIER_LOW:
      // Use 2 threads on budget devices
      return 2;
      
    default:
      return 4;
  }
}

/**
 * Get optimal batch size for system prompt processing
 */
int ethervox_device_profile_get_optimal_batch_size(void) {
  if (!g_caps_initialized) {
    ethervox_device_profile_init();
  }
  
  switch (g_device_caps.tier) {
    case DEVICE_TIER_ULTRA:
    case DEVICE_TIER_HIGH:
      // Full batch on flagship devices
      return 1024;
      
    case DEVICE_TIER_MEDIUM:
      // Slightly smaller for mid-range
      return 512;
      
    case DEVICE_TIER_LOW:
      // Small batches for budget devices (reduce memory pressure)
      return 256;
      
    default:
      return 512;
  }
}

/**
 * Get optimal KV cache type based on available memory
 * NOTE: KV cache quantization (Q8_0, Q4_0) requires flash attention in llama.cpp
 */
int ethervox_device_profile_get_optimal_kv_cache_type(void) {
  if (!g_caps_initialized) {
    ethervox_device_profile_init();
  }
  
  // CRITICAL: KV cache quantization requires flash attention
  // If flash attention is disabled, we MUST use F16 (no quantization)
  bool flash_enabled = ethervox_device_profile_should_use_flash_attention();
  
  if (!flash_enabled) {
    // Flash attention disabled - cannot use quantized KV cache
    ETHERVOX_LOG_INFO("[Device Profile] KV cache quantization disabled (requires flash attention)");
    return 1;  // GGML_TYPE_F16
  }
  
  // Flash attention enabled - can use quantized cache based on RAM
  // F16 uses ~2X memory of Q8_0 but is faster
  
  if (g_device_caps.available_ram_mb > 3000) {
    // Plenty of RAM - use F16 for speed
    return 1;  // GGML_TYPE_F16
  } else if (g_device_caps.available_ram_mb > 1500) {
    // Moderate RAM - use Q8_0 (good balance)
    return 7;  // GGML_TYPE_Q8_0
  } else {
    // Low RAM - use maximum quantization
    return 8;  // GGML_TYPE_Q4_0
  }
}

/**
 * Should flash attention be enabled? (requires more compute)
 */
bool ethervox_device_profile_should_use_flash_attention(void) {
  if (!g_caps_initialized) {
    ethervox_device_profile_init();
  }
  
  // Flash attention benefits high-end devices most
  return (g_device_caps.tier >= DEVICE_TIER_MEDIUM);
}

/**
 * Get device tier (for external use)
 */
int ethervox_device_profile_get_tier(void) {
  if (!g_caps_initialized) {
    ethervox_device_profile_init();
  }
  return g_device_caps.tier;
}

/**
 * Get CPU core count
 */
int ethervox_device_profile_get_cpu_cores(void) {
  if (!g_caps_initialized) {
    ethervox_device_profile_init();
  }
  return g_device_caps.cpu_cores;
}

/**
 * Get total RAM in MB
 */
long ethervox_device_profile_get_total_ram_mb(void) {
  if (!g_caps_initialized) {
    ethervox_device_profile_init();
  }
  return g_device_caps.total_ram_mb;
}
