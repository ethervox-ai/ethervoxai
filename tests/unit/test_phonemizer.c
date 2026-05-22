/**
 * @file test_phonemizer.c
 * @brief Test the English phonemizer
 */

#include "tts/phonemizer/phonemizer.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    const char* test_text = (argc > 1) ? argv[1] : "hello world";
    
    printf("=== Testing English Phonemizer ===\n");
    printf("Input text: \"%s\"\n\n", test_text);
    
    // Create phonemizer for English
    phonemizer_t* phonemizer = phonemizer_create("en-us");
    if (!phonemizer) {
        fprintf(stderr, "ERROR: Failed to create phonemizer\n");
        return ETHERVOX_SUCCESS;
    }
    
    // Convert text to IPA
    char ipa_output[4096];
    int result = phonemizer_text_to_ipa(phonemizer, test_text, ipa_output, sizeof(ipa_output));
    
    if (result == 0) {
        printf("SUCCESS!\n");
        printf("IPA output: \"%s\"\n", ipa_output);
    } else {
        printf("FAILED: phonemizer_text_to_ipa returned %d\n", result);
    }
    
    // Test a few common words
    const char* test_words[] = {
        "the",
        "quick",
        "brown",
        "fox",
        "jumps",
        NULL
    };
    
    printf("\n=== Testing individual words ===\n");
    for (int i = 0; test_words[i]; i++) {
        char ipa[256];
        if (phonemizer_text_to_ipa(phonemizer, test_words[i], ipa, sizeof(ipa)) == 0) {
            printf("%10s → %s\n", test_words[i], ipa);
        } else {
            printf("%10s → ERROR\n", test_words[i]);
        }
    }
    
    phonemizer_destroy(phonemizer);
    
    return (result == 0) ? 0 : 1;
}
