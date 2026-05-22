/**
 * @file test_phonemizer_chinese.c
 * @brief Test Chinese phonemizer
 */

#include "tts/phonemizer/phonemizer.h"
#include "ethervox/error.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    const char* test_text = (argc > 1) ? argv[1] : "你好世界";
    
    printf("=== Testing Chinese Phonemizer ===\n");
    printf("Input text: \"%s\"\n\n", test_text);
    
    // Create phonemizer for Chinese
    phonemizer_t* phonemizer = phonemizer_create("zh-cn");
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
    
    // Test common phrases
    const char* test_phrases[] = {
        "你好",
        "世界",
        "中国",
        "早上好",
        NULL
    };
    
    printf("\n=== Testing common phrases ===\n");
    for (int i = 0; test_phrases[i]; i++) {
        char ipa[256];
        if (phonemizer_text_to_ipa(phonemizer, test_phrases[i], ipa, sizeof(ipa)) == 0) {
            printf("%s → %s\n", test_phrases[i], ipa);
        } else {
            printf("%s → ERROR\n", test_phrases[i]);
        }
    }
    
    phonemizer_destroy(phonemizer);
    
    return (result == 0) ? 0 : 1;
}
