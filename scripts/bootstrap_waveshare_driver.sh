#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${TMPDIR:-/tmp}/token-light-waveshare"
REPO="https://github.com/waveshareteam/ESP32-S3-RLCD-4.2.git"
COMMIT="cb590c853f3d2bb58efe280c98b2f3cbf1e2876e"

rm -rf "$WORK"
git clone --depth 1 "$REPO" "$WORK"
git -C "$WORK" fetch --depth 1 origin "$COMMIT"
git -C "$WORK" checkout "$COMMIT"

mkdir -p "$ROOT/firmware/src"
cp "$WORK/02_Example/Arduino/10_U8G2_Test/ST7305_U8g2.h" "$ROOT/firmware/src/ST7305_U8g2.h"
cp "$WORK/02_Example/Arduino/10_U8G2_Test/ST7305_U8g2.cpp" "$ROOT/firmware/src/ST7305_U8g2.cpp"

echo "Copied Waveshare ST7305_U8g2 driver at $COMMIT"
