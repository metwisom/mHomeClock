#!/usr/bin/env bash
# Build (and optionally upload/monitor) the firmware via PlatformIO.
# Checks that PlatformIO is installed and installs it (via pip) if missing.
#
# Usage:
#   ./scripts/build.sh              build only
#   ./scripts/build.sh --upload     build and flash over USB
#   ./scripts/build.sh --monitor    build and open serial monitor after
#   ./scripts/build.sh --clean      clean build artifacts first
#   ./scripts/build.sh --upload --monitor
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

UPLOAD=0
MONITOR=0
CLEAN=0

for arg in "$@"; do
  case "$arg" in
    --upload) UPLOAD=1 ;;
    --monitor) MONITOR=1 ;;
    --clean) CLEAN=1 ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^#//'
      exit 0
      ;;
    *)
      echo "Unknown option: $arg" >&2
      exit 1
      ;;
  esac
done

find_python() {
  if command -v python3 >/dev/null 2>&1; then echo python3; return 0; fi
  if command -v python >/dev/null 2>&1; then echo python; return 0; fi
  return 1
}

find_pio() {
  if command -v pio >/dev/null 2>&1; then PIO=(pio); return 0; fi
  if command -v platformio >/dev/null 2>&1; then PIO=(platformio); return 0; fi
  if command -v python3 >/dev/null 2>&1 && python3 -m platformio --version >/dev/null 2>&1; then
    PIO=(python3 -m platformio); return 0
  fi
  if command -v python >/dev/null 2>&1 && python -m platformio --version >/dev/null 2>&1; then
    PIO=(python -m platformio); return 0
  fi
  return 1
}

echo "==> Checking dependencies"

if find_pio; then
  echo "PlatformIO found: ${PIO[*]} ($(${PIO[@]} --version 2>&1))"
else
  echo "PlatformIO not found, installing via pip..."
  PYBIN="$(find_python)" || {
    echo "Python not found. Install Python 3 first: https://www.python.org/downloads/" >&2
    exit 1
  }
  "$PYBIN" -m pip install -U platformio

  if ! find_pio; then
    echo "PlatformIO install failed." >&2
    exit 1
  fi
  echo "PlatformIO installed: ${PIO[*]} ($(${PIO[@]} --version 2>&1))"
fi

echo "==> Installing project dependencies (platform + libraries)"
"${PIO[@]}" pkg install

if [ "$CLEAN" -eq 1 ]; then
  echo "==> Cleaning"
  "${PIO[@]}" run --target clean
fi

echo "==> Building"
"${PIO[@]}" run

if [ "$UPLOAD" -eq 1 ]; then
  echo "==> Uploading"
  "${PIO[@]}" run --target upload
fi

if [ "$MONITOR" -eq 1 ]; then
  echo "==> Monitor (Ctrl+C to exit)"
  "${PIO[@]}" device monitor
fi
