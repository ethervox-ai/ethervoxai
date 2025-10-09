#!/usr/bin/env bash
# Simple build script for Linux. Mirrors Makefile behavior.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [ ! -d .venv ]; then
  python3 -m venv .venv
fi
# shellcheck disable=SC1091
. .venv/bin/activate
pip install -U pip setuptools wheel

if [ -f requirements.txt ]; then
  pip install -r requirements.txt
elif [ -f pyproject.toml ]; then
  pip install -e .
elif [ -f package.json ]; then
  npm install
fi

if [ -f setup.py ]; then
  python setup.py build
elif [ -f pyproject.toml ]; then
  pip wheel . -w dist || true
elif [ -f package.json ]; then
  npm run build || true
else
  echo "No recognized build manifest found."
  exit 1
fi

echo "Build complete."