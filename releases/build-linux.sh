#!/bin/bash
# Build script for Linux - Run on Linux machine

set -e

VERSION="2.0.1"
BUILD_DIR="build-linux"

echo "Building Kern v${VERSION} for Linux x64..."

# Install dependencies (Ubuntu/Debian)
# sudo apt-get update
# sudo apt-get install -y cmake build-essential libgl1-mesa-dev

# Clean build
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++

# Build
cmake --build . --config Release --target kern -j$(nproc)

# Package
mkdir -p "kern-v${VERSION}-linux-x64"
cp Release/kern "kern-v${VERSION}-linux-x64/"
cp ../kern_logo.ico "kern-v${VERSION}-linux-x64/" 2>/dev/null || true
cp ../releases/v2.0.1/README.md "kern-v${VERSION}-linux-x64/"

tar czf "../kern-v${VERSION}-linux-x64.tar.gz" "kern-v${VERSION}-linux-x64"
echo "Created: kern-v${VERSION}-linux-x64.tar.gz"

# Verify
file "kern-v${VERSION}-linux-x64/kern"
ldd "kern-v${VERSION}-linux-x64/kern" | head -5
