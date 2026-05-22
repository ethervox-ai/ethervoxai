#!/bin/bash
# Test script for conversation session commands

echo "Testing conversation session infrastructure..."
echo ""

# Start EthervoxAI and send test commands
{
    sleep 2
    echo "/conversation"
    sleep 1
    echo "/convon"
    sleep 1
    echo "/conversation"
    sleep 1
    echo "/convoff"
    sleep 1
    echo "/quit"
} | ./build/ethervoxai

echo ""
echo "✓ Conversation command test complete!"
