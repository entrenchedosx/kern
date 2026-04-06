#!/usr/bin/env sh
# Build and install Kern with user/global/portable modes.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
MODE="${MODE:-user}"
PREFIX="${PREFIX:-}"
ADD_TO_PATH="${ADD_TO_PATH:-auto}" # auto|yes|no
FORCE="${FORCE:-0}"

usage() {
  echo "Usage: ./install.sh [--mode user|global|portable] [--prefix <dir>] [--add-to-path|--no-path] [--force]"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --mode) MODE="$2"; shift 2 ;;
    --prefix) PREFIX="$2"; shift 2 ;;
    --add-to-path) ADD_TO_PATH="yes"; shift ;;
    --no-path) ADD_TO_PATH="no"; shift ;;
    --force) FORCE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1"; usage; exit 1 ;;
  esac
done

case "$MODE" in
  user|global|portable) ;;
  *) echo "Invalid --mode: $MODE"; exit 1 ;;
esac

if [ -z "$PREFIX" ]; then
  case "$MODE" in
    user) PREFIX="$HOME/.local/share/kern" ;;
    global) PREFIX="/usr/local/share/kern" ;;
    portable) PREFIX="$ROOT/.kern-portable" ;;
  esac
fi

if [ "$MODE" = "global" ] && [ "$(id -u)" -ne 0 ]; then
  echo "Global install requires root privileges. Re-run with sudo or use --mode user."
  exit 1
fi

TARGET_VERSION="unknown"
if [ -f "$ROOT/KERN_VERSION.txt" ]; then
  TARGET_VERSION="$(tr -d ' \n\r\t' < "$ROOT/KERN_VERSION.txt")"
fi

version_from_text() {
  echo "$1" | sed -n 's/.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' | head -n 1
}

semver_cmp() {
  # returns: -1 if $1<$2, 0 if ==, 1 if >
  a="$1"; b="$2"
  IFS=. set -- $a; a1="${1:-0}"; a2="${2:-0}"; a3="${3:-0}"
  IFS=. set -- $b; b1="${1:-0}"; b2="${2:-0}"; b3="${3:-0}"
  if [ "$a1" -lt "$b1" ]; then echo -1; return; fi
  if [ "$a1" -gt "$b1" ]; then echo 1; return; fi
  if [ "$a2" -lt "$b2" ]; then echo -1; return; fi
  if [ "$a2" -gt "$b2" ]; then echo 1; return; fi
  if [ "$a3" -lt "$b3" ]; then echo -1; return; fi
  if [ "$a3" -gt "$b3" ]; then echo 1; return; fi
  echo 0
}

echo "==> Kern installer"
echo "    mode: $MODE"
echo "    prefix: $PREFIX"
echo "    target version: $TARGET_VERSION"

if command -v kern >/dev/null 2>&1; then
  EXISTING_PATH="$(command -v kern)"
  EXISTING_RAW="$(kern --version 2>/dev/null || true)"
  EXISTING_VERSION="$(version_from_text "$EXISTING_RAW")"
  echo "==> Detected existing kern: $EXISTING_PATH"
  if [ -n "$EXISTING_VERSION" ] && [ -n "$TARGET_VERSION" ]; then
    CMP="$(semver_cmp "$EXISTING_VERSION" "$TARGET_VERSION")"
    echo "    installed version: $EXISTING_VERSION"
    if [ "$CMP" = "-1" ]; then
      echo "    recommendation: upgrade (installer has newer version)"
    elif [ "$CMP" = "1" ]; then
      echo "    recommendation: installed version is newer; this install will downgrade"
    else
      echo "    recommendation: same version detected; reinstall only if needed"
    fi
  else
    echo "    recommendation: unable to parse installed version; proceeding with install/update"
  fi
fi

echo "==> Configuring (CMAKE_BUILD_TYPE=Release)..."
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
echo "==> Building target: kern..."
cmake --build "$BUILD_DIR" --config Release --target kern

BUILT_BIN="$BUILD_DIR/kern"
if [ ! -f "$BUILT_BIN" ] && [ -f "$BUILD_DIR/Release/kern" ]; then
  BUILT_BIN="$BUILD_DIR/Release/kern"
