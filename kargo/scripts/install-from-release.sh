#!/usr/bin/env sh
# Install Kargo from a GitHub Release (download → verify checksum → extract → launcher).
# Order is always verify before relying on extracted files.
#
# Requires: curl, node (>= 18.17), tar; sha256sum OR shasum for -c verification.
#
# Usage:
#   KARGO_RELEASE_REPO=owner/repo ./install-from-release.sh [latest|v1.0.0]
# Env:
#   KARGO_RELEASE_REPO   required unless you are in the upstream Kern repo (defaults below)
#   KARGO_PREFIX         install root (default ~/.local) — app goes to $KARGO_PREFIX/lib/kargo
#   KARGO_BINDIR         launcher directory (default ~/.local/bin)
#   GITHUB_TOKEN         optional, for API rate limits / private repos
#   FORCE=1              skip overwrite prompt for ~/.local/bin/kargo
#   KARGO_RESOLVER_JS    override path to gh-release-resolve.mjs (default: alongside this script)
#   KARGO_STRICT_BOOTSTRAP  Set to 1 to require KARGO_BOOTSTRAP_REF to equal the explicit release tag (not "latest").
#   KARGO_INSTALL_QUIET  Set to 1 for errors-only output on stderr
#   KARGO_CURL_EXTRA      Extra curl arguments (quoted string), e.g. proxy or IP version
#   KARGO_INSTALL_MODE    auto (default) | upgrade | reinstall | uninstall | cancel — non-interactive / CI
#
# Run from a checkout (needs kargo-install-common.sh alongside):
#   ./kargo/scripts/install-from-release.sh latest
# Or use bootstrap-kargo-release.sh (curl | sh), which downloads all three script files into a temp dir.

set -eu
umask 022

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
RESOLVER="${KARGO_RESOLVER_JS:-$SCRIPT_DIR/gh-release-resolve.mjs}"
VERSION_ARG="${1:-${KARGO_VERSION:-latest}}"

KARGO_RELEASE_REPO="${KARGO_RELEASE_REPO:-entrenchedosx/kern}"
PREFIX="${KARGO_PREFIX:-$HOME/.local}"
BINDIR="${KARGO_BINDIR:-$HOME/.local/bin}"
DEST="${KARGO_DEST:-$PREFIX/lib/kargo}"
FORCE="${FORCE:-0}"

KARGO_LC_PFX=kargo-install-release
# shellcheck source=kargo-install-common.sh
. "$SCRIPT_DIR/kargo-install-common.sh"

die() {
  echo "kargo-install-release: error: $*" >&2
  exit 1
}

log() {
  test "${KARGO_INSTALL_QUIET:-}" = "1" && return 0
  echo "kargo-install-release: $*" >&2
}

step() {
  log "[$1] $2"
}

# curl: retries, timeouts; optional KARGO_CURL_EXTRA (e.g. "-4" or proxy flags).
kargo_curl() {
  # shellcheck disable=SC2086
  curl -fsSL \
    --retry 3 \
    --retry-delay 2 \
    --connect-timeout 30 \
    --max-time 600 \
    ${KARGO_CURL_EXTRA:-} \
    "$@"
}

must_nonempty() {
  _f=$1
  _label=$2
  test -f "$_f" && test -s "$_f" || die "download missing or empty (${_label}) — check network, GitHub status, and retry"
}

test -f "$RESOLVER" || die "missing $RESOLVER (run from repo or copy both scripts to the same directory)."

command -v curl >/dev/null 2>&1 || die "curl not on PATH"
command -v node >/dev/null 2>&1 || die "node not on PATH"
command -v tar >/dev/null 2>&1 || die "tar not on PATH"

if ! node -e '
  const p = process.versions.node.split(".").map(Number);
  const ok = p[0] > 18 || (p[0] === 18 && (p[1] > 17 || (p[1] === 17 && p[2] >= 0)));
  process.exit(ok ? 0 : 1);
' 2>/dev/null; then
  die "Node $(node -p process.versions.node) is too old; need >= 18.17.0."
fi

case "${KARGO_INSTALL_MODE:-auto}" in
  auto|upgrade|reinstall|uninstall|cancel) ;;
  *) die "invalid KARGO_INSTALL_MODE=${KARGO_INSTALL_MODE:-} (use auto|upgrade|reinstall|uninstall|cancel)" ;;
esac

kargo_detect_state "$DEST" "$BINDIR/kargo"
if test "${KARGO_INSTALL_MODE:-auto}" = "uninstall" && test "$KARGO_STATE_ANY" != 1; then
  echo "kargo-install-release: nothing to uninstall at $DEST (and no recognized launcher)." >&2
  exit 0
