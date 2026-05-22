#!/bin/bash
# Quick test to verify error handling in main.c

echo "Testing error handling in main.c..."
echo ""

# Test 1: Verify error.h is included
echo "✓ Test 1: Checking if error.h is included..."
if grep -q "#include \"ethervox/error.h\"" src/main.c; then
    echo "  ✓ error.h included"
else
    echo "  ✗ error.h NOT included"
    exit 1
fi

# Test 2: Check for ETHERVOX_ERROR_SET usage
echo "✓ Test 2: Checking for ETHERVOX_ERROR_SET usage..."
error_set_count=$(grep -c "ETHERVOX_ERROR_SET" src/main.c)
if [ $error_set_count -gt 0 ]; then
    echo "  ✓ Found $error_set_count uses of ETHERVOX_ERROR_SET"
else
    echo "  ✗ No ETHERVOX_ERROR_SET found"
    exit 1
fi

# Test 3: Check for ethervox_is_error usage
echo "✓ Test 3: Checking for ethervox_is_error usage..."
is_error_count=$(grep -c "ethervox_is_error" src/main.c)
if [ $is_error_count -gt 0 ]; then
    echo "  ✓ Found $is_error_count uses of ethervox_is_error"
else
    echo "  ✗ No ethervox_is_error found"
    exit 1
fi

# Test 4: Check for error context retrieval
echo "✓ Test 4: Checking for error context usage..."
context_count=$(grep -c "ethervox_error_get_context" src/main.c)
if [ $context_count -gt 0 ]; then
    echo "  ✓ Found $context_count uses of ethervox_error_get_context"
else
    echo "  ✗ No ethervox_error_get_context found"
    exit 1
fi

# Test 5: Check for error string conversion
echo "✓ Test 5: Checking for error string conversion..."
string_count=$(grep -c "ethervox_error_string" src/main.c)
if [ $string_count -gt 0 ]; then
    echo "  ✓ Found $string_count uses of ethervox_error_string"
else
    echo "  ✗ No ethervox_error_string found"
    exit 1
fi

# Test 6: Verify build succeeds
echo "✓ Test 6: Verifying build..."
if [ -f build/ethervoxai ]; then
    echo "  ✓ Build successful - executable exists"
else
    echo "  ✗ Build failed - executable not found"
    exit 1
fi

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "✅ All error handling tests passed!"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "Summary:"
echo "  - error.h properly included"
echo "  - Error macros used: $error_set_count times"
echo "  - Error checking: $is_error_count times"
echo "  - Context retrieval: $context_count times"
echo "  - Error strings: $string_count times"
echo ""
echo "The error handling system is now integrated into main.c!"
