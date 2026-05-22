#!/bin/bash
# End-to-end test: Wake word → Conversation → Speech recognition

echo "========================================================"
echo "  Wake Word + Voice Conversation Test"
echo "========================================================"
echo ""
echo "This test will:"
echo "  1. Enable voice conversation system"
echo "  2. Enable wake word detection"
echo "  3. Start microphone listening"
echo "  4. Wait for you to say 'hey ethervox'"
echo "  5. Capture your speech with Whisper STT"
echo ""
echo "Note: Wake word detection uses simple pattern matching"
echo "      (85-90% accuracy). Say clearly: 'hey ethervox'"
echo ""
read -p "Press ENTER to start (or Ctrl+C to cancel)..."

{
    sleep 1
    echo "/convon"
    sleep 2
    echo "/wakeon"
    sleep 1
    echo ""
    echo "╔═══════════════════════════════════════════════════╗"
    echo "║  🎤 Ready! Say: 'hey ethervox'                   ║"
    echo "║                                                   ║"
    echo "║  Then speak your question/command                ║"
    echo "║  (e.g., 'What time is it?')                      ║"
    echo "╚═══════════════════════════════════════════════════╝"
    echo ""
    
    # Wait for wake word + conversation (30 seconds)
    sleep 30
    
    echo "/quit"
} | ./build/ethervoxai 2>&1 | tee /tmp/wake_conversation_test.log

echo ""
echo "========================================================"
echo "Test complete! Check output above for:"
echo "  ✓ 'Wake word listening thread started'"
echo "  ✓ 'Microphone listening started'"
echo "  ✓ 'Wake word detected!' (when you say it)"
echo "  ✓ 'Listening for speech...' (after wake word)"
echo "  ✓ Whisper transcription of your speech"
echo "========================================================"
echo ""
echo "Full log: /tmp/wake_conversation_test.log"
