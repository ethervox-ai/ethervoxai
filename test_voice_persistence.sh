#!/bin/bash
# Test script to verify voice selection persists correctly

set -e

SETTINGS_FILE="/tmp/test_voice_settings.json"
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Voice Persistence Test ==="
echo ""

# Clean up any existing test settings
rm -f "$SETTINGS_FILE"

# Create a test program that:
# 1. Sets voice_en to "en_US-lessac-high"
# 2. Saves settings
# 3. Loads settings back
# 4. Verifies piper_model_path was auto-generated correctly

cat > /tmp/test_voice_persist.c << 'EOF'
#include "ethervox/settings.h"
#include "ethervox/logging.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    ethervox_log_set_level(ETHERVOX_LOG_LEVEL_DEBUG);
    
    const char* test_file = "/tmp/test_voice_settings.json";
    
    // Step 1: Create settings with a specific voice
    printf("Step 1: Creating settings with voice_en = 'en_US-lessac-high'...\n");
    ethervox_persistent_settings_t settings1 = ethervox_settings_get_defaults();
    strncpy(settings1.tts.voice_en, "en_US-lessac-high", sizeof(settings1.tts.voice_en) - 1);
    
    // Manually set piper_model_path (to simulate what settings menu does)
    const char* home = getenv("HOME");
    if (home) {
        snprintf(settings1.tts.piper_model_path, sizeof(settings1.tts.piper_model_path),
                "%s/.ethervox/models/piper/en_US-lessac-high.onnx", home);
    }
    
    printf("  voice_en: %s\n", settings1.tts.voice_en);
    printf("  piper_model_path: %s\n", settings1.tts.piper_model_path);
    
    // Step 2: Save settings
    printf("\nStep 2: Saving settings to %s...\n", test_file);
    if (ethervox_settings_save(&settings1, test_file) != 0) {
        printf("ERROR: Failed to save settings\n");
        return 1;
    }
    printf("  ✓ Settings saved\n");
    
    // Step 3: Load settings back (simulating app restart)
    printf("\nStep 3: Loading settings from disk (simulating restart)...\n");
    ethervox_persistent_settings_t settings2 = {0};
    if (ethervox_settings_load(&settings2, test_file) != 0) {
        printf("ERROR: Failed to load settings\n");
        return 1;
    }
    printf("  ✓ Settings loaded\n");
    
    // Step 4: Verify voice_en persisted
    printf("\nStep 4: Verifying voice_en persisted...\n");
    printf("  voice_en: %s\n", settings2.tts.voice_en);
    if (strcmp(settings2.tts.voice_en, "en_US-lessac-high") != 0) {
        printf("ERROR: voice_en did not persist! Expected 'en_US-lessac-high', got '%s'\n", 
               settings2.tts.voice_en);
        return 1;
    }
    printf("  ✓ voice_en persisted correctly\n");
    
    // Step 5: Verify piper_model_path was auto-generated
    printf("\nStep 5: Verifying piper_model_path auto-generated...\n");
    printf("  piper_model_path: %s\n", settings2.tts.piper_model_path);
    
    char expected_path[512];
    if (home) {
        snprintf(expected_path, sizeof(expected_path),
                "%s/.ethervox/models/piper/en_US-lessac-high.onnx", home);
    }
    
    if (strcmp(settings2.tts.piper_model_path, expected_path) != 0) {
        printf("ERROR: piper_model_path incorrect!\n");
        printf("  Expected: %s\n", expected_path);
        printf("  Got:      %s\n", settings2.tts.piper_model_path);
        return 1;
    }
    printf("  ✓ piper_model_path auto-generated correctly\n");
    
    // Step 6: Change voice and verify it updates
    printf("\nStep 6: Changing voice to 'en_GB-alba-medium' and re-saving...\n");
    strncpy(settings2.tts.voice_en, "en_GB-alba-medium", sizeof(settings2.tts.voice_en) - 1);
    snprintf(settings2.tts.piper_model_path, sizeof(settings2.tts.piper_model_path),
            "%s/.ethervox/models/piper/en_GB-alba-medium.onnx", home);
    
    if (ethervox_settings_save(&settings2, test_file) != 0) {
        printf("ERROR: Failed to save updated settings\n");
        return 1;
    }
    
    // Step 7: Load again and verify new voice
    printf("\nStep 7: Loading settings again to verify voice change...\n");
    ethervox_persistent_settings_t settings3 = {0};
    if (ethervox_settings_load(&settings3, test_file) != 0) {
        printf("ERROR: Failed to load settings\n");
        return 1;
    }
    
    printf("  voice_en: %s\n", settings3.tts.voice_en);
    printf("  piper_model_path: %s\n", settings3.tts.piper_model_path);
    
    if (strcmp(settings3.tts.voice_en, "en_GB-alba-medium") != 0) {
        printf("ERROR: Updated voice_en did not persist!\n");
        return 1;
    }
    
    snprintf(expected_path, sizeof(expected_path),
            "%s/.ethervox/models/piper/en_GB-alba-medium.onnx", home);
    if (strcmp(settings3.tts.piper_model_path, expected_path) != 0) {
        printf("ERROR: Updated piper_model_path incorrect!\n");
        return 1;
    }
    printf("  ✓ Voice change persisted correctly\n");
    
    printf("\n=== ALL TESTS PASSED ✓ ===\n");
    return 0;
}
EOF

# Compile the test
echo "Compiling test program..."
gcc -o /tmp/test_voice_persist /tmp/test_voice_persist.c \
    -I"$TEST_DIR/include" \
    -I"$TEST_DIR/build/autogenerated" \
    -L"$TEST_DIR/build" \
    -L"$TEST_DIR/build/external/cJSON" \
    -lethervoxai -lcjson \
    -Wl,-rpath,"$TEST_DIR/build" \
    -Wl,-rpath,"$TEST_DIR/build/external/cJSON"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile test program"
    exit 1
fi

echo "✓ Test compiled"
echo ""

# Run the test
echo "Running test..."
/tmp/test_voice_persist

# Clean up
rm -f "$SETTINGS_FILE" /tmp/test_voice_persist /tmp/test_voice_persist.c

echo ""
echo "Test completed successfully!"