fi
if test "$KARGO_STATE_ANY" = 1; then
  kargo_resolve_lifecycle_choice "$DEST" "$BINDIR/kargo" "${KARGO_INSTALL_MODE:-auto}" 1
  case "$KARGO_CHOICE" in
    4) exit 0 ;;
    3)
      kargo_uninstall_posix "$DEST" "$BINDIR/kargo" || exit 0
      exit 0
      ;;
    1) VERSION_ARG=latest ;;
    2) ;;
  esac
  FORCE=1
fi

if test "${KARGO_STRICT_BOOTSTRAP:-}" = "1"; then
  case "$VERSION_ARG" in
    latest)
      echo "kargo-install-release: ERROR: strict bootstrap alignment failed" >&2
      echo "kargo-install-release: Reason: release request is 'latest' (e.g. menu upgrade); strict mode needs a fixed tag." >&2
      echo "kargo-install-release: Fix: use reinstall with an explicit tag, or unset KARGO_STRICT_BOOTSTRAP." >&2
      exit 1
      ;;
  esac
  rel_norm="$VERSION_ARG"
  case "$rel_norm" in v*) ;; *) rel_norm="v$rel_norm" ;; esac
  br="${KARGO_BOOTSTRAP_REF:-}"
  if test -z "$br"; then
    echo "kargo-install-release: ERROR: strict bootstrap alignment failed" >&2
    echo "kargo-install-release: Expected:" >&2
    echo "kargo-install-release:   release tag:   $rel_norm" >&2
    echo "kargo-install-release:   bootstrap ref: $rel_norm  (KARGO_BOOTSTRAP_REF is unset)" >&2
    echo "kargo-install-release:" >&2
    echo "kargo-install-release: Fix: export KARGO_BOOTSTRAP_REF=$rel_norm  before running this installer," >&2
    echo "kargo-install-release:      or unset KARGO_STRICT_BOOTSTRAP." >&2
    echo "kargo-install-release: Example (curl bootstrap):" >&2
    printf 'kargo-install-release:   TAG=%s curl -fsSL "https://raw.githubusercontent.com/%s/${TAG}/kargo/scripts/bootstrap-kargo-release.sh" \\\n' \
      "$rel_norm" "$KARGO_RELEASE_REPO" >&2
    printf '%s\n' 'kargo-install-release:     | KARGO_BOOTSTRAP_REF="$TAG" KARGO_STRICT_BOOTSTRAP=1 sh -s -- "$TAG"' >&2
    exit 1
  fi
  if test "$br" != "$rel_norm"; then
    echo "kargo-install-release: ERROR: strict bootstrap alignment failed" >&2
    echo "kargo-install-release: Expected:" >&2
    echo "kargo-install-release:   release tag:   $rel_norm" >&2
    echo "kargo-install-release:   bootstrap ref: $rel_norm" >&2
    echo "kargo-install-release: Actual:" >&2
    echo "kargo-install-release:   KARGO_BOOTSTRAP_REF=$br" >&2
    echo "kargo-install-release:" >&2
    echo "kargo-install-release: Fix: set KARGO_BOOTSTRAP_REF=$rel_norm (match the tag in your raw.githubusercontent.com URL)," >&2
    echo "kargo-install-release:      or unset KARGO_STRICT_BOOTSTRAP." >&2
    echo "kargo-install-release: Example:" >&2
    printf 'kargo-install-release:   TAG=%s curl -fsSL "https://raw.githubusercontent.com/%s/${TAG}/kargo/scripts/bootstrap-kargo-release.sh" \\\n' \
      "$rel_norm" "$KARGO_RELEASE_REPO" >&2
    printf '%s\n' 'kargo-install-release:     | KARGO_BOOTSTRAP_REF="$TAG" KARGO_STRICT_BOOTSTRAP=1 sh -s -- "$TAG"' >&2
    exit 1
  fi
fi

log "installing Kargo from GitHub Releases ($KARGO_RELEASE_REPO, request: $VERSION_ARG)"

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

export KARGO_RELEASE_REPO
export KARGO_VERSION_REQUEST="$VERSION_ARG"
META_JSON="$WORKDIR/meta.json"

step "1/5" "resolving release assets (GitHub API)…"
if ! node "$RESOLVER" >"$META_JSON" 2>"$WORKDIR/resolve.err"; then
  if test -s "$WORKDIR/resolve.err"; then
    sed 's/^/kargo-install-release: /' "$WORKDIR/resolve.err" >&2
  fi
  die "could not resolve GitHub release assets (see messages above)"
fi

