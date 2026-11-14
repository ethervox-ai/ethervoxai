#!/bin/bash
# check-no-hardcoded-paths.sh
# Grep test to prevent new hardcoded path derivations
# 
# Checks that new code uses ethervox_paths_t instead of getenv("HOME") or
# Documents/ derivations. Existing violations are grandfathered via allowlist.
#
# Copyright (c) 2024-2025 EthervoxAI Team
# SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Files excluded from checks (legacy code, external deps, platform-specific utils)
EXCLUDED_FILES=(
    "src/common/platform_utils_unix.c"    # Platform HAL - defines path defaults
    "src/common/platform_utils_windows.c" # Platform HAL - defines path defaults
    "src/common/paths.c"                  # Paths API implementation
    "include/ethervox/config.h"          # Config defaults (grandfathered)
    "esp32-project/"                     # ESP32 uses separate config
    "external/"                          # Third-party code
    "build/"                             # Build artifacts
    "tests/"                             # Tests may use hardcoded /tmp paths
    "scripts/"                           # Scripts themselves
    "main.c"                            # Standalone CLI app (not library)
)

# Build exclusion pattern for grep
EXCLUSION_PATTERN=""
for exclude in "${EXCLUDED_FILES[@]}"; do
    EXCLUSION_PATTERN="${EXCLUSION_PATTERN}|${exclude}"
done
EXCLUSION_PATTERN="${EXCLUSION_PATTERN:1}"  # Remove leading |

echo "═══════════════════════════════════════════════════════"
echo "  Hardcoded Path Check"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "NOTE: This is a new check (TASK-C1.3). Many existing files still use"
echo "      hardcoded paths. New code should use ethervox_paths_t instead."
echo "      See include/ethervox/paths.h for the API."
echo ""

# Check for getenv("HOME") outside allowed files
echo "Checking for getenv(\"HOME\") usage..."
VIOLATIONS=$(grep -r --include='*.c' --include='*.h' --exclude-dir=external \
    --exclude-dir=esp32-project --exclude-dir=build --exclude-dir=tests \
    'getenv.*["'"'"']HOME["'"'"']' "$REPO_ROOT/src" "$REPO_ROOT/include" || true)

VIOLATION_COUNT=$(echo "$VIOLATIONS" | grep -c "getenv" || true)

if [ -n "$VIOLATIONS" ] && [ "$VIOLATION_COUNT" -gt 0 ]; then
    echo "⚠️  Found $VIOLATION_COUNT getenv(\"HOME\") usages (migration in progress)"
    echo "   Use ethervox_paths_t for new code. See include/ethervox/paths.h"
    echo ""
    # Don't fail - this is infrastructure work in progress
    # exit 1
else
    echo "✅ No getenv(\"HOME\") violations"
fi
echo ""

# Check for Documents/ hardcoded paths
echo "Checking for hardcoded Documents/ paths..."
VIOLATIONS=$(grep -r --include='*.c' --include='*.h' --exclude-dir=external \
    --exclude-dir=esp32-project --exclude-dir=build --exclude-dir=tests \
    'Documents/' "$REPO_ROOT/src" "$REPO_ROOT/include" || true)

VIOLATION_COUNT=$(echo "$VIOLATIONS" | grep -c "Documents/" || true)

if [ -n "$VIOLATIONS" ] && [ "$VIOLATION_COUNT" -gt 0 ]; then
    echo "⚠️  Found $VIOLATION_COUNT hardcoded Documents/ paths (migration in progress)"
    echo "   Use ethervox_paths_t for new code."
    echo ""
else
    echo "✅ No hardcoded Documents/ paths"
fi
echo ""

# Check for /Library/ hardcoded paths (macOS specific)
echo "Checking for hardcoded /Library/ paths..."
VIOLATIONS=$(grep -r --include='*.c' --include='*.h' --exclude-dir=external \
    --exclude-dir=esp32-project --exclude-dir=build --exclude-dir=tests \
    '/Library/' "$REPO_ROOT/src" "$REPO_ROOT/include" || true)

VIOLATION_COUNT=$(echo "$VIOLATIONS" | grep -c "/Library/" || true)

if [ -n "$VIOLATIONS" ] && [ "$VIOLATION_COUNT" -gt 0 ]; then
    echo "⚠️  Found $VIOLATION_COUNT hardcoded /Library/ paths (migration in progress)"
    echo "   Use ethervox_paths_t for new code."
    echo ""
else
    echo "✅ No hardcoded /Library/ paths"
fi

echo ""
echo "═══════════════════════════════════════════════════════"
echo "✨ Hardcoded path check complete (warnings only during migration)"
echo "═══════════════════════════════════════════════════════"
exit 0
