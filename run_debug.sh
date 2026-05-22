#!/bin/bash
# Script to debug the optimize_tool_prompts crash

echo "Running ethervoxai under lldb to catch crash..."
echo ""

# Create lldb command file
cat > /tmp/lldb_cmds.txt << 'EOF'
run
bt
thread backtrace all
frame variable
quit
EOF

# Run lldb with the commands
lldb -s /tmp/lldb_cmds.txt ./ethervoxai -- -engineering -testllm calculator 2>&1 | tee /tmp/crash_full.log

echo ""
echo "Full log saved to: /tmp/crash_full.log"
echo ""
echo "Stack trace (last 50 lines):"
tail -50 /tmp/crash_full.log
