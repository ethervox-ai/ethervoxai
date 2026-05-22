#!/bin/bash
# Interactive test for real-time Vosk STT with microphone

echo "======================================================"
echo "  Real-time Vosk STT Test with Microphone"
echo "======================================================"
echo ""
echo "This test will:"
echo "  1. Enable voice conversation"
echo "  2. Manually trigger conversation listening"
echo "  3. Capture audio from your microphone"
echo "  4. Process speech with Vosk STT in real-time"
echo ""
echo "Make sure you have a microphone connected!"
echo ""
read -p "Press ENTER to start..."

# Start EthervoxAI and send test sequence
{
    sleep 1
    echo "/convon"
    sleep 2
    echo "/convtrigger"
    sleep 7
    echo "/quit"
} | ./build/ethervoxai 2>&1 | tee /tmp/vosk_test_output.log

echo ""
echo "======================================================"
echo "Test complete! Check the output above for:"
echo "  ✓ 'Audio runtime initialized'"
echo "  ✓ 'Listening for speech'"
echo "  ✓ 'Partial:' - intermediate results"
echo "  ✓ 'Final recognition:' - complete transcription"
echo "======================================================"
echo ""
echo "Full output saved to: /tmp/vosk_test_output.log"
