#!/usr/bin/env bash
# Liest FW_VERSION_STR aus src/version.h (einzige Quelle).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER_FILE="${ROOT}/src/version.h"
if [[ ! -f "$VER_FILE" ]]; then
  echo "version.h nicht gefunden: $VER_FILE" >&2
  exit 1
fi
VER="$(grep -E '^\s*#define\s+FW_VERSION_STR\s+"' "$VER_FILE" | head -n1 | sed -E 's/.*"([^"]+)".*/\1/')"
if [[ -z "$VER" ]]; then
  echo "FW_VERSION_STR in version.h nicht gefunden" >&2
  exit 1
fi
echo "$VER"
