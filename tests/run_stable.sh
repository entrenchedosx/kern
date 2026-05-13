#!/usr/bin/env bash
# Stable validation: contract binary, bytecode golden, then coverage (needs pwsh).
#   --quick              contract + golden only
#   --build-dir <name>   CMake output dir (default: build)
# Usage: bash tests/run_stable.sh [--quick] [--build-dir build-dbg]
set -euo pipefail

QUICK=0
BUILD_DIR="build"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --quick) QUICK=1; shift ;;
    --build-dir)
      [[ $# -ge 2 ]] || { echo "usage: $0 --build-dir <dir>" >&2; exit 1; }
      BUILD_DIR="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BD="$ROOT/$BUILD_DIR"

KERN=""
for c in "$BD/kern" "$BD/Release/kern.exe" "$BD/Debug/kern.exe"; do
  if [[ -f "$c" ]]; then KERN="$c"; break; fi
done
[[ -n "$KERN" ]] || { echo "kern not found under $BUILD_DIR/ (configure and build first)" >&2; exit 1; }

CONTRACT=""
for c in "$BD/kern_contract_humanize" "$BD/Release/kern_contract_humanize.exe" "$BD/Debug/kern_contract_humanize.exe"; do
  if [[ -f "$c" ]]; then CONTRACT="$c"; break; fi
done
[[ -n "$CONTRACT" ]] || { echo "kern_contract_humanize not found under $BUILD_DIR/" >&2; exit 1; }

echo "== kern_contract_humanize =="
"$CONTRACT"

echo "== bytecode golden =="
KN="$ROOT/examples/basic/01_hello_world.kn"
EXP="$ROOT/tests/coverage/bytecode_golden_hello_world.expected"
GOT="$(mktemp)"
trap 'rm -f "$GOT" "${GOT}.n" "${GOT}.e"' EXIT
if ! "$KERN" --bytecode-normalized "$KN" >"$GOT" 2>&1; then
  cat "$GOT" >&2
  exit 1
fi
norm() { sed 's/\r$//' | sed '/^[[:space:]]*$/d'; }
norm <"$GOT" >"${GOT}.n"
norm <"$EXP" >"${GOT}.e"
if ! cmp -s "${GOT}.e" "${GOT}.n"; then
  echo "GOLDEN MISMATCH" >&2
  echo "--- got (normalized) ---" >&2
  cat "${GOT}.n" >&2
  echo "--- expected (normalized) ---" >&2
  cat "${GOT}.e" >&2
  exit 1
fi
echo "bytecode golden OK"

if [[ "$QUICK" -eq 1 ]]; then
  echo "== (quick: skipping coverage) =="
  exit 0
fi

echo "== coverage / regression .kn =="
if command -v pwsh >/dev/null 2>&1; then
  exec pwsh -NoProfile -File "$ROOT/tests/run_all_coverage_kn.ps1" -Exe "$KERN"
elif command -v powershell >/dev/null 2>&1; then
  exec powershell -NoProfile -File "$ROOT/tests/run_all_coverage_kn.ps1" -Exe "$KERN"
else
  echo "error: install PowerShell (pwsh) to run tests/run_all_coverage_kn.ps1" >&2
  exit 1
fi
