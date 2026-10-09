#!/usr/bin/env bash
# Build + Upload aus einer zentralen Version (src/version.h).
#
# Nutzung:
#   ./build.sh              # sync → compile → upload (env esp32-c3)
#   ./build.sh --no-upload  # nur sync + compile
#   ./build.sh --sync-only  # nur Version in README/VERSION schreiben
#   UPLOAD_PORT=COM34 ./build.sh
#
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

ENV_NAME="${PIO_ENV:-esp32-c3}"
DO_UPLOAD=1
SYNC_ONLY=0

for arg in "$@"; do
  case "$arg" in
    --no-upload) DO_UPLOAD=0 ;;
    --sync-only) SYNC_ONLY=1; DO_UPLOAD=0 ;;
    -h|--help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
  esac
done

if ! command -v pio >/dev/null 2>&1; then
  echo "PlatformIO (pio) nicht im PATH." >&2
  exit 1
fi

chmod +x "${ROOT}/scripts/read_version.sh" "${ROOT}/scripts/sync_version.sh" 2>/dev/null || true
VERSION="$("${ROOT}/scripts/read_version.sh")"
echo "==> Firmware-Version: ${VERSION} (aus src/version.h)"

"${ROOT}/scripts/sync_version.sh"

if [[ "$SYNC_ONLY" -eq 1 ]]; then
  exit 0
fi

echo "==> Compile (${ENV_NAME})"
pio run -e "$ENV_NAME"

if [[ "$DO_UPLOAD" -eq 1 ]]; then
  echo "==> Upload (${ENV_NAME})"
  if [[ -n "${UPLOAD_PORT:-}" ]]; then
    pio run -e "$ENV_NAME" -t upload --upload-port "$UPLOAD_PORT"
  else
    pio run -e "$ENV_NAME" -t upload
  fi
  echo "==> Fertig: ${VERSION} auf MCU"
else
  echo "==> Fertig: ${VERSION} gebaut (kein Upload)"
fi
