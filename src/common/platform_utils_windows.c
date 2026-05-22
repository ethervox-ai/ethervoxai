/**
 * @file platform_utils_windows.c
 * @brief Windows-specific platform utilities
 */

#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <Windows.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>

/**
 * Get Windows OS version information
 */
ethervox_result_t platform_get_os_version(char* os_name, size_t os_name_size,
                                         char* os_version, size_t os_version_size) {
    if (!os_name || !os_version) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    
    snprintf(os_name, os_name_size, "Windows");
    
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        snprintf(os_version, os_version_size, "%lu.%lu.%lu", 
                 osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    } else {
        snprintf(os_version, os_version_size, "Unknown");
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * Get system architecture
 */
ethervox_result_t platform_get_architecture(char* architecture, size_t arch_size) {
    if (!architecture) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    SYSTEM_INFO si;
    ZeroMemory(&si, sizeof(SYSTEM_INFO));
    GetSystemInfo(&si);
    
    switch(si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            snprintf(architecture, arch_size, "x86_64");
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            snprintf(architecture, arch_size, "x86");
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            snprintf(architecture, arch_size, "ARM");
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            snprintf(architecture, arch_size, "ARM64");
            break;
        default:
            snprintf(architecture, arch_size, "Unknown");
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * Get hostname
 */
ethervox_result_t platform_get_hostname(char* hostname, size_t hostname_size) {
    if (!hostname) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    DWORD size = (DWORD)hostname_size;
    if (GetComputerNameA(hostname, &size)) {
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(hostname, hostname_size, "Unknown");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Recursively calculate directory size using Windows FindFirstFile API
 */
uint64_t platform_get_directory_size(const char* path) {
    if (!path) {
        return 0;
    }
    
    WIN32_FIND_DATAA find_data;
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);
    
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    uint64_t total_size = 0;
    char subpath[1024];
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        snprintf(subpath, sizeof(subpath), "%s\\%s", path, find_data.cFileName);
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total_size += platform_get_directory_size(subpath);
        } else {
            LARGE_INTEGER file_size;
            file_size.LowPart = find_data.nFileSizeLow;
            file_size.HighPart = find_data.nFileSizeHigh;
            total_size += file_size.QuadPart;
        }
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
    return total_size;
}

/**
 * Get available disk space
 */
ethervox_result_t platform_get_disk_space(const char* path, uint64_t* available_bytes) {
    if (!path || !available_bytes) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    ULARGE_INTEGER free_bytes;
    if (GetDiskFreeSpaceExA(path, &free_bytes, NULL, NULL)) {
        *available_bytes = free_bytes.QuadPart;
        return ETHERVOX_SUCCESS;
    }
    
    *available_bytes = 0;
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Get application data directory (roaming)
 * Windows: %APPDATA%\EthervoxAI
 */
ethervox_result_t platform_get_app_data_dir(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(buffer, buffer_size, "%s\\EthervoxAI", appdata);
        return ETHERVOX_SUCCESS;
    }
    
    // Fallback to USERPROFILE
    char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        snprintf(buffer, buffer_size, "%s\\AppData\\Roaming\\EthervoxAI", userprofile);
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(buffer, buffer_size, "EthervoxAI");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Get local application data directory (non-roaming)
 * Windows: %LOCALAPPDATA%\EthervoxAI
 */
ethervox_result_t platform_get_local_app_data_dir(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* localappdata = getenv("LOCALAPPDATA");
    if (localappdata) {
        snprintf(buffer, buffer_size, "%s\\EthervoxAI", localappdata);
        return ETHERVOX_SUCCESS;
    }
    
    // Fallback to USERPROFILE
    char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        snprintf(buffer, buffer_size, "%s\\AppData\\Local\\EthervoxAI", userprofile);
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(buffer, buffer_size, "EthervoxAI");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Get temporary directory
 * Windows: %TEMP%\EthervoxAI
 */
ethervox_result_t platform_get_temp_dir(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char* temp = getenv("TEMP");
    if (!temp) {
        temp = getenv("TMP");
    }
    
    if (temp) {
        snprintf(buffer, buffer_size, "%s\\EthervoxAI", temp);
        return ETHERVOX_SUCCESS;
    }
    
    snprintf(buffer, buffer_size, "C:\\Temp\\EthervoxAI");
    return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
}

/**
 * Create directory recursively (Windows implementation)
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
    
    // Replace forward slashes with backslashes
    for (size_t i = 0; i < len; i++) {
        if (temp_path[i] == '/') {
            temp_path[i] = '\\';
        }
    }
    
    // Remove trailing backslash if present
    if (len > 0 && temp_path[len - 1] == '\\') {
        temp_path[len - 1] = '\0';
        len--;
    }
    
    // Skip drive letter (e.g., "C:\")
    size_t start_pos = 0;
    if (len >= 2 && temp_path[1] == ':') {
        start_pos = 2;
        if (len > 2 && temp_path[2] == '\\') {
            start_pos = 3;
        }
    }
    
    // Create each directory in the path
    for (size_t i = start_pos; i <= len; i++) {
        if (temp_path[i] == '\\' || temp_path[i] == '\0') {
            char saved = temp_path[i];
            temp_path[i] = '\0';
            
            // Try to create directory
            if (_mkdir(temp_path) != 0) {
                // Check if it already exists
                DWORD attrs = GetFileAttributesA(temp_path);
                if (attrs == INVALID_FILE_ATTRIBUTES) {
                    ETHERVOX_LOG_ERROR("Failed to create directory: %s (error: %lu)", 
                                     temp_path, GetLastError());
                    return ETHERVOX_ERROR_FILE_WRITE;
                }
                if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
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
    
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    char search_path[MAX_PATH];
    char full_path[MAX_PATH];
    
    // Create search pattern
    snprintf(search_path, sizeof(search_path), "%s\\*", path);
    
    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            // Directory doesn't exist - not an error
            return ETHERVOX_SUCCESS;
        }
        return ETHERVOX_ERROR_FILE_DELETE_FAILED;
    }
    
    do {
        // Skip . and ..
        if (strcmp(find_data.cFileName, ".") == 0 || 
            strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        
        // Build full path
        snprintf(full_path, sizeof(full_path), "%s\\%s", path, find_data.cFileName);
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recursively delete subdirectory
            ethervox_result_t result = platform_rmdir_recursive(full_path);
            if (result != ETHERVOX_SUCCESS) {
                FindClose(hFind);
                return result;
            }
        } else {
            // Delete file
            if (!DeleteFileA(full_path)) {
                FindClose(hFind);
                return ETHERVOX_ERROR_FILE_DELETE_FAILED;
            }
        }
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
    
    // Now delete the empty directory itself
    if (!RemoveDirectoryA(path)) {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
            return ETHERVOX_ERROR_FILE_DELETE_FAILED;
        }
    }
    
    return ETHERVOX_SUCCESS;
}
