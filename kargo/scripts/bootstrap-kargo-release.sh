#!/usr/bin/env sh
# One-liner entrypoint: fetch install-from-release.sh + gh-release-resolve.mjs from GitHub, then run the verified install.
# No local clone required.
#
# Usage — pinned (recommended): KARGO_BOOTSTRAP_REF must match the ref in the raw URL (tag or commit),
# because this script cannot see its own URL when read from stdin:
#   TAG=v1.0.0
#   curl -fsSL "https://raw.githubusercontent.com/OWNER/REPO/${TAG}/kargo/scripts/bootstrap-kargo-release.sh" \
#     | KARGO_BOOTSTRAP_REF="$TAG" sh -s -- latest
#
# Mutable branch (convenience only; see WARNING at runtime):
#   curl -fsSL https://raw.githubusercontent.com/OWNER/REPO/main/kargo/scripts/bootstrap-kargo-release.sh | sh -s -- latest
# Fetches three files: install-from-release.sh, gh-release-resolve.mjs, kargo-install-common.sh
#
# Env (inherited by the child installer):
#   KARGO_RELEASE_REPO      GitHub owner/repo for releases API (default: entrenchedosx/kern)
#   KARGO_BOOTSTRAP_REF     Ref for install-from-release.sh + gh-release-resolve.mjs (default: main).
#   KARGO_BOOTSTRAP_NO_WARN Set to 1 to silence the mutable-ref warning (e.g. CI).
#   KARGO_STRICT_BOOTSTRAP  Set to 1 in child: require release tag to match KARGO_BOOTSTRAP_REF (see install-from-release.sh).
#   KARGO_CURL_EXTRA        Extra curl flags (same as install-from-release.sh)
#   KARGO_PREFIX, KARGO_BINDIR, GITHUB_TOKEN, FORCE, etc. — see install-from-release.sh
#
# Example (fork, pinned):
#   export KARGO_RELEASE_REPO=myfork/kern
#   TAG=v1.0.0
#   curl -fsSL "https://raw.githubusercontent.com/myfork/kern/${TAG}/kargo/scripts/bootstrap-kargo-release.sh" \
#     | KARGO_BOOTSTRAP_REF="$TAG" sh -s -- latest

set -eu
umask 022

REPO="${KARGO_RELEASE_REPO:-entrenchedosx/kern}"
REF="${KARGO_BOOTSTRAP_REF:-main}"
BASE="https://raw.githubusercontent.com/${REPO}/${REF}/kargo/scripts"

die() {
  echo "kargo-bootstrap: error: $*" >&2
  exit 1
}

log() {
  echo "kargo-bootstrap: $*" >&2
}

# shellcheck disable=SC2086
bootstrap_curl() {
  curl -fsSL \
    --retry 3 \
    --retry-delay 2 \
    --connect-timeout 30 \
    --max-time 300 \
    ${KARGO_CURL_EXTRA:-} \
    "$@"
}

warn_if_mutable_bootstrap_ref() {
  ref="$1"
  test "${KARGO_BOOTSTRAP_NO_WARN:-}" = "1" && return 0
  _lc=$(printf '%s' "$ref" | tr '[:upper:]' '[:lower:]')
  case "$_lc" in
    main|master|develop|dev|trunk) ;;
    *) return 0 ;;
  esac
  log "WARNING: mutable bootstrap ref '$ref' (branch may change)."
  log "WARNING: for reproducible installs use a tag or commit in raw.githubusercontent.com/.../REF/..."
}

warn_if_mutable_bootstrap_ref "$REF"

command -v curl >/dev/null 2>&1 || die "curl not on PATH"

log "fetching installer scripts from $BASE …"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

bootstrap_curl -o "$TMP/install-from-release.sh" "$BASE/install-from-release.sh" ||
  die "could not download install-from-release.sh"
bootstrap_curl -o "$TMP/gh-release-resolve.mjs" "$BASE/gh-release-resolve.mjs" ||
  die "could not download gh-release-resolve.mjs"
bootstrap_curl -o "$TMP/kargo-install-common.sh" "$BASE/kargo-install-common.sh" ||
  die "could not download kargo-install-common.sh"

test -s "$TMP/install-from-release.sh" || die "downloaded install-from-release.sh is empty"
test -s "$TMP/gh-release-resolve.mjs" || die "downloaded gh-release-resolve.mjs is empty"
test -s "$TMP/kargo-install-common.sh" || die "downloaded kargo-install-common.sh is empty"

case $(head -n 1 "$TMP/install-from-release.sh") in
  \#!*) ;;
  *) die "downloaded install-from-release.sh does not look like a shell script (wrong URL or HTML error page?)" ;;
esac

chmod +x "$TMP/install-from-release.sh"

export KARGO_BOOTSTRAP_REF="$REF"
log "starting release installer…"
sh "$TMP/install-from-release.sh" "$@"
exit $?
