/**
 * @file reference_buffer.h
 * @brief Circular buffer for TTS reference signal storage
 * 
 * Thread-safe ring buffer used to store TTS output samples for AEC.
 * Allows TTS playback thread to write samples while microphone capture
 * thread reads time-aligned samples for echo cancellation.
 */

#ifndef ETHERVOX_REFERENCE_BUFFER_H
#define ETHERVOX_REFERENCE_BUFFER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque reference buffer handle
 */
typedef struct ethervox_reference_buffer_s ethervox_reference_buffer_t;

/**
 * Create reference buffer
 * 
 * @param capacity Buffer size in samples (e.g., 32000 = 2 seconds @ 16kHz)
 * @return Buffer handle, or NULL on allocation failure
 */
ethervox_reference_buffer_t* ethervox_reference_buffer_create(size_t capacity);

/**
 * Write samples to buffer (from TTS playback thread)
 * 
 * Thread-safe: Can be called concurrently with read operations.
 * 
 * @param buffer Buffer handle
 * @param samples Audio samples to write
 * @param count Number of samples
 * @return Number of samples actually written (may be less if buffer full)
 */
size_t ethervox_reference_buffer_write(ethervox_reference_buffer_t* buffer, 
                                       const float* samples, 
                                       size_t count);

/**
 * Read samples from buffer (from microphone capture thread)
 * 
 * Thread-safe: Can be called concurrently with write operations.
 * Returns time-aligned samples for AEC processing.
 * 
 * @param buffer Buffer handle
 * @param samples Output buffer for samples
 * @param count Number of samples to read
 * @return Number of samples actually read (may be less if buffer empty)
 */
size_t ethervox_reference_buffer_read(ethervox_reference_buffer_t* buffer,
                                      float* samples,
                                      size_t count);

/**
 * Get number of samples available for reading
 * 
 * @param buffer Buffer handle
 * @return Number of samples ready to read
 */
size_t ethervox_reference_buffer_available(const ethervox_reference_buffer_t* buffer);

/**
 * Get remaining buffer capacity
 * 
 * @param buffer Buffer handle
 * @return Number of samples that can be written without blocking
 */
size_t ethervox_reference_buffer_space(const ethervox_reference_buffer_t* buffer);

/**
 * Clear all data from buffer
 * 
 * @param buffer Buffer handle
 */
void ethervox_reference_buffer_clear(ethervox_reference_buffer_t* buffer);

/**
 * Check if buffer is empty
 * 
 * @param buffer Buffer handle
 * @return true if no samples available for reading
 */
bool ethervox_reference_buffer_is_empty(const ethervox_reference_buffer_t* buffer);

/**
 * Destroy reference buffer
 * 
 * @param buffer Buffer handle (may be NULL)
 */
void ethervox_reference_buffer_destroy(ethervox_reference_buffer_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_REFERENCE_BUFFER_H
