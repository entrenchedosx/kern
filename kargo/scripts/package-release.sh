#!/usr/bin/env sh
# Build a distributable Kargo tree (Node + production node_modules), tarball, checksums, and manifest.
# Usage: ./package-release.sh <git-tag> [output-dir]
# Example (from repo root): ./kargo/scripts/package-release.sh v0.1.0 .
#
# Requires: node, npm on PATH. Writes:
#   <out>/kargo-<tag>.tar.gz
#   <out>/kargo-SHA256SUMS   (GNU sha256sum -c compatible)
#   <out>/kargo-release.json

set -eu
umask 022

TAG="${1:?usage: package-release.sh <git-tag> [output-dir]}"
OUT_DIR="${2:-.}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
KARGO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$KARGO_ROOT/.." && pwd)

die() {
  echo "package-release: error: $*" >&2
  exit 1
}

case "$TAG" in
  v*) ;;
  *) die "tag should look like v1.2.3 (got: $TAG)" ;;
esac

command -v node >/dev/null 2>&1 || die "node not on PATH"
command -v npm >/dev/null 2>&1 || die "npm not on PATH"

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

PKG_NAME="kargo-${TAG}"
STAGE_PKG="$STAGE/$PKG_NAME"
mkdir -p "$STAGE_PKG"

(
  cd "$KARGO_ROOT" || exit 1
  npm ci --omit=dev
)

cp -R "$KARGO_ROOT/cli" "$KARGO_ROOT/lib" "$STAGE_PKG/"
cp "$KARGO_ROOT/package.json" "$KARGO_ROOT/package-lock.json" "$STAGE_PKG/"
test -f "$KARGO_ROOT/README.md" && cp "$KARGO_ROOT/README.md" "$STAGE_PKG/"
test -f "$REPO_ROOT/LICENSE" && cp "$REPO_ROOT/LICENSE" "$STAGE_PKG/"

cp -R "$KARGO_ROOT/node_modules" "$STAGE_PKG/"
rm -rf "$STAGE_PKG/cli/tests"

chmod +x "$STAGE_PKG/cli/entry.js" 2>/dev/null || true

mkdir -p "$OUT_DIR"
TAR_BASE="kargo-${TAG}.tar.gz"
TAR_PATH="$OUT_DIR/$TAR_BASE"

tar -czf "$TAR_PATH" -C "$STAGE" "$PKG_NAME"

(
  cd "$OUT_DIR" || exit 1
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$TAR_BASE" >kargo-SHA256SUMS
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$TAR_BASE" >kargo-SHA256SUMS
  else
    die "need sha256sum or shasum"
  fi
)

PKG_VER=$(cd "$KARGO_ROOT" && node -p "require('./package.json').version")
HASH=$(awk '{print $1}' "$OUT_DIR/kargo-SHA256SUMS")
export TAG PKG_VER TAR_BASE HASH
MANIFEST="$OUT_DIR/kargo-release.json"
export MANIFEST

node -e "
const fs = require('fs');
const o = {
  schema: 1,
  git_tag: process.env.TAG,
  package_version: process.env.PKG_VER,
  tarball: process.env.TAR_BASE,
  sha256: process.env.HASH
};
fs.writeFileSync(process.env.MANIFEST, JSON.stringify(o, null, 2) + '\n');
"

echo "package-release: wrote $TAR_PATH"
echo "package-release: wrote $OUT_DIR/kargo-SHA256SUMS"
echo "package-release: wrote $OUT_DIR/kargo-release.json"
