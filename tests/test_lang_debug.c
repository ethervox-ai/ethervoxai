#include <stdio.h>
#include <string.h>
#include "ethervox/language_detector.h"

int main(int argc, char** argv) {
    const char* test_text = "Of course! Here's a short story for you: Once upon a time, in a small village nestled between rolling hills and a sparkling river, there lived a young girl named Lily.";
    
    if (argc > 1) {
        test_text = argv[1];
    }
    
    printf("Testing: %s\n", test_text);
    const char* result = ethervox_detect_language(test_text);
    printf("Detected language: %s\n", result);
    
    return 0;
}
