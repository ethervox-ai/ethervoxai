/**
 * @file media.c
 * @brief Multimodal media API implementation
 *
 * Implements media handling for vision and audio inputs using llama.cpp's mtmd library.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/media.h"
#include "ethervox/error.h"
#include "ethervox/logging.h"
#include <string.h>
#include <stdlib.h>

#if defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE && defined(MTMD_AVAILABLE) && MTMD_AVAILABLE
#include "mtmd.h"
#include "llama.h"
#endif

// Forward declaration of internal model handle structure
// Full definition in model_pool.c
struct ethervox_model_handle {
    void* model;
    void* ctx;
    void* mtmd_ctx;  // Will be NULL if model doesn't support multimodal
    // ... other fields not needed here
};

ethervox_result_t ethervox_model_capabilities(
    ethervox_model_handle_t* handle,
    ethervox_capabilities_t* out
) {
    // Validate inputs
    if (!handle) {
        ETHERVOX_LOG_ERROR("[Media] NULL model handle");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!out) {
        ETHERVOX_LOG_ERROR("[Media] NULL output pointer");
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Initialize output
    memset(out, 0, sizeof(*out));
    out->supports_vision = false;
    out->supports_audio = false;
    out->audio_sample_rate = -1;
    out->media_marker = NULL;

#if defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE && defined(MTMD_AVAILABLE) && MTMD_AVAILABLE
    // Access the model handle's mtmd context through forward-declared structure
    // This is safe because we only access the mtmd_ctx field which is at a known offset
    struct ethervox_model_handle* h = (struct ethervox_model_handle*)handle;
    mtmd_context* mctx = (mtmd_context*)h->mtmd_ctx;
    
    if (!mctx) {
        // Model doesn't have multimodal support - return all false/NULL
        ETHERVOX_LOG_DEBUG("[Media] Model has no mtmd context");
        return ETHERVOX_SUCCESS;
    }

    // Query capabilities from mtmd
    out->supports_vision = mtmd_support_vision(mctx);
    out->supports_audio = mtmd_support_audio(mctx);
    out->audio_sample_rate = mtmd_get_audio_sample_rate(mctx);
    out->media_marker = mtmd_default_marker();

    ETHERVOX_LOG_INFO("[Media] Model capabilities: vision=%d, audio=%d, sample_rate=%d",
                     out->supports_vision, out->supports_audio, out->audio_sample_rate);

    return ETHERVOX_SUCCESS;
#else
    // No multimodal support compiled in
    ETHERVOX_LOG_DEBUG("[Media] Multimodal support not compiled in");
    return ETHERVOX_SUCCESS;
#endif
}

ethervox_result_t ethervox_media_prepare(
    ethervox_media_t* media
) {
    if (!media) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    if (!media->data || media->data_size == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    // Validate media kind
    switch (media->kind) {
        case ETHERVOX_MEDIA_IMAGE_RGBA8:
            // Validate dimensions
            if (media->width == 0 || media->height == 0) {
                return ETHERVOX_ERROR_INVALID_ARGUMENT;
            }
            // Check data size matches dimensions (4 bytes per pixel for RGBA8)
            if (media->data_size != (size_t)(media->width * media->height * 4)) {
                return ETHERVOX_ERROR_INVALID_ARGUMENT;
            }
            break;

        case ETHERVOX_MEDIA_IMAGE_PNG:
        case ETHERVOX_MEDIA_IMAGE_JPEG:
            // Encoded images - dimensions will be determined during decoding
            // Just verify we have data
            if (media->data_size < 16) {  // Minimum valid image size
                return ETHERVOX_ERROR_INVALID_ARGUMENT;
            }
            break;

        case ETHERVOX_MEDIA_AUDIO_PCM16:
            // Validate sample rate
            if (media->sample_rate == 0) {
                return ETHERVOX_ERROR_INVALID_ARGUMENT;
            }
            // Data size should be even (2 bytes per sample)
            if (media->data_size % 2 != 0) {
                return ETHERVOX_ERROR_INVALID_ARGUMENT;
            }
            break;

        default:
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    return ETHERVOX_SUCCESS;
}

void ethervox_media_free(
    ethervox_media_t* media
) {
    if (!media) {
        return;
    }

    // For now, media doesn't own any internal allocations
    // The data pointer is assumed to be owned by the caller
    // This may change if we add format conversion in the future
    memset(media, 0, sizeof(*media));
}
