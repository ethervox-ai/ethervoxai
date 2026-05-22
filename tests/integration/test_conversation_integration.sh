#!/bin/bash
# Test conversation integration

echo "Starting EthervoxAI test..."
echo ""

# Start ethervoxai in background
./build/ethervoxai --noautoload --quiet > /tmp/ethervox_test.log 2>&1 &
PID=$!

sleep 2

echo "Sending commands to test conversation..."

# Load a model (use tiny for speed)
echo "/load ~/.ethervox/models/governor/granite-3.0-2b-instruct-Q4_K_M.gguf" | nc -U /tmp/ethervox.sock || echo "(skipping if model not found)"

sleep 1

# Enable conversation
echo "/convon"

# Enable wake word
echo "/wakeon"

# Manually trigger conversation for testing
echo "/convtrigger"

echo ""
echo "Test setup complete. You can:"
echo "  1. Say 'hey ethervox' to trigger via wake word"
echo "  2. Use /convtrigger to manually test"
echo "  3. Check logs in /tmp/ethervox_test.log"
echo ""
echo "To stop: kill $PID"
