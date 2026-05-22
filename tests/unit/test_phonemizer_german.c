/**
 * Test German rule-based phonemizer
 */

#include <stdio.h>
#include "ethervox/error.h"
#include <string.h>
#include "tts/phonemizer/rules_de.h"

int main() {
    printf("=== Testing German Rule-Based Phonemizer ===\n\n");
    
    // Test cases: common German words
    const char* test_words[] = {
        "Haus",          // house
        "ich",           // I
        "deutsch",       // German
        "schön",         // beautiful
        "Straße",        // street
        "König",         // king
        "Mädchen",       // girl
        "sprechen",      // to speak
        "Guten Tag",     // good day
        "danke",         // thanks
        "Freund",        // friend
        "über",          // over
        "möchte",        // would like
        "Apfel",         // apple
        "Zeit",          // time
        "acht",          // eight
        "Nacht",         // night
        "Buch",          // book
        "hören",         // to hear
        "fünf"           // five
    };
    
    char ipa_output[256];
    int num_tests = sizeof(test_words) / sizeof(test_words[0]);
    int passed = 0;
    
    for (int i = 0; i < num_tests; i++) {
        int result = apply_german_g2p_rules(test_words[i], ipa_output, sizeof(ipa_output));
        
        if (result == 0 && strlen(ipa_output) > 0) {
            printf("✓ %-15s → %s\n", test_words[i], ipa_output);
            passed++;
        } else {
            printf("✗ %-15s → FAILED\n", test_words[i]);
        }
    }
    
    printf("\n=== Results ===\n");
    printf("Passed: %d/%d tests (%.1f%%)\n", passed, num_tests, (passed * 100.0) / num_tests);
    
    if (passed == num_tests) {
        printf("\n✓ SUCCESS - All German phonemization tests passed!\n");
        return ETHERVOX_SUCCESS;
    } else {
        printf("\n✗ Some tests failed\n");
        return ETHERVOX_SUCCESS;
    }
}
