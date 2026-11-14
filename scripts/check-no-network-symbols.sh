#!/usr/bin/env bash
# Fails if the given static library references any network symbol. Intended to be run
# against a build configured with every network feature flag OFF (TASK-C1.0, AGENTS.md I1).
#
# Usage: scripts/check-no-network-symbols.sh <path-to-libethervoxai.a>

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <path-to-libethervoxai.a>" >&2
  exit 2
fi

LIB_PATH="$1"

if [[ ! -f "$LIB_PATH" ]]; then
  echo "error: library not found: $LIB_PATH" >&2
  exit 2
fi

if ! command -v nm >/dev/null 2>&1; then
  echo "error: nm not found on PATH" >&2
  exit 2
fi

# Undefined symbols only: a static archive never *defines* socket()/curl_easy_init(), it only
# references them if a network-capable translation unit was linked in. Defined symbols with
# these names would be false positives; -u restricts to undefined ones.
FORBIDDEN_PATTERN='(^|[^A-Za-z0-9_])(socket|connect|getaddrinfo|gethostbyname|curl_[A-Za-z_]+)$'
FORBIDDEN_SUBSTRING_PATTERN='CFNetwork|NWConnection|CFSocket'

MATCHES="$(nm -u "$LIB_PATH" 2>/dev/null \
  | sed -E 's/^_//' \
  | grep -E "${FORBIDDEN_PATTERN}|${FORBIDDEN_SUBSTRING_PATTERN}" || true)"

if [[ -n "$MATCHES" ]]; then
  echo "error: network symbols found in $LIB_PATH:" >&2
  echo "$MATCHES" >&2
  exit 1
fi

echo "clean: no network symbols in $LIB_PATH"
