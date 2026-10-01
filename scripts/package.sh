#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
VERSION="$(tr -d '\r\n' < "$ROOT/VERSION.txt")"
PACKAGE_ROOT="$ROOT/dist/SolarisSilverline-$VERSION-linux-x64"
rm -rf "$PACKAGE_ROOT"
mkdir -p "$PACKAGE_ROOT/VST3" "$PACKAGE_ROOT/Standalone"
VST3="$(find "$BUILD_DIR" -type d -name 'Solaris Silverline.vst3' -print -quit)"
STANDALONE="$(find "$BUILD_DIR" -type f -name 'Solaris Silverline' -perm -111 -print -quit)"
[[ -n "$VST3" ]] || { echo "VST3 artifact not found under $BUILD_DIR" >&2; exit 2; }
[[ -n "$STANDALONE" ]] || { echo "Standalone artifact not found under $BUILD_DIR" >&2; exit 3; }
cp -a "$VST3" "$PACKAGE_ROOT/VST3/"
cp -a "$STANDALONE" "$PACKAGE_ROOT/Standalone/"
cp "$ROOT/LICENSE.txt" "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/VERSION.txt" "$PACKAGE_ROOT/"
echo "Package: $PACKAGE_ROOT"
