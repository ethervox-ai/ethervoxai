/**
 * @file device_profile.h
 * @brief Device capability detection and performance profiling
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#ifndef ETHERVOX_DEVICE_PROFILE_H
#define ETHERVOX_DEVICE_PROFILE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize device profiling system
 * Auto-detects CPU cores, RAM, SoC, etc.
 */
void ethervox_device_profile_init(void);

/**
 * Get optimal thread count for this device
 * @return Recommended number of threads (2-6)
 */
int ethervox_device_profile_get_optimal_threads(void);

/**
 * Get optimal batch size for system prompt processing
 * @return Recommended batch size (256-1024)
 */
int ethervox_device_profile_get_optimal_batch_size(void);

/**
 * Get optimal KV cache type (quantization level)
 * @return GGML type enum: 1=F16, 7=Q8_0, 8=Q4_0
 */
int ethervox_device_profile_get_optimal_kv_cache_type(void);

/**
 * Should flash attention be enabled?
 * @return true if device has sufficient compute for flash attention
 */
bool ethervox_device_profile_should_use_flash_attention(void);

/**
 * Get device performance tier
 * @return 0=LOW, 1=MEDIUM, 2=HIGH, 3=ULTRA
 */
int ethervox_device_profile_get_tier(void);

/**
 * Get CPU core count
 * @return Number of CPU cores available
 */
int ethervox_device_profile_get_cpu_cores(void);

/**
 * Get total system RAM
 * @return Total RAM in MB
 */
long ethervox_device_profile_get_total_ram_mb(void);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_DEVICE_PROFILE_H
