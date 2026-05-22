/**
 * @file platform_utils.h
 * @brief Cross-platform utility functions
 */

#ifndef ETHERVOX_PLATFORM_UTILS_H
#define ETHERVOX_PLATFORM_UTILS_H

#include "ethervox/error.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get OS name and version
 * @param os_name Buffer for OS name (e.g., "Windows", "Linux")
 * @param os_name_size Size of os_name buffer
 * @param os_version Buffer for OS version (e.g., "10.0.19041")
 * @param os_version_size Size of os_version buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_get_os_version(char* os_name, size_t os_name_size,
                                         char* os_version, size_t os_version_size);

/**
 * Get system architecture
 * @param architecture Buffer for architecture (e.g., "x86_64", "ARM64")
 * @param arch_size Size of architecture buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_get_architecture(char* architecture, size_t arch_size);

/**
 * Get hostname
 * @param hostname Buffer for hostname
 * @param hostname_size Size of hostname buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_get_hostname(char* hostname, size_t hostname_size);

/**
 * Recursively calculate directory size
 * @param path Directory path
 * @return Total size in bytes, or 0 on error
 */
uint64_t platform_get_directory_size(const char* path);

/**
 * Get available disk space
 * @param path Path to check (can be file or directory)
 * @param available_bytes Output parameter for available bytes
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_get_disk_space(const char* path, uint64_t* available_bytes);

/**
 * Get application data directory (roaming/synced user data)
 * Windows: %APPDATA%\EthervoxAI (e.g., C:\Users\Name\AppData\Roaming\EthervoxAI)
 * Unix/Mac: ~/.ethervox
 * @param buffer Buffer to store the path
 * @param buffer_size Size of the buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_get_app_data_dir(char* buffer, size_t buffer_size);

/**
 * Get local application data directory (machine-specific, non-roaming)
 * Windows: %LOCALAPPDATA%\EthervoxAI (e.g., C:\Users\Name\AppData\Local\EthervoxAI)
 * Unix/Mac: ~/.ethervox (same as app_data_dir on Unix)
 * Use this for: models, cache, large files that shouldn't sync
 * @param buffer Buffer to store the path
 * @param buffer_size Size of the buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_get_local_app_data_dir(char* buffer, size_t buffer_size);

/**
 * Get temporary directory
 * Windows: %TEMP%\EthervoxAI
 * Unix/Mac: /tmp/ethervoxai or ~/.ethervox/tmp
 * @param buffer Buffer to store the path
 * @param buffer_size Size of the buffer
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_get_temp_dir(char* buffer, size_t buffer_size);

/**
 * Create directory recursively (like mkdir -p)
 * Creates all parent directories as needed
 * @param path Directory path to create
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_mkdir_recursive(const char* path);
/**
 * Remove directory recursively (like rm -rf)
 * Deletes directory and all its contents
 * @param path Directory path to remove
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t platform_rmdir_recursive(const char* path);
#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_PLATFORM_UTILS_H */
