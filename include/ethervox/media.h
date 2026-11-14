/**
 * @file media.h
 * @brief Multimodal media API for vision and audio inputs
 *
 * Provides a media-agnostic interface over llama.cpp's mtmd library for
 * processing images and audio through multimodal LLM models.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_MEDIA_H
#define ETHERVOX_MEDIA_H

#include "ethervox/error.h"
#include "ethervox/cancel_token.h"
#include "ethervox/model_pool.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Media type enumeration
 */
typedef enum {
    ETHERVOX_MEDIA_IMAGE_RGBA8 = 1,    /**< Raw RGBA8 image data (4 bytes/pixel) */
    ETHERVOX_MEDIA_IMAGE_PNG = 2,      /**< PNG-encoded image */
    ETHERVOX_MEDIA_IMAGE_JPEG = 3,     /**< JPEG-encoded image */
    ETHERVOX_MEDIA_AUDIO_PCM16 = 4     /**< 16-bit PCM audio (converted to float internally) */
} ethervox_media_kind_t;

/**
 * Media data container
 */
typedef struct {
    ethervox_media_kind_t kind;        /**< Media type */
    const void* data;                  /**< Media data (format depends on kind) */
    size_t data_size;                  /**< Size of data in bytes */
    uint32_t width;                    /**< Image width (pixels, 0 for audio) */
    uint32_t height;                   /**< Image height (pixels, 0 for audio) */
    uint32_t sample_rate;              /**< Audio sample rate (Hz, 0 for images) */
    const char* id;                    /**< Optional ID for KV cache tracking (can be NULL) */
} ethervox_media_t;

/**
 * Model capability flags
 */
typedef struct {
    bool supports_vision;              /**< Model can process image inputs */
    bool supports_audio;               /**< Model can process audio inputs */
    int audio_sample_rate;             /**< Required audio sample rate (Hz), -1 if not supported */
    const char* media_marker;          /**< Media placeholder marker (e.g., "<__media__>") */
} ethervox_capabilities_t;

/**
 * Query model capabilities
 *
 * Determines what types of media input a model supports.
 *
 * @param handle Model handle from model pool
 * @param out Receives capability information (required)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_model_capabilities(
    ethervox_model_handle_t* handle,
    ethervox_capabilities_t* out
);

/**
 * Prepare media for encoding
 *
 * Validates and prepares media data for model input. For images, performs any
 * necessary format conversions. For audio, converts PCM16 to float32.
 *
 * @param media Media to prepare (modified in place if conversion needed)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t ethervox_media_prepare(
    ethervox_media_t* media
);

/**
 * Free media resources
 *
 * Frees any internally allocated resources associated with media.
 * Does not free the media struct itself.
 *
 * @param media Media to free (can be NULL)
 */
void ethervox_media_free(
    ethervox_media_t* media
);

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_MEDIA_H */
