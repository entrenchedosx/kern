from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STDLIB = ROOT / "src" / "stdlib_modules.cpp"


def _read(p: Path) -> str:
    try:
        return p.read_text(encoding="utf-8", errors="replace")
    except Exception as e:
        raise SystemExit(f"error: failed to read {p}: {e}")


def parse_modules_keys(src: str) -> set[str]:
    # very lightweight parse: { "name", { ... } }
    m = re.search(r"static\s+const\s+std::unordered_map<\s*std::string\s*,\s*std::vector<\s*std::string\s*>\s*>\s+MODULES\s*=\s*\{(?P<body>[\s\S]*?)\n\s*\};",
                  src)
    if not m:
        raise SystemExit("error: could not find MODULES map in src/stdlib_modules.cpp")
    body = m.group("body")
    keys = set(re.findall(r'\{\s*"([^"]+)"\s*,\s*\{', body))
    return keys


def parse_is_stdlib_names(src: str) -> set[str]:
    # parse: return name == "math" || name == "string" || ...
    m = re.search(r"bool\s+isStdlibModuleName\s*\(\s*const\s+std::string&\s+name\s*\)\s*\{(?P<body>[\s\S]*?)\n\s*\}",
                  src)
    if not m:
        raise SystemExit("error: could not find isStdlibModuleName() in src/stdlib_modules.cpp")
    body = m.group("body")
    names = set(re.findall(r'name\s*==\s*"([^"]+)"', body))
    return names


def main() -> int:
    src = _read(STDLIB)
    modules = parse_modules_keys(src)
    predicate = parse_is_stdlib_names(src)

    extra_in_pred = sorted(predicate - modules)
    missing_in_pred = sorted(modules - predicate)

    if extra_in_pred or missing_in_pred:
        print("FAIL: stdlib registry drift detected", file=sys.stderr)
        if extra_in_pred:
            print("  names allowed by isStdlibModuleName() but missing from MODULES:", file=sys.stderr)
            for n in extra_in_pred:
                print(f"    - {n}", file=sys.stderr)
        if missing_in_pred:
            print("  names present in MODULES but not recognized by isStdlibModuleName():", file=sys.stderr)
            for n in missing_in_pred:
                print(f"    - {n}", file=sys.stderr)
        print("", file=sys.stderr)
        print("Fix: keep MODULES and isStdlibModuleName() in sync (single source of truth).", file=sys.stderr)
        return 1

    print(f"OK: stdlib registry in sync ({len(modules)} modules)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

