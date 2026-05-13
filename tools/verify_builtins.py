"""Verify getBuiltinNames() list matches setGlobalFn(..., index) coverage in builtins.hpp."""
import re
from pathlib import Path

def main() -> int:
    p = Path(__file__).resolve().parents[1] / "src" / "vm" / "builtins.hpp"
    text = p.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"names\s*=\s*\{([^}]+)\};\s*return names", text, re.S)
    if not m:
        print("Could not find names = { ... }; return names")
        return 1
    body = m.group(1)
    parts = re.findall(r'"([^"]+)"', body)
    sgf = re.findall(r'setGlobalFn\s*\(\s*"([^"]+)"\s*,\s*(\d+)\s*\)', text)
    by_idx: dict[int, list[str]] = {}
    for name, idx_s in sgf:
        idx = int(idx_s)
        by_idx.setdefault(idx, []).append(name)
    max_idx = max(by_idx)
    missing = [i for i in range(max_idx + 1) if i not in by_idx]
    errs = 0
    if missing:
        print("ERROR: indices with no setGlobalFn:", missing[:40], "..." if len(missing) > 40 else "")
        errs += 1
    if len(parts) != max_idx + 1:
        print(f"ERROR: getBuiltinNames count {len(parts)} != max_index+1 {max_idx + 1}")
        errs += 1
    mismatches = []
    for i, vec_name in enumerate(parts):
        names_at = by_idx.get(i)
        if not names_at:
            mismatches.append((i, vec_name, None))
        elif vec_name not in names_at:
            mismatches.append((i, vec_name, names_at))
    if mismatches:
        print(f"ERROR: {len(mismatches)} vector entries not among setGlobalFn names for that index:")
        for row in mismatches[:30]:
            print(" ", row)
        errs += 1
    dup = [(i, n) for i, n in sorted(by_idx.items()) if len(n) > 1]
    if dup:
        print("INFO: indices with multiple global aliases:", len(dup))
    if errs == 0:
        print(f"OK: {len(parts)} builtins, indices 0..{max_idx}")
    return errs


if __name__ == "__main__":
    raise SystemExit(main())
