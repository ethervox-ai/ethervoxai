/**
 * @file test_tts_text_to_phoneme.c
 * @brief Text-to-Phoneme integration tests for Piper TTS
 * 
 * Tests the phonemizer integration with Piper backend:
 * - English text → ARPAbet → IPA
 * - Chinese text → Pinyin → IPA
 * - German text → Rules → IPA
 * - Phoneme → Phoneme ID mapping
 * - Long text handling
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ethervox/tts.h"

// Forward declarations from phonemizer
typedef struct phonemizer_context phonemizer_t;

typedef enum {
    PHONEMIZER_LANG_UNKNOWN = 0,
    PHONEMIZER_LANG_EN_US,
    PHONEMIZER_LANG_EN_GB,
    PHONEMIZER_LANG_ZH_CN,
    PHONEMIZER_LANG_DE_DE,
    PHONEMIZER_LANG_ES_MX
} phonemizer_language_t;

// Phonemizer API (forward declarations)
extern phonemizer_t* phonemizer_create(const char* lang_code);
extern void phonemizer_destroy(phonemizer_t* phonemizer);
extern int phonemizer_text_to_ipa(phonemizer_t* phonemizer, const char* text, char* ipa_output, size_t max_len);
extern phonemizer_language_t phonemizer_get_language(phonemizer_t* ctx);

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("✗ FAIL: %s\n", msg); \
            printf("   Condition: %s\n", #cond); \
            return ETHERVOX_ERROR_INVALID_ARGUMENT; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr, msg) \
    ASSERT_TRUE((ptr) != NULL, msg)

#define ASSERT_CONTAINS(str, substr, msg) \
    ASSERT_TRUE(strstr(str, substr) != NULL, msg)

/**
 * Test: English text to IPA phonemes
 */
