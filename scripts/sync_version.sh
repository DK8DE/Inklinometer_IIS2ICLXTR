#!/usr/bin/env bash
# Schreibt die Version aus src/version.h in abhängige Textdateien (README, …).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$("${ROOT}/scripts/read_version.sh")"

# README: Marker <!--FW_VERSION-->…<!--/FW_VERSION-->
if [[ -f "${ROOT}/README.md" ]]; then
  if grep -q '<!--FW_VERSION-->' "${ROOT}/README.md"; then
    sed -i.bak -E "s|(<!--FW_VERSION-->)[^<]*(<!--/FW_VERSION-->)|\\1${VERSION}\\2|" "${ROOT}/README.md"
    rm -f "${ROOT}/README.md.bak"
  fi
fi

# Optional: VERSION-Datei für Tools (Inhalt immer aus version.h)
printf '%s\n' "$VERSION" > "${ROOT}/VERSION"

echo "Version synced: ${VERSION}"
