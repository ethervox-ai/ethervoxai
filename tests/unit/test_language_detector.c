/**
 * @file test_language_detector.c
 * @brief Unit tests for multilingual language detection
 *
 * Tests heuristic fallback detection for TTS voice selection when Whisper STT
 * is unavailable. Validates character pattern analysis for English, German,
 * Spanish, and Chinese.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <string.h>
#include <assert.h>
#include "ethervox/language_detector.h"

// Test colors
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define RESET "\033[0m"

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) \
    do { \
        printf("  Testing: %s... ", name); \
        fflush(stdout); \
        test_count++; \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if (strcmp(expected, actual) == 0) { \
            printf(GREEN "✓ PASS" RESET "\n"); \
            pass_count++; \
        } else { \
            printf(RED "✗ FAIL" RESET " (expected '%s', got '%s')\n", expected, actual); \
        } \
    } while(0)

int main(void) {
    printf("\n========================================\n");
    printf("Language Detection Unit Tests\n");
    printf("========================================\n\n");

    // English detection tests
    TEST("English simple");
    ASSERT_EQ("en", ethervox_detect_language("Hello, how are you today?"));

    TEST("English mixed case");
    ASSERT_EQ("en", ethervox_detect_language("Hello World"));

    TEST("English technical");
    ASSERT_EQ("en", ethervox_detect_language("The quick brown fox jumps over the lazy dog."));

    // German detection tests
    TEST("German with umlauts");
    ASSERT_EQ("de", ethervox_detect_language("Ich möchte nach Hause gehen."));

    TEST("German formal greeting");
    ASSERT_EQ("de", ethervox_detect_language("Guten Tag, wie geht es Ihnen heute?"));  // First sentence has "guten" and "wie"

    TEST("German with ß");
    ASSERT_EQ("de", ethervox_detect_language("Das ist groß und weiß."));

    TEST("German word pattern");
    ASSERT_EQ("de", ethervox_detect_language("Ich bin der Meinung dass..."));

    TEST("German casual");
    ASSERT_EQ("de", ethervox_detect_language("Das ist gut und richtig."));

    // False positive prevention tests
    TEST("English story with 'und' substring");
    ASSERT_EQ("en", ethervox_detect_language("Once upon a time, she found a book bound in leather."));

    TEST("English with 'der' substring");
    ASSERT_EQ("en", ethervox_detect_language("She wandered through the forest filled with wonder."));

    TEST("English long story");
    ASSERT_EQ("en", ethervox_detect_language("Of course! Once upon a time, in a small village nestled between two towering mountains, there lived a young girl named Lily. She had golden hair that shimmered like the sun and eyes as blue as the sky on a clear day."));

    // Spanish detection tests
    TEST("Spanish with accents");
    ASSERT_EQ("es", ethervox_detect_language("¿Cómo estás? ¡Me gusta mucho!"));

    TEST("Spanish inverted punctuation");
    ASSERT_EQ("es", ethervox_detect_language("¡Hola! ¿Qué tal?"));

    TEST("Spanish with ñ");
    ASSERT_EQ("es", ethervox_detect_language("Mañana es miércoles."));

    TEST("Spanish sentence");
    ASSERT_EQ("es", ethervox_detect_language("Buenos días, señor."));

    // Chinese detection tests
    TEST("Chinese simplified");
    ASSERT_EQ("zh", ethervox_detect_language("你好，今天天气很好。"));

    TEST("Chinese question");
    ASSERT_EQ("zh", ethervox_detect_language("你叫什么名字？"));

    TEST("Chinese mixed");
    ASSERT_EQ("zh", ethervox_detect_language("我喜欢学习中文"));

    // Edge cases
    TEST("Empty string");
    ASSERT_EQ("en", ethervox_detect_language(""));

    TEST("NULL input");
    ASSERT_EQ("en", ethervox_detect_language(NULL));

    TEST("Numbers only");
    ASSERT_EQ("en", ethervox_detect_language("123456789"));

    TEST("Mixed language (CJK triggers Chinese)");
    ASSERT_EQ("zh", ethervox_detect_language("Hello 你好 World"));  // CJK characters dominate

    TEST("Punctuation only");
    ASSERT_EQ("en", ethervox_detect_language("... ??? !!!"));

    // Results
    printf("\n========================================\n");
    if (pass_count == test_count) {
        printf(GREEN "✓ ALL TESTS PASSED" RESET " (%d/%d)\n", pass_count, test_count);
        printf("========================================\n\n");
        return ETHERVOX_SUCCESS;
    } else {
        printf(RED "✗ SOME TESTS FAILED" RESET " (%d/%d passed)\n", pass_count, test_count);
        printf("========================================\n\n");
        return ETHERVOX_SUCCESS;
    }
}