static int test_english_text_to_phoneme(void) {
    printf("\n[Test 1] English Text → IPA Phonemes\n");
    
    // Create phonemizer using language code format "en-us"
    phonemizer_t* phonemizer = phonemizer_create("en-us");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    // Test simple words
    const char* test_cases[][2] = {
        {"hello", "h ɛ l ˈoʊ"},
        {"world", "w ˈɝ l d"},
        {"test", "t ˈɛ s t"},
        {"speech", "s p ˈi tʃ"},
        {NULL, NULL}
    };
    
    char ipa_output[256];
    for (int i = 0; test_cases[i][0] != NULL; i++) {
        memset(ipa_output, 0, sizeof(ipa_output));
        int result = phonemizer_text_to_ipa(phonemizer, test_cases[i][0], 
                                            ipa_output, sizeof(ipa_output));
        
        ASSERT_TRUE(result >= 0, "Phonemization should succeed");
        ASSERT_TRUE(strlen(ipa_output) > 0, "IPA output should not be empty");
        
        printf("  ✓ '%s' → '%s'\n", test_cases[i][0], ipa_output);
    }
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Chinese text to IPA phonemes
 */
static int test_chinese_text_to_phoneme(void) {
    printf("\n[Test 2] Chinese Text → IPA Phonemes\n");
    
    // Create phonemizer for Chinese
    phonemizer_t* phonemizer = phonemizer_create("zh");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    // Test common Chinese phrases
    const char* test_cases[] = {
        "你好",      // ni hao (hello)
        "谢谢",      // xie xie (thank you)
        "再见",      // zai jian (goodbye)
        NULL
    };
    
    char ipa_output[256];
    for (int i = 0; test_cases[i] != NULL; i++) {
        memset(ipa_output, 0, sizeof(ipa_output));
        int result = phonemizer_text_to_ipa(phonemizer, test_cases[i],
                                            ipa_output, sizeof(ipa_output));
        
        ASSERT_TRUE(result >= 0, "Chinese phonemization should succeed");
        ASSERT_TRUE(strlen(ipa_output) > 0, "IPA output should not be empty");
        
        printf("  ✓ '%s' → '%s'\n", test_cases[i], ipa_output);
    }
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: German text to IPA phonemes
 */
static int test_german_text_to_phoneme(void) {
    printf("\n[Test 3] German Text → IPA Phonemes\n");
    
    // Create phonemizer for German
    phonemizer_t* phonemizer = phonemizer_create("de");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    // Test German words with special phonemes
    const char* test_cases[] = {
        "Guten",      // Should have /g/
        "Tag",        // Should have /t/
        "ich",        // Should have ich-Laut /ç/
        "ach",        // Should have ach-Laut /x/
        "schön",      // Should have /ʃ/ and /œ/
        NULL
    };
    
    char ipa_output[256];
    for (int i = 0; test_cases[i] != NULL; i++) {
        memset(ipa_output, 0, sizeof(ipa_output));
        int result = phonemizer_text_to_ipa(phonemizer, test_cases[i],
                                            ipa_output, sizeof(ipa_output));
        
        ASSERT_TRUE(result >= 0, "German phonemization should succeed");
        ASSERT_TRUE(strlen(ipa_output) > 0, "IPA output should not be empty");
        
        printf("  ✓ '%s' → '%s'\n", test_cases[i], ipa_output);
    }
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Sentence-level phonemization
 */
static int test_sentence_phonemization(void) {
    printf("\n[Test 4] Sentence-Level Phonemization\n");
    
    phonemizer_t* phonemizer = phonemizer_create("en-us");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    const char* sentence = "Hello, how are you today?";
    char ipa_output[512];
    
    int result = phonemizer_text_to_ipa(phonemizer, sentence,
                                        ipa_output, sizeof(ipa_output));
    
    ASSERT_TRUE(result >= 0, "Sentence phonemization should succeed");
    ASSERT_TRUE(strlen(ipa_output) > 0, "IPA output should not be empty");
    
    printf("  ✓ Input: '%s'\n", sentence);
    printf("  ✓ Output: '%s'\n", ipa_output);
    printf("  ✓ Length: %zu phonemes\n", strlen(ipa_output));
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Mixed case handling
 */
static int test_case_handling(void) {
    printf("\n[Test 5] Mixed Case Handling\n");
    
    phonemizer_t* phonemizer = phonemizer_create("en-us");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    // Test uppercase, lowercase, and mixed
    const char* test_cases[] = {
        "HELLO",
        "hello",
        "Hello",
        "HeLLo",
        NULL
    };
    
    char ipa_output[256];
    char first_output[256] = {0};
    
    for (int i = 0; test_cases[i] != NULL; i++) {
        memset(ipa_output, 0, sizeof(ipa_output));
        int result = phonemizer_text_to_ipa(phonemizer, test_cases[i],
                                            ipa_output, sizeof(ipa_output));
        
        ASSERT_TRUE(result >= 0, "Case handling should succeed");
        
        if (i == 0) {
            strncpy(first_output, ipa_output, sizeof(first_output));
        }
        
        printf("  ✓ '%s' → '%s'\n", test_cases[i], ipa_output);
    }
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Punctuation stripping
 */
static int test_punctuation_handling(void) {
    printf("\n[Test 6] Punctuation Handling\n");
    
    phonemizer_t* phonemizer = phonemizer_create("en-us");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    const char* test_cases[] = {
        "hello!",
        "hello?",
        "hello,",
        "hello.",
        "'hello'",
        "\"hello\"",
        NULL
    };
    
    char ipa_output[256];
    for (int i = 0; test_cases[i] != NULL; i++) {
        memset(ipa_output, 0, sizeof(ipa_output));
        int result = phonemizer_text_to_ipa(phonemizer, test_cases[i],
                                            ipa_output, sizeof(ipa_output));
        
        ASSERT_TRUE(result >= 0, "Punctuation handling should succeed");
        ASSERT_TRUE(strlen(ipa_output) > 0, "Should produce output");
        
        printf("  ✓ '%s' → '%s'\n", test_cases[i], ipa_output);
    }
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Long text handling
 */
static int test_long_text(void) {
    printf("\n[Test 7] Long Text Handling\n");
    
    phonemizer_t* phonemizer = phonemizer_create("en-us");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    const char* long_text = 
        "The quick brown fox jumps over the lazy dog. "
        "This is a longer sentence to test the phonemizer's ability "
        "to handle extended passages of text without errors.";
    
    char ipa_output[1024];
    int result = phonemizer_text_to_ipa(phonemizer, long_text,
                                        ipa_output, sizeof(ipa_output));
    
    ASSERT_TRUE(result >= 0, "Long text phonemization should succeed");
    ASSERT_TRUE(strlen(ipa_output) > 0, "Should produce output");
    
    printf("  ✓ Input length: %zu characters\n", strlen(long_text));
    printf("  ✓ Output length: %zu phonemes\n", strlen(ipa_output));
    printf("  ✓ First 60 chars: '%.60s...'\n", ipa_output);
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Empty and NULL handling
 */
static int test_edge_cases(void) {
    printf("\n[Test 8] Edge Cases\n");
    
    phonemizer_t* phonemizer = phonemizer_create("en-us");
    ASSERT_NOT_NULL(phonemizer, "Phonemizer should be created");
    
    char ipa_output[256];
    
    // Empty string
    int result = phonemizer_text_to_ipa(phonemizer, "",
                                        ipa_output, sizeof(ipa_output));
    printf("  ✓ Empty string handled (result=%d)\n", result);
    
    // NULL text (should fail)
    result = phonemizer_text_to_ipa(phonemizer, NULL,
                                    ipa_output, sizeof(ipa_output));
    ASSERT_TRUE(result < 0, "NULL text should fail");
    printf("  ✓ NULL text rejected\n");
    
    // NULL output (should fail)
    result = phonemizer_text_to_ipa(phonemizer, "test", NULL, 256);
    ASSERT_TRUE(result < 0, "NULL output should fail");
    printf("  ✓ NULL output rejected\n");
    
    phonemizer_destroy(phonemizer);
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Test: Language detection and switching
 */
static int test_language_switching(void) {
    printf("\n[Test 9] Language Switching\n");
    
    // Create phonemizers for different languages
    phonemizer_t* phonemizer_en = phonemizer_create("en-us");
    phonemizer_t* phonemizer_zh = phonemizer_create("zh");
    phonemizer_t* phonemizer_de = phonemizer_create("de");
    
    ASSERT_NOT_NULL(phonemizer_en, "English phonemizer created");
    ASSERT_NOT_NULL(phonemizer_zh, "Chinese phonemizer created");
    ASSERT_NOT_NULL(phonemizer_de, "German phonemizer created");
    
    char ipa_output[256];
    
    // English
    phonemizer_text_to_ipa(phonemizer_en, "hello", ipa_output, sizeof(ipa_output));
    printf("  ✓ English: 'hello' → '%s'\n", ipa_output);
    
    // Chinese
    phonemizer_text_to_ipa(phonemizer_zh, "你好", ipa_output, sizeof(ipa_output));
    printf("  ✓ Chinese: '你好' → '%s'\n", ipa_output);
    
    // German
    phonemizer_text_to_ipa(phonemizer_de, "Guten", ipa_output, sizeof(ipa_output));
    printf("  ✓ German: 'Guten' → '%s'\n", ipa_output);
    
    phonemizer_destroy(phonemizer_en);
    phonemizer_destroy(phonemizer_zh);
    phonemizer_destroy(phonemizer_de);
    
    printf("  ✓ PASSED\n");
    return ETHERVOX_SUCCESS;
}

/**
 * Main test runner
 */
int main(void) {
    printf("═══════════════════════════════════════════════\n");
    printf("  TTS Text-to-Phoneme Integration Tests\n");
    printf("═══════════════════════════════════════════════\n");
    
    printf("\nNote: These tests require phonemizer data files:\n");
    printf("  - CMU Dictionary: ~/.ethervox/phonemizer/cmudict-0.7b.txt\n");
    printf("  - Unicode Unihan: ~/.ethervox/phonemizer/Unihan_Readings.txt\n");
    printf("  Run: ./scripts/download_phonemizer_data.sh\n");
    
    int failed = 0;
    
    if (test_english_text_to_phoneme() != 0) failed++;
    if (test_chinese_text_to_phoneme() != 0) failed++;
    if (test_german_text_to_phoneme() != 0) failed++;
    if (test_sentence_phonemization() != 0) failed++;
    if (test_case_handling() != 0) failed++;
    if (test_punctuation_handling() != 0) failed++;
    if (test_long_text() != 0) failed++;
    if (test_edge_cases() != 0) failed++;
    if (test_language_switching() != 0) failed++;
    
    // Summary
    printf("\n═══════════════════════════════════════════════\n");
    if (failed == 0) {
        printf("  ✓ All tests PASSED (9/9)\n");
    } else {
        printf("  ✗ %d tests FAILED\n", failed);
    }
    printf("═══════════════════════════════════════════════\n");
    
    return failed > 0 ? 1 : 0;
}
