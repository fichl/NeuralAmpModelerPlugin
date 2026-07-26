#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-linux"
OUT_DIR="$BUILD_DIR/out"

PLUGIN_NAME="$(sed -n 's/^#define BUNDLE_NAME "\(.*\)".*$/\1/p' "$PROJECT_DIR/config.h" | tr -d '\r' | head -n 1)"
VERSION="$(sed -n 's/^#define PLUG_VERSION_STR "\(.*\)".*$/\1/p' "$PROJECT_DIR/config.h" | tr -d '\r' | head -n 1)"

if [[ -z "$PLUGIN_NAME" || -z "$VERSION" ]]; then
  echo "Could not read plugin name/version from config.h" >&2
  exit 1
fi

rm -rf "$BUILD_DIR"
mkdir -p "$OUT_DIR"

cmake -S "$PROJECT_DIR/projects/linux-vst3" -B "$BUILD_DIR/cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR/cmake" --config Release --parallel

VST3_DIR="$BUILD_DIR/cmake/VST3/Release/${PLUGIN_NAME}.vst3"
if [[ ! -d "$VST3_DIR" ]]; then
  echo "Expected VST3 bundle not found: $VST3_DIR" >&2
  find "$BUILD_DIR/cmake" -maxdepth 5 -type d -name "*.vst3" -print >&2
  exit 1
fi

ARCHIVE_NAME="${PLUGIN_NAME}-v${VERSION}-linux-vst3"
cp -R "$VST3_DIR" "$OUT_DIR/"
(
  cd "$OUT_DIR"
  zip -qry "${ARCHIVE_NAME}.zip" "${PLUGIN_NAME}.vst3"
)

echo "$OUT_DIR/${ARCHIVE_NAME}.zip"
