from __future__ import annotations

import argparse
import sys
from pathlib import Path

from pe_imports import PEFormatError, iter_exes, list_imported_dlls


SYSTEM_DLL_PREFIXES = (
    "api-ms-win-",
    "ext-ms-win-",
)

SYSTEM_DLLS = {
    # core
    "ntdll.dll",
    "kernel32.dll",
    "kernelbase.dll",
    "user32.dll",
    "gdi32.dll",
    "gdi32full.dll",
    "advapi32.dll",
    "shell32.dll",
    "ole32.dll",
    "oleaut32.dll",
    "shlwapi.dll",
    "comdlg32.dll",
    "comctl32.dll",
    "sechost.dll",
    "rpcrt4.dll",
    "bcrypt.dll",
    "bcryptprimitives.dll",
    "crypt32.dll",
    "ws2_32.dll",
    "winhttp.dll",
    "wininet.dll",
    "iphlpapi.dll",
    "imm32.dll",
    "msvcrt.dll",
    "ucrtbase.dll",
    # graphics/system
    "opengl32.dll",
    "dwmapi.dll",
    "uxtheme.dll",
    "version.dll",
    "cfgmgr32.dll",
    "setupapi.dll",
    "hid.dll",
    "winmm.dll",
}


def is_system_dll(name: str) -> bool:
    n = name.lower()
    if n in SYSTEM_DLLS:
        return True
    return any(n.startswith(p) for p in SYSTEM_DLL_PREFIXES)


def main() -> int:
    ap = argparse.ArgumentParser(description="Verify shareable executables have self-contained DLL deps.")
    ap.add_argument(
        "--roots",
        nargs="+",
        required=True,
        help="One or more directories/files to scan (recursively for .exe).",
    )
    ap.add_argument(
        "--allow-dll",
        action="append",
        default=[],
        help="Extra DLL names to treat as system/allowed (repeatable).",
    )
    args = ap.parse_args()

    roots = [Path(x) for x in args.roots]
    exes = iter_exes(roots)
    if not exes:
        print("FAIL: no .exe files found under provided roots", file=sys.stderr)
        return 2

    extra_allowed = {x.lower() for x in args.allow_dll}

    failures: list[str] = []
    for exe in exes:
        try:
            dlls = list_imported_dlls(exe)
        except PEFormatError as e:
            failures.append(f"{exe}: PE parse error: {e}")
            continue
        except Exception as e:
            failures.append(f"{exe}: error reading imports: {e}")
            continue

        exe_dir = exe.parent
        missing: list[str] = []
        for d in dlls:
            if is_system_dll(d) or d in extra_allowed:
                continue
            # Must be shipped next to the exe (portable drop rule).
            if not (exe_dir / d).exists() and not (exe_dir / d.upper()).exists() and not (exe_dir / d.capitalize()).exists():
                missing.append(d)
        if missing:
            failures.append(f"{exe}: missing non-system DLLs: {', '.join(missing)}")

    if failures:
        print("FAIL: shareable dependency check failed", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1

    print(f"OK: checked {len(exes)} executables")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

