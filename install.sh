#!/usr/bin/env sh
# Build Kern and install the `kern` binary into a prefix (default: ~/.local).
# Does not duplicate PATH entries if your shell profile already adds ~/.local/bin.

set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
PREFIX="${PREFIX:-$HOME/.local}"

echo "==> Configuring (CMAKE_BUILD_TYPE=Release)..."
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building target: kern..."
cmake --build "$BUILD_DIR" --config Release --target kern

echo "==> Installing to prefix: $PREFIX ..."
cmake --install "$BUILD_DIR" --prefix "$PREFIX" --config Release

BINDIR="$PREFIX/bin"
echo ""
echo "Installed: $BINDIR/kern"
echo "Add to PATH if needed, then run: kern --version"
case ":$PATH:" in
  *":$BINDIR:"*) echo "PATH already contains $BINDIR" ;;
  *) echo "Example: export PATH=\"$BINDIR:\$PATH\"" ;;
esac
