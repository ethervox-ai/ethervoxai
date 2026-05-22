#!/bin/bash
# Test script for model management system

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║         EthervoxAI Model Management Test Suite              ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if ethervoxai binary exists
if [ ! -f "./build/ethervoxai" ]; then
    echo "❌ Build not found. Run: cmake --build build"
    exit 1
fi

echo "✓ EthervoxAI binary found"
echo ""

# Test 1: List all models
echo "━━━ Test 1: List All Models ━━━"
./build/ethervoxai <<EOF
/models
/quit
EOF

echo ""
echo "━━━ Test 2: Check Whisper Status ━━━"
./build/ethervoxai <<EOF
/modelstatus whisper
/quit
EOF

echo ""
echo "━━━ Test 3: Check Vosk Status ━━━"
./build/ethervoxai <<EOF
/modelstatus vosk
/quit
EOF

echo ""
echo "━━━ Test 4: Check Governor Status ━━━"
./build/ethervoxai <<EOF
/modelstatus governor
/quit
EOF

echo ""
echo "━━━ Test 5: Check Piper Status ━━━"
./build/ethervoxai <<EOF
/modelstatus piper
/quit
EOF

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                    Test Suite Complete                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "To download models, run:"
echo "  ./build/ethervoxai"
echo "  > /modeldownload whisper ggml-base.en.bin"
echo ""
