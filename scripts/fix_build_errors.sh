#!/usr/bin/env bash
set -euo pipefail

echo "Checking for duplicate type definitions..."

# Check if ethervox_llm_response_t is defined in multiple places
echo "Searching for ethervox_llm_response_t definitions..."
grep -n "^} ethervox_llm_response_t;" include/ethervox/*.h || true

echo "Build error fix script complete. Please review the output above."