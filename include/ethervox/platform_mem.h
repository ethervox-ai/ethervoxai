/**
 * @file platform_mem.h
 * @brief Cross-platform memory operations
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 *
 * Thin static inline wrappers over POSIX and Win32 memory APIs.
 * Zero overhead, header-only, chosen by CMake defines.
 */

#ifndef ETHERVOX_PLATFORM_MEM_H
#define ETHERVOX_PLATFORM_MEM_H

#include "ethervox/error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#endif

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Aligned Allocation
// ============================================================================

#ifdef _WIN32
#include <malloc.h>

/**
 * Allocate aligned memory
 * 
 * @param size Size in bytes
 * @param alignment Alignment in bytes (must be power of 2)
 * @return Pointer to aligned memory, or NULL on failure
 */
static inline void* ethervox_mem_aligned_alloc(size_t size, size_t alignment) {
    return _aligned_malloc(size, alignment);
}

/**
 * Free aligned memory
 * 
 * @param ptr Pointer returned by ethervox_mem_aligned_alloc
 */
static inline void ethervox_mem_aligned_free(void* ptr) {
    _aligned_free(ptr);
}

#else
// POSIX implementation

static inline void* ethervox_mem_aligned_alloc(size_t size, size_t alignment) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

static inline void ethervox_mem_aligned_free(void* ptr) {
    free(ptr);
}

#endif

// ============================================================================
// Memory Mapping (for large files)
// ============================================================================

#ifdef _WIN32
#include <windows.h>

typedef struct {
    HANDLE file_handle;
    HANDLE map_handle;
    void* data;
    size_t size;
} ethervox_mmap_t;

/**
 * Memory-map a file for reading
 * 
 * @param path File path
 * @param mmap Output memory map structure
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_mmap_open(const char* path, ethervox_mmap_t* mapping) {
    if (!path || !mapping) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    mapping->file_handle = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (mapping->file_handle == INVALID_HANDLE_VALUE) {
        return ETHERVOX_ERROR_FILE_NOT_FOUND;
    }
    
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(mapping->file_handle, &file_size)) {
        CloseHandle(mapping->file_handle);
        return ETHERVOX_ERROR_FAILED;
    }
    mapping->size = (size_t)file_size.QuadPart;
    
    mapping->map_handle = CreateFileMappingA(
        mapping->file_handle,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );
    
    if (!mapping->map_handle) {
        CloseHandle(mapping->file_handle);
        return ETHERVOX_ERROR_FAILED;
    }
    
    mapping->data = MapViewOfFile(
        mapping->map_handle,
        FILE_MAP_READ,
        0,
        0,
        0
    );
    
    if (!mapping->data) {
        CloseHandle(mapping->map_handle);
        CloseHandle(mapping->file_handle);
        return ETHERVOX_ERROR_FAILED;
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * Unmap and close memory-mapped file
 * 
 * @param mmap Memory map structure
 */
static inline void ethervox_mmap_close(ethervox_mmap_t* mapping) {
    if (!mapping) return;
    
    if (mapping->data) {
        UnmapViewOfFile(mapping->data);
        mapping->data = NULL;
    }
    
    if (mapping->map_handle) {
        CloseHandle(mapping->map_handle);
        mapping->map_handle = NULL;
    }
    
    if (mapping->file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(mapping->file_handle);
        mapping->file_handle = INVALID_HANDLE_VALUE;
    }
}

#else
// POSIX implementation
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    int fd;
    void* data;
    size_t size;
} ethervox_mmap_t;

static inline ethervox_result_t ethervox_mmap_open(const char* path, ethervox_mmap_t* mapping) {
    if (!path || !mapping) return ETHERVOX_ERROR_INVALID_ARGUMENT;
    
    mapping->fd = open(path, O_RDONLY);
    if (mapping->fd < 0) {
        return ETHERVOX_ERROR_FILE_NOT_FOUND;
    }
    
    struct stat st;
    if (fstat(mapping->fd, &st) != 0) {
        close(mapping->fd);
        return ETHERVOX_ERROR_FAILED;
    }
    mapping->size = (size_t)st.st_size;
    
    mapping->data = mmap(NULL, mapping->size, PROT_READ, MAP_PRIVATE, mapping->fd, 0);
    if (mapping->data == MAP_FAILED) {
        close(mapping->fd);
        return ETHERVOX_ERROR_FAILED;
    }
    
    return ETHERVOX_SUCCESS;
}

static inline void ethervox_mmap_close(ethervox_mmap_t* mapping) {
    if (!mapping) return;
    
    if (mapping->data && mapping->data != MAP_FAILED) {
        munmap(mapping->data, mapping->size);
        mapping->data = NULL;
    }
    
    if (mapping->fd >= 0) {
        close(mapping->fd);
        mapping->fd = -1;
    }
}

#endif

// ============================================================================
// Memory Info
// ============================================================================

/**
 * Get system memory info
 * 
 * @param out_total_mb Total system memory in MB (can be NULL)
 * @param out_available_mb Available memory in MB (can be NULL)
 * @return ETHERVOX_SUCCESS or error code
 */
static inline ethervox_result_t ethervox_mem_get_info(
    uint64_t* out_total_mb,
    uint64_t* out_available_mb
) {
#ifdef _WIN32
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    
    if (!GlobalMemoryStatusEx(&mem_status)) {
        return ETHERVOX_ERROR_FAILED;
    }
    
    if (out_total_mb) {
        *out_total_mb = mem_status.ullTotalPhys / (1024 * 1024);
    }
    
    if (out_available_mb) {
        *out_available_mb = mem_status.ullAvailPhys / (1024 * 1024);
    }
    
    return ETHERVOX_SUCCESS;
#elif defined(__APPLE__)
    if (out_total_mb) {
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        uint64_t physical_memory;
        size_t length = sizeof(physical_memory);
        
        if (sysctl(mib, 2, &physical_memory, &length, NULL, 0) == 0) {
            *out_total_mb = physical_memory / (1024 * 1024);
        } else {
            return ETHERVOX_ERROR_FAILED;
        }
    }
    
    if (out_available_mb) {
        mach_port_t host_port = mach_host_self();
        vm_size_t page_size;
        vm_statistics64_data_t vm_stats;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        
        if (host_page_size(host_port, &page_size) == KERN_SUCCESS &&
            host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
            
            uint64_t free_memory = (uint64_t)(vm_stats.free_count * page_size);
            *out_available_mb = free_memory / (1024 * 1024);
        } else {
            return ETHERVOX_ERROR_FAILED;
        }
    }
    
    return ETHERVOX_SUCCESS;
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return ETHERVOX_ERROR_FAILED;
    }
    
    if (out_total_mb) {
        *out_total_mb = (info.totalram * info.mem_unit) / (1024 * 1024);
    }
    
    if (out_available_mb) {
        *out_available_mb = (info.freeram * info.mem_unit) / (1024 * 1024);
    }
    
    return ETHERVOX_SUCCESS;
#else
    // Unsupported platform
    if (out_total_mb) *out_total_mb = 0;
    if (out_available_mb) *out_available_mb = 0;
    return ETHERVOX_ERROR_NOT_SUPPORTED;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_PLATFORM_MEM_H */
