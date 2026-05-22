#!/bin/bash
# Test script to run tool prompt optimization

cd "$(dirname "$0")"

echo "Starting optimization test..."
echo "Press Ctrl+C within 10 seconds to test cancellation"
echo ""

# Send the command after a 2-second delay to let the program initialize
(sleep 2 && echo "/optimize_tool_prompts") | ./build/ethervoxai -engineering 2>&1 | tee /tmp/optimize_test.log

echo ""
echo "=== Test complete. Check /tmp/optimize_test.log for full output ==="
