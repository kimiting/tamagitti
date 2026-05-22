#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <PORT>" >&2
  echo "Example: $0 /dev/cu.usbmodem1101" >&2
  exit 1
fi

PORT="$1"
BAUD="${BAUD:-1500000}"
FQBN="esp32:esp32:XIAO_ESP32C6"
ARDUINO_CLI="${ARDUINO_CLI:-$HOME/.local/bin/arduino-cli}"
ESPTOOL="${ESPTOOL:-$HOME/Library/Arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool}"

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
SKETCH_DIR="$(mktemp -d)/cat_tama"
BUILD_DIR="$(mktemp -d)"

mkdir -p "$SKETCH_DIR"
cp "$ROOT_DIR/cat_tama.ino" "$ROOT_DIR/pico_sprites.h" "$ROOT_DIR/images.h" "$SKETCH_DIR/"

"$ARDUINO_CLI" compile --fqbn "$FQBN" --build-path "$BUILD_DIR" "$SKETCH_DIR"

"$ESPTOOL" \
  --chip esp32c6 \
  --port "$PORT" \
  --baud "$BAUD" \
  --before default-reset \
  --after hard-reset \
  write-flash \
  -z \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size 4MB \
  0x10000 "$BUILD_DIR/cat_tama.ino.bin"