node -e "
const fs = require('fs');
const p = process.argv[1];
let j;
try {
  j = JSON.parse(fs.readFileSync(p, 'utf8'));
} catch {
  console.error('invalid JSON from resolver');
  process.exit(1);
}
for (const k of ['tag_name', 'tarball_name', 'tarball_url', 'sums_url']) {
  if (!j[k] || typeof j[k] !== 'string') {
    console.error('missing or invalid field: ' + k);
    process.exit(1);
  }
}
if (!/^kargo-v.+\\.tar\\.gz\$/.test(j.tarball_name)) {
  console.error('invalid tarball_name');
  process.exit(1);
}
" "$META_JSON" || die "resolver metadata failed validation (unexpected API response)"

TARBALL_NAME=$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1],'utf8')).tarball_name" "$META_JSON")
SUMS_URL=$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1],'utf8')).sums_url" "$META_JSON")
TB_URL=$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1],'utf8')).tarball_url" "$META_JSON")
MANIFEST_URL=$(node -p "try{JSON.parse(require('fs').readFileSync(process.argv[1],'utf8')).manifest_url||''}catch(e){''}" "$META_JSON")
REL_TAG=$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1],'utf8')).tag_name" "$META_JSON")

step "2/5" "downloading checksum file and tarball…"
kargo_curl -o "$WORKDIR/kargo-SHA256SUMS" "$SUMS_URL"
must_nonempty "$WORKDIR/kargo-SHA256SUMS" "kargo-SHA256SUMS"

kargo_curl -o "$WORKDIR/$TARBALL_NAME" "$TB_URL"
must_nonempty "$WORKDIR/$TARBALL_NAME" "$TARBALL_NAME"

if test -n "$MANIFEST_URL"; then
  if kargo_curl -o "$WORKDIR/kargo-release.json" "$MANIFEST_URL" 2>/dev/null && test -s "$WORKDIR/kargo-release.json"; then
    :
  else
    rm -f "$WORKDIR/kargo-release.json"
    log "warning: optional kargo-release.json not fetched; continuing with checksum verification only"
  fi
fi

step "3/5" "verifying SHA-256 (checksums must match before extract)…"
(
  cd "$WORKDIR" || exit 1
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum -c kargo-SHA256SUMS
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 -c kargo-SHA256SUMS
  else
    die "need sha256sum or shasum to verify checksums"
  fi
) || die "checksum verification failed — tarball not trusted; aborting install"

if test -f "$WORKDIR/kargo-release.json"; then
  export MANI_WORKDIR="$WORKDIR"
  export MANI_TARBALL="$TARBALL_NAME"
  node -e "
  const fs = require('fs');
  const path = require('path');
  const wd = process.env.MANI_WORKDIR;
  const tb = process.env.MANI_TARBALL;
  const m = JSON.parse(fs.readFileSync(path.join(wd, 'kargo-release.json'), 'utf8'));
  const buf = fs.readFileSync(path.join(wd, tb));
  const h = require('crypto').createHash('sha256').update(buf).digest('hex');
  if (m.sha256 && m.sha256 !== h) {
    console.error('kargo-install-release: manifest sha256 does not match tarball');
    process.exit(1);
  }
  " || die "kargo-release.json disagrees with tarball — aborting install"
fi

step "4/5" "validating archive structure…"
tar -tzf "$WORKDIR/$TARBALL_NAME" >/dev/null || die "tarball is not a readable gzip tar (corrupt download?)"

step "5/5" "installing under $DEST and launcher $BINDIR/kargo…"
mkdir -p "$(dirname "$DEST")"
rm -rf "$DEST"
tar -xzf "$WORKDIR/$TARBALL_NAME" -C "$(dirname "$DEST")"
EXTRACTED="$(dirname "$DEST")/$(basename "$TARBALL_NAME" .tar.gz)"
test -d "$EXTRACTED" || die "expected top-level directory in tarball matching $TARBALL_NAME"

mv "$EXTRACTED" "$DEST"

mkdir -p "$BINDIR"
cat >"$BINDIR/kargo" << EOF
#!/bin/sh
exec node "$DEST/cli/entry.js" "\$@"
EOF
chmod +x "$BINDIR/kargo"
chmod +x "$DEST/cli/entry.js" 2>/dev/null || true

if ! "$BINDIR/kargo" --version >/dev/null 2>&1; then
  die "smoke test failed: $BINDIR/kargo --version — check Node and paths"
fi

ver=$("$BINDIR/kargo" --version 2>/dev/null || true)
log "done — Kargo $ver (release $REL_TAG)"
log "  application:  $DEST"
log "  launcher:     $BINDIR/kargo"
log "  next step:    ensure $BINDIR is on PATH (open a new terminal if PATH was just updated)"
