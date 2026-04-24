#!/bin/bash
# Build script for macOS - Run on Mac machine

set -e

VERSION="2.0.1"
BUILD_DIR="build-macos"

echo "Building Kern v${VERSION} for macOS..."

# Clean build
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure (Universal binary: Intel + Apple Silicon)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

# Build
cmake --build . --config Release --target kern -j$(sysctl -n hw.ncpu)

# Package
mkdir -p "kern-v${VERSION}-macos-universal"
cp Release/kern "kern-v${VERSION}-macos-universal/"
cp ../kern_logo.ico "kern-v${VERSION}-macos-universal/" 2>/dev/null || true
cp ../releases/v2.0.1/README.md "kern-v${VERSION}-macos-universal/"

tar czf "../kern-v${VERSION}-macos-universal.tar.gz" "kern-v${VERSION}-macos-universal"
echo "Created: kern-v${VERSION}-macos-universal.tar.gz"

# Verify
file "kern-v${VERSION}-macos-universal/kern"
