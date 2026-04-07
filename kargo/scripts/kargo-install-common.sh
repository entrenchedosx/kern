#!/bin/sh
# Shared lifecycle helpers for Kargo POSIX installers (source this file; do not execute).
# Before sourcing, set: KARGO_LC_PFX (e.g. kargo-install or kargo-install-release)

kargo_lc_log() {
  test "${KARGO_INSTALL_QUIET:-}" = "1" && return 0
  echo "${KARGO_LC_PFX}: $*" >&2
}

kargo_launcher_is_ours_sh() {
  lf=$1
  test -f "$lf" || return 1
  h=$(head -n 1 "$lf" 2>/dev/null || true)
  case "$h" in
    '#!/bin/sh'|'#!/usr/bin/env sh'|'#!/bin/dash') ;;
    *) return 1 ;;
  esac
  grep -q 'cli/entry\.js' "$lf" 2>/dev/null
}

kargo_detect_state() {
  _dest=$1
  _launch=$2
  KARGO_STATE_APP=0
  KARGO_STATE_LAUNCHER=0
  KARGO_STATE_VER="(none)"
  if [ -f "$_dest/cli/entry.js" ] && [ -f "$_dest/package.json" ]; then
    KARGO_STATE_APP=1
    KARGO_STATE_VER=$(node "$_dest/cli/entry.js" --version 2>/dev/null || echo "unknown")
  fi
  if [ -f "$_launch" ] && kargo_launcher_is_ours_sh "$_launch"; then
    KARGO_STATE_LAUNCHER=1
  fi
  KARGO_STATE_ANY=0
  if [ "$KARGO_STATE_APP" = 1 ] || [ "$KARGO_STATE_LAUNCHER" = 1 ]; then
    KARGO_STATE_ANY=1
  fi
}

# Sets KARGO_CHOICE: 1=upgrade 2=reinstall 3=uninstall 4=cancel
# $3 = mode: auto|upgrade|reinstall|uninstall|cancel
# $4 = from_release: 1 = GitHub release installer (wording)
kargo_resolve_lifecycle_choice() {
  _dest=$1
  _launch=$2
  _mode=$3
  _rel=$4

  KARGO_CHOICE=
  case "$_mode" in
    upgrade) KARGO_CHOICE=1; return ;;
    reinstall) KARGO_CHOICE=2; return ;;
    uninstall) KARGO_CHOICE=3; return ;;
    cancel) KARGO_CHOICE=4; return ;;
    auto) ;;
    *)
      kargo_lc_log "invalid KARGO_INSTALL_MODE='$_mode' (use auto|upgrade|reinstall|uninstall|cancel)"
      KARGO_CHOICE=4
      return
      ;;
  esac

  if [ ! -t 0 ]; then
    kargo_lc_log "non-interactive: existing install detected — upgrading (replace install). Set KARGO_INSTALL_MODE to control."
    KARGO_CHOICE=1
    return
  fi

  kargo_lc_log "Existing Kargo installation detected."
  kargo_lc_log "  Version:      $KARGO_STATE_VER"
  kargo_lc_log "  Application:  $_dest"
  if [ "$KARGO_STATE_LAUNCHER" = 1 ]; then
    kargo_lc_log "  Launcher:     $_launch (this installer)"
  else
    kargo_lc_log "  Launcher:     $_launch (missing or not from this installer — reinstall will recreate if needed)"
  fi
  echo "${KARGO_LC_PFX}:" >&2
  echo "${KARGO_LC_PFX}: What would you like to do?" >&2
  if [ "$_rel" = 1 ]; then
    echo "${KARGO_LC_PFX}:   [1] Upgrade   — fetch and install latest GitHub release (~/.kargo kept)" >&2
    echo "${KARGO_LC_PFX}:   [2] Reinstall — remove install tree, then install the version you request again" >&2
  else
    echo "${KARGO_LC_PFX}:   [1] Upgrade   — replace from this tree (~/.kargo kept)" >&2
    echo "${KARGO_LC_PFX}:   [2] Reinstall — remove install tree, then fresh copy from this tree" >&2
  fi
  echo "${KARGO_LC_PFX}:   [3] Uninstall — remove application directory and our launcher" >&2
  echo "${KARGO_LC_PFX}:   [4] Cancel" >&2
  printf "${KARGO_LC_PFX}: Enter choice [1-4]: " >&2
  read -r _c
  case "$_c" in
    1) KARGO_CHOICE=1 ;;
    2) KARGO_CHOICE=2 ;;
    3) KARGO_CHOICE=3 ;;
    4) KARGO_CHOICE=4 ;;
    *)
      kargo_lc_log "invalid choice; exiting."
      KARGO_CHOICE=4
      ;;
  esac
}

# Returns 0 if uninstalled, 1 if user aborted confirmation
kargo_uninstall_posix() {
  _dest=$1
  _launch=$2

  kargo_lc_log "This will delete:"
  if [ -d "$_dest" ]; then
    kargo_lc_log "  $_dest"
  else
    kargo_lc_log "  (application directory already absent)"
  fi
  if [ -f "$_launch" ] && kargo_launcher_is_ours_sh "$_launch"; then
    kargo_lc_log "  $_launch"
  elif [ -f "$_launch" ]; then
    kargo_lc_log "  (launcher $_launch is not from this installer — will not remove)"
  fi
  printf "${KARGO_LC_PFX}: Are you sure? [y/N] " >&2
  read -r _a
  case "$_a" in
    y|Y) ;;
    *)
      kargo_lc_log "uninstall cancelled."
      return 1
      ;;
  esac

  rm -rf "$_dest"
  if [ -f "$_launch" ] && kargo_launcher_is_ours_sh "$_launch"; then
    rm -f "$_launch"
  fi

  _kd="$HOME/.kargo"
  if [ -d "$_kd" ]; then
    printf "${KARGO_LC_PFX}: Remove ~/.kargo (config + cached packages)? [y/N] " >&2
    read -r _b
    case "$_b" in
      y|Y)
        rm -rf "$_kd"
        kargo_lc_log "removed $_kd"
        ;;
      *) kargo_lc_log "left ~/.kargo in place" ;;
    esac
  fi
  kargo_lc_log "uninstall complete."
  return 0
}
