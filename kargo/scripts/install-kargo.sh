#!/usr/bin/env sh
# Install only Kargo (Node CLI). No Kern build.
# Defaults: ~/.local/lib/kargo (app) and ~/.local/bin/kargo (launcher).
#
# Env: KARGO_PREFIX (default ~/.local), KARGO_BINDIR (default ~/.local/bin),
#      KARGO_DEST (default $KARGO_PREFIX/lib/kargo), FORCE=1 to skip overwrite prompt.
#      KARGO_INSTALL_QUIET=1 — errors and prompts only
#      KARGO_INSTALL_MODE — auto | upgrade | reinstall | uninstall | cancel (for non-interactive / CI)
#
#   ./kargo/scripts/install-kargo.sh
#   FORCE=1 ./kargo/scripts/install-kargo.sh

set -eu
umask 022

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
KARGO_SRC=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
PREFIX="${KARGO_PREFIX:-$HOME/.local}"
BINDIR="${KARGO_BINDIR:-$HOME/.local/bin}"
DEST="${KARGO_DEST:-$PREFIX/lib/kargo}"
FORCE="${FORCE:-0}"

KARGO_LC_PFX=kargo-install
# shellcheck source=kargo-install-common.sh
. "$SCRIPT_DIR/kargo-install-common.sh"

die() {
  echo "kargo-install: error: $*" >&2
  exit 1
}

log() {
  test "${KARGO_INSTALL_QUIET:-}" = "1" && return 0
  echo "kargo-install: $*" >&2
}

step() {
  log "[$1] $2"
}

if ! command -v node >/dev/null 2>&1; then
  die "Node.js is not on PATH. Install Node 18.17+ from https://nodejs.org/"
fi

if ! node -e '
  const p = process.versions.node.split(".").map(Number);
  const ok = p[0] > 18 || (p[0] === 18 && (p[1] > 17 || (p[1] === 17 && p[2] >= 0)));
  process.exit(ok ? 0 : 1);
' 2>/dev/null; then
  die "Node $(node -p process.versions.node) is too old; need >= 18.17.0 (see kargo/package.json engines)."
fi

if ! command -v npm >/dev/null 2>&1; then
  die "npm is not on PATH; install Node.js (includes npm) or fix PATH."
fi
if ! npm --version >/dev/null 2>&1; then
  die "npm failed (npm --version). Repair your Node.js installation."
fi

if [ ! -f "$KARGO_SRC/package.json" ] || [ ! -f "$KARGO_SRC/cli/entry.js" ]; then
  die "expected kargo package at $KARGO_SRC (missing package.json or cli/entry.js)."
fi

case "${KARGO_INSTALL_MODE:-auto}" in
  auto|upgrade|reinstall|uninstall|cancel) ;;
  *) die "invalid KARGO_INSTALL_MODE=${KARGO_INSTALL_MODE:-} (use auto|upgrade|reinstall|uninstall|cancel)" ;;
esac

kargo_detect_state "$DEST" "$BINDIR/kargo"
if [ "${KARGO_INSTALL_MODE:-auto}" = "uninstall" ] && [ "$KARGO_STATE_ANY" != 1 ]; then
  echo "kargo-install: nothing to uninstall at $DEST (and no recognized launcher)." >&2
  exit 0
fi
if [ "$KARGO_STATE_ANY" = 1 ]; then
  kargo_resolve_lifecycle_choice "$DEST" "$BINDIR/kargo" "${KARGO_INSTALL_MODE:-auto}" 0
  case "$KARGO_CHOICE" in
    4) exit 0 ;;
    3)
      kargo_uninstall_posix "$DEST" "$BINDIR/kargo" || exit 0
      exit 0
      ;;
    1|2) FORCE=1 ;;
  esac
fi

log "installing from $KARGO_SRC"

step "1/3" "copying package and running npm install --omit=dev…"
mkdir -p "$BINDIR" "$(dirname "$DEST")"
rm -rf "$DEST"
mkdir -p "$DEST"
cp -R "$KARGO_SRC/." "$DEST/"

(
  cd "$DEST" || exit 1
  npm install --omit=dev
) || die "npm install --omit=dev failed in $DEST — fix network/cache and retry."

step "2/3" "writing launcher $BINDIR/kargo…"
cat >"$BINDIR/kargo" << EOF
#!/bin/sh
exec node "$DEST/cli/entry.js" "\$@"
EOF
chmod +x "$BINDIR/kargo"
chmod +x "$DEST/cli/entry.js" 2>/dev/null || true

step "3/3" "verifying kargo --version…"
if ! "$BINDIR/kargo" --version >/dev/null 2>&1; then
  die "smoke test failed: $BINDIR/kargo --version"
fi

ver=$("$BINDIR/kargo" --version 2>/dev/null || true)
log "done — Kargo $ver"
log "  application:  $DEST"
log "  launcher:     $BINDIR/kargo"
log "  next step:    ensure $BINDIR is on PATH (open a new shell if you just added it)."
