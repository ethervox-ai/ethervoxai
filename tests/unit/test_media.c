/**
 * @file test_media.c
 * @brief Unit tests for multimodal media API
 *
 * Tests media type validation, capability queries, and data preparation.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/media.h"
#include "test_utils.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <string.h>

// Test helpers
#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "❌ FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

#define TEST(name) \
    printf("Running %s...\n", #name); \
    if (name() != 0) { \
        printf("❌ Test %s FAILED\n", #name); \
        return 1; \
    } \
    printf("✅ Test %s PASSED\n", #name)

/**
 * Test 1: NULL pointer safety
 */
static int test_null_safety(void) {
    ethervox_capabilities_t caps;
    ethervox_media_t media = {0};
    
    // NULL handle to capabilities query
    ASSERT(ethervox_model_capabilities(NULL, &caps) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject NULL handle");
    
    // NULL output to capabilities query  
    // Note: We can't call with NULL handle and valid output, so skip this test
    
    // NULL media to prepare
    ASSERT(ethervox_media_prepare(NULL) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject NULL media");
    
    // NULL media to free (should not crash)
    ethervox_media_free(NULL);
    
    return 0;
}

/**
 * Test 2: Media validation - RGBA8 images
 */
static int test_rgba8_validation(void) {
    unsigned char image_data[64 * 48 * 4];  // 64x48 RGBA8 image
    memset(image_data, 0, sizeof(image_data));
    
    ethervox_media_t media = {
        .kind = ETHERVOX_MEDIA_IMAGE_RGBA8,
        .data = image_data,
        .data_size = sizeof(image_data),
        .width = 64,
        .height = 48,
        .sample_rate = 0,
        .id = "test_image"
    };
    
    // Valid RGBA8 image
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_SUCCESS,
           "Should accept valid RGBA8 image");
    
    // Invalid dimensions (zero width)
    media.width = 0;
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject zero width");
    media.width = 64;
    
    // Invalid dimensions (zero height)
    media.height = 0;
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject zero height");
    media.height = 48;
    
    // Invalid data size (doesn't match dimensions)
    media.data_size = 1000;
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject mismatched data size");
    
    return 0;
}

/**
 * Test 3: Media validation - Audio PCM16
 */
static int test_audio_validation(void) {
    int16_t audio_data[16000];  // 1 second at 16kHz
    memset(audio_data, 0, sizeof(audio_data));
    
    ethervox_media_t media = {
        .kind = ETHERVOX_MEDIA_AUDIO_PCM16,
        .data = audio_data,
        .data_size = sizeof(audio_data),
        .width = 0,
        .height = 0,
        .sample_rate = 16000,
        .id = "test_audio"
    };
    
    // Valid PCM16 audio
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_SUCCESS,
           "Should accept valid PCM16 audio");
    
    // Invalid sample rate (zero)
    media.sample_rate = 0;
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject zero sample rate");
    media.sample_rate = 16000;
    
    // Invalid data size (odd number of bytes)
    media.data_size = 1001;
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject odd-sized audio data");
    
    return 0;
}

/**
 * Test 4: Media validation - encoded images
 */
static int test_encoded_image_validation(void) {
    // Minimal PNG header (8 bytes)
    unsigned char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                                  0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52};
    
    ethervox_media_t media = {
        .kind = ETHERVOX_MEDIA_IMAGE_PNG,
        .data = png_header,
        .data_size = sizeof(png_header),
        .width = 0,   // Dimensions not required for encoded images
        .height = 0,
        .sample_rate = 0,
        .id = NULL
    };
    
    // Valid PNG data
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_SUCCESS,
           "Should accept encoded PNG data");
    
    // Too small
    media.data_size = 4;
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject too-small encoded image");
    
    return 0;
}

/**
 * Test 5: Invalid media kind
 */
static int test_invalid_kind(void) {
    unsigned char data[64];
    memset(data, 0, sizeof(data));
    
    ethervox_media_t media = {
        .kind = (ethervox_media_kind_t)999,  // Invalid kind
        .data = data,
        .data_size = sizeof(data),
        .width = 8,
        .height = 8,
        .sample_rate = 0,
        .id = NULL
    };
    
    ASSERT(ethervox_media_prepare(&media) == ETHERVOX_ERROR_INVALID_ARGUMENT,
           "Should reject invalid media kind");
    
    return 0;
}

/**
 * Test 6: Media free
 */
static int test_media_free(void) {
    ethervox_media_t media = {
        .kind = ETHERVOX_MEDIA_IMAGE_RGBA8,
        .data = NULL,
        .data_size = 0,
        .width = 64,
        .height = 48,
        .sample_rate = 0,
        .id = "test"
    };
    
    // Should not crash
    ethervox_media_free(&media);
    
    // Should clear the struct
    ASSERT(media.kind == 0, "Should clear kind");
    ASSERT(media.data == NULL, "Should clear data");
    ASSERT(media.data_size == 0, "Should clear data_size");
    
    return 0;
}

int main(void) {
    printf("=== Multimodal Media API Tests ===\n\n");
    
    TEST(test_null_safety);
    TEST(test_rgba8_validation);
    TEST(test_audio_validation);
    TEST(test_encoded_image_validation);
    TEST(test_invalid_kind);
    TEST(test_media_free);
    
    printf("\n✅ All 6 tests passed!\n");
    return 0;
}
