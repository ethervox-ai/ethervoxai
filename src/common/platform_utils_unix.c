/**
 * @file platform_utils_unix.c
 * @brief Unix/Linux-specific platform utilities
 */

#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

/**
 * Get Unix OS version information
 */
ethervox_result_t platform_get_os_version(char* os_name, size_t os_name_size,
                                         char* os_version, size_t os_version_size) {
    if (!os_name || !os_version) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        snprintf(os_name, os_name_size, "%s", sys_info.sysname);
        snprintf(os_version, os_version_size, "%s", sys_info.release);
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(os_name, os_name_size, "Unknown");
    snprintf(os_version, os_version_size, "Unknown");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Get system architecture
 */
ethervox_result_t platform_get_architecture(char* architecture, size_t arch_size) {
    if (!architecture) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        snprintf(architecture, arch_size, "%s", sys_info.machine);
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(architecture, arch_size, "Unknown");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Get hostname
 */
ethervox_result_t platform_get_hostname(char* hostname, size_t hostname_size) {
    if (!hostname) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    if (gethostname(hostname, hostname_size) == 0) {
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(hostname, hostname_size, "Unknown");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Recursively calculate directory size using Unix readdir
 */
uint64_t platform_get_directory_size(const char* path) {
    if (!path) {
        return 0;
    }
    
    DIR* dir = opendir(path);
    if (!dir) {
        return 0;
    }
    
    uint64_t total_size = 0;
    struct dirent* entry;
    char subpath[1024];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(subpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total_size += platform_get_directory_size(subpath);
            } else {
                total_size += st.st_size;
            }
        }
    }
    
    closedir(dir);
    return total_size;
}

/**
 * Get available disk space
 */
ethervox_result_t platform_get_disk_space(const char* path, uint64_t* available_bytes) {
    if (!path || !available_bytes) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    struct statvfs stat;
    if (statvfs(path, &stat) == 0) {
        *available_bytes = (uint64_t)stat.f_bavail * stat.f_frsize;
        return ETHERVOX_SUCCESS;
    }
    
    *available_bytes = 0;
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Get application data directory
 * Unix/Mac: ~/.ethervox
 */
ethervox_result_t platform_get_app_data_dir(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const char* home = getenv("HOME");
    if (home) {
        snprintf(buffer, buffer_size, "%s/.ethervox", home);
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(buffer, buffer_size, ".ethervox");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Get local application data directory
 * Unix/Mac: Same as app_data_dir (no distinction on Unix)
 */
ethervox_result_t platform_get_local_app_data_dir(char* buffer, size_t buffer_size) {
    // On Unix, local and roaming data are the same
    return platform_get_app_data_dir(buffer, buffer_size);
}

/**
 * Get temporary directory
 * Unix/Mac: ~/.ethervox/tmp or /tmp/ethervoxai
 */
ethervox_result_t platform_get_temp_dir(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    const char* home = getenv("HOME");
    if (home) {
        snprintf(buffer, buffer_size, "%s/.ethervox/tmp", home);
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(buffer, buffer_size, "/tmp/ethervoxai");
    return ETHERVOX_SUCCESS;
}

/**
 * Create directory recursively (Unix implementation)
 */
ethervox_result_t platform_mkdir_recursive(const char* path) {
    if (!path || path[0] == '\0') {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char temp_path[1024];
    size_t len = strlen(path);
    
    if (len >= sizeof(temp_path)) {
        return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Copy path to temporary buffer
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    
    // Remove trailing slash if present
    if (len > 0 && temp_path[len - 1] == '/') {
        temp_path[len - 1] = '\0';
        len--;
    }
    
    // Skip leading slash for absolute paths
    size_t start_pos = (temp_path[0] == '/') ? 1 : 0;
    
    // Create each directory in the path
    for (size_t i = start_pos; i <= len; i++) {
        if (temp_path[i] == '/' || temp_path[i] == '\0') {
            char saved = temp_path[i];
            temp_path[i] = '\0';
            
            // Try to create directory
            if (mkdir(temp_path, 0755) != 0) {
                // Check if it already exists
                struct stat st;
                if (stat(temp_path, &st) != 0) {
                    ETHERVOX_LOG_ERROR("Failed to create directory: %s (error: %s)", 
                                     temp_path, strerror(errno));
                    return ETHERVOX_ERROR_FILE_WRITE;
                }
                if (!S_ISDIR(st.st_mode)) {
                    ETHERVOX_LOG_ERROR("Path exists but is not a directory: %s", temp_path);
                    return ETHERVOX_ERROR_FILE_WRITE;
                }
            }
            
            temp_path[i] = saved;
        }
    }
    
    return ETHERVOX_SUCCESS;
}

ethervox_result_t platform_rmdir_recursive(const char* path) {
    if (!path || !*path) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    DIR* dir = opendir(path);
    if (!dir) {
        // Directory doesn't exist - not an error
        if (errno == ENOENT) {
            return ETHERVOX_SUCCESS;
        }
        return ETHERVOX_ERROR_FILE_DELETE_FAILED;
    }
    
    struct dirent* entry;
    ethervox_result_t result = ETHERVOX_SUCCESS;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == -1) {
            result = ETHERVOX_ERROR_FILE_DELETE_FAILED;
            break;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // Recursively delete subdirectory
            result = platform_rmdir_recursive(full_path);
            if (result != ETHERVOX_SUCCESS) {
                break;
            }
        } else {
            // Delete file
            if (unlink(full_path) != 0) {
                result = ETHERVOX_ERROR_FILE_DELETE_FAILED;
                break;
            }
        }
    }
    
    closedir(dir);
    
    if (result == ETHERVOX_SUCCESS) {
        // Delete the empty directory itself
        if (rmdir(path) != 0 && errno != ENOENT) {
            result = ETHERVOX_ERROR_FILE_DELETE_FAILED;
        }
    }
    
    return result;
}