fi
if [ ! -f "$BUILT_BIN" ]; then
  echo "Could not find built kern binary under $BUILD_DIR"
  exit 1
fi

VERSION_ROOT="$PREFIX/versions/$TARGET_VERSION"
ACTIVE_BIN_DIR="$PREFIX/bin"
ACTIVE_LIB_DIR="$PREFIX/lib/kern"
VERSION_BIN_DIR="$VERSION_ROOT/bin"
VERSION_LIB_DIR="$VERSION_ROOT/lib/kern"

mkdir -p "$ACTIVE_BIN_DIR" "$ACTIVE_LIB_DIR" "$VERSION_BIN_DIR" "$VERSION_LIB_DIR" "$PREFIX/packages" "$PREFIX/config" "$PREFIX/cache"

if [ -f "$ACTIVE_BIN_DIR/kern" ] && [ "$FORCE" -ne 1 ]; then
  printf "Active binary exists at %s. Overwrite? [y/N] " "$ACTIVE_BIN_DIR/kern"
  read -r ans
  case "$ans" in
    y|Y) ;;
    *) echo "Install cancelled."; exit 0 ;;
  esac
fi

cp -f "$BUILT_BIN" "$VERSION_BIN_DIR/kern"
cp -f "$BUILT_BIN" "$ACTIVE_BIN_DIR/kern"
chmod +x "$VERSION_BIN_DIR/kern" "$ACTIVE_BIN_DIR/kern"

if [ -d "$ROOT/lib/kern" ]; then
  cp -R "$ROOT/lib/kern/." "$ACTIVE_LIB_DIR/"
  cp -R "$ROOT/lib/kern/." "$VERSION_LIB_DIR/"
fi

if command -v sha256sum >/dev/null 2>&1; then
  SHA="$(sha256sum "$ACTIVE_BIN_DIR/kern" | awk '{print $1}')"
elif command -v shasum >/dev/null 2>&1; then
  SHA="$(shasum -a 256 "$ACTIVE_BIN_DIR/kern" | awk '{print $1}')"
else
  SHA="unavailable"
fi

case "$MODE" in
  user) PATH_BIN="$HOME/.local/bin" ;;
  global) PATH_BIN="/usr/local/bin" ;;
  portable) PATH_BIN="$ACTIVE_BIN_DIR" ;;
esac

mkdir -p "$PATH_BIN"
cp -f "$ACTIVE_BIN_DIR/kern" "$PATH_BIN/kern"
chmod +x "$PATH_BIN/kern"

SHOULD_ADD_PATH=0
if [ "$ADD_TO_PATH" = "yes" ]; then SHOULD_ADD_PATH=1; fi
if [ "$ADD_TO_PATH" = "auto" ] && [ "$MODE" != "portable" ]; then SHOULD_ADD_PATH=1; fi

if [ "$SHOULD_ADD_PATH" -eq 1 ]; then
  case ":$PATH:" in
    *":$PATH_BIN:"*) echo "PATH already contains $PATH_BIN" ;;
    *)
      if [ "$MODE" = "user" ]; then
        for rc in "$HOME/.bashrc" "$HOME/.zshrc"; do
          if [ -f "$rc" ] || [ ! -e "$rc" ]; then
            touch "$rc"
            if ! grep -Fq "# kern path" "$rc"; then
              {
                echo ""
                echo "# kern path"
                echo "export PATH=\"$PATH_BIN:\$PATH\""
              } >> "$rc"
              echo "Added Kern PATH to $rc"
            fi
          fi
        done
      else
        echo "PATH update skipped: $PATH_BIN is expected for $MODE installs."
      fi
      ;;
  esac
fi

echo ""
echo "Installed: $PATH_BIN/kern"
echo "SHA256: $SHA"
echo "Kern $TARGET_VERSION installed successfully."
echo "Next steps:"
echo "  - Open a new terminal and run: kern --version"
echo "  - Upgrade: rerun install.sh with new sources/version"
echo "  - Uninstall: remove '$PREFIX' and '$PATH_BIN/kern'"
