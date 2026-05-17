#!/bin/bash
#
# Author  : Sylvain Deguire (VA2OPS)
# Date    : May 2026
# Purpose : Out-of-source build for QtGpsSync
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../QtGpsSync-build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
qmake "$SCRIPT_DIR/QtGpsSync.pro"
make -j$(nproc)

echo "Binary: $BUILD_DIR/QtGpsSync"
