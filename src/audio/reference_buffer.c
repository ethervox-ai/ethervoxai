/**
 * @file reference_buffer.c
 * @brief Thread-safe circular buffer for TTS reference signal
 */

#include "ethervox/reference_buffer.h"
#include "ethervox/logging.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct ethervox_reference_buffer_s {
    float* data;           // Circular buffer storage
    size_t capacity;       // Total buffer size in samples
    size_t write_pos;      // Write position (head)
    size_t read_pos;       // Read position (tail)
    size_t count;          // Number of samples currently in buffer
    pthread_mutex_t lock;  // Thread safety
};

ethervox_reference_buffer_t* ethervox_reference_buffer_create(size_t capacity) {
    if (capacity == 0) {
        ETHERVOX_LOG_ERROR("Invalid buffer capacity: %zu", capacity);
        return NULL;
    }
    
    ethervox_reference_buffer_t* buffer = (ethervox_reference_buffer_t*)calloc(1, sizeof(*buffer));
    if (!buffer) {
        ETHERVOX_LOG_ERROR("Failed to allocate reference buffer structure");
        return NULL;
    }
    
    buffer->data = (float*)calloc(capacity, sizeof(float));
    if (!buffer->data) {
        ETHERVOX_LOG_ERROR("Failed to allocate reference buffer data: %zu samples", capacity);
        free(buffer);
        return NULL;
    }
    
    buffer->capacity = capacity;
    buffer->write_pos = 0;
    buffer->read_pos = 0;
    buffer->count = 0;
    
    if (pthread_mutex_init(&buffer->lock, NULL) != 0) {
        ETHERVOX_LOG_ERROR("Failed to initialize buffer mutex");
        free(buffer->data);
        free(buffer);
        return NULL;
    }
    
    ETHERVOX_LOG_DEBUG("Created reference buffer: capacity=%zu samples (%.1f seconds @ 16kHz)", 
                       capacity, (float)capacity / 16000.0f);
    
    return buffer;
}

size_t ethervox_reference_buffer_write(ethervox_reference_buffer_t* buffer, 
                                       const float* samples, 
                                       size_t count) {
    if (!buffer || !samples || count == 0) {
        return 0;
    }
    
    pthread_mutex_lock(&buffer->lock);
    
    // Calculate how many samples we can actually write
    size_t available_space = buffer->capacity - buffer->count;
    size_t to_write = (count < available_space) ? count : available_space;
    
    if (to_write == 0) {
        // Buffer full - could either drop samples or overwrite old data
        // For AEC, dropping is safer than desync
        pthread_mutex_unlock(&buffer->lock);
        ETHERVOX_LOG_WARN("Reference buffer full, dropping %zu samples", count);
        return 0;
    }
    
    // Write samples in two parts if wrapping around
    size_t first_part = buffer->capacity - buffer->write_pos;
    if (first_part > to_write) {
        first_part = to_write;
    }
    
    memcpy(&buffer->data[buffer->write_pos], samples, first_part * sizeof(float));
    
    if (to_write > first_part) {
        // Wrapped around - write remainder at beginning
        size_t second_part = to_write - first_part;
        memcpy(buffer->data, &samples[first_part], second_part * sizeof(float));
    }
    
    buffer->write_pos = (buffer->write_pos + to_write) % buffer->capacity;
    buffer->count += to_write;
    
    pthread_mutex_unlock(&buffer->lock);
    
    if (to_write < count) {
        ETHERVOX_LOG_WARN("Reference buffer partial write: %zu/%zu samples", to_write, count);
    }
    
    return to_write;
}

size_t ethervox_reference_buffer_read(ethervox_reference_buffer_t* buffer,
                                      float* samples,
                                      size_t count) {
    if (!buffer || !samples || count == 0) {
        return 0;
    }
    
    pthread_mutex_lock(&buffer->lock);
    
    // Calculate how many samples are available
    size_t to_read = (count < buffer->count) ? count : buffer->count;
    
    if (to_read == 0) {
        // Buffer empty - return silence
        pthread_mutex_unlock(&buffer->lock);
        memset(samples, 0, count * sizeof(float));
        return 0;
    }
    
    // Read samples in two parts if wrapping around
    size_t first_part = buffer->capacity - buffer->read_pos;
    if (first_part > to_read) {
        first_part = to_read;
    }
    
    memcpy(samples, &buffer->data[buffer->read_pos], first_part * sizeof(float));
    
    if (to_read > first_part) {
        // Wrapped around - read remainder from beginning
        size_t second_part = to_read - first_part;
        memcpy(&samples[first_part], buffer->data, second_part * sizeof(float));
    }
    
    buffer->read_pos = (buffer->read_pos + to_read) % buffer->capacity;
    buffer->count -= to_read;
    
    pthread_mutex_unlock(&buffer->lock);
    
    // Fill remainder with silence if we didn't read enough
    if (to_read < count) {
        memset(&samples[to_read], 0, (count - to_read) * sizeof(float));
    }
    
    return to_read;
}

size_t ethervox_reference_buffer_available(const ethervox_reference_buffer_t* buffer) {
    if (!buffer) {
        return 0;
    }
    
    // No lock needed - single atomic read
    return buffer->count;
}

size_t ethervox_reference_buffer_space(const ethervox_reference_buffer_t* buffer) {
    if (!buffer) {
        return 0;
    }
    
    // No lock needed - single atomic read
    return buffer->capacity - buffer->count;
}

void ethervox_reference_buffer_clear(ethervox_reference_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    pthread_mutex_lock(&buffer->lock);
    
    buffer->write_pos = 0;
    buffer->read_pos = 0;
    buffer->count = 0;
    memset(buffer->data, 0, buffer->capacity * sizeof(float));
    
    pthread_mutex_unlock(&buffer->lock);
    
    ETHERVOX_LOG_DEBUG("Reference buffer cleared");
}

bool ethervox_reference_buffer_is_empty(const ethervox_reference_buffer_t* buffer) {
    return buffer ? (buffer->count == 0) : true;
}

void ethervox_reference_buffer_destroy(ethervox_reference_buffer_t* buffer) {
    if (!buffer) {
        return;
    }
    
    pthread_mutex_destroy(&buffer->lock);
    free(buffer->data);
    free(buffer);
    
    ETHERVOX_LOG_DEBUG("Reference buffer destroyed");
}
