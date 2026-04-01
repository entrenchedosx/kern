from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BYTECODE = ROOT / "src" / "vm" / "bytecode.hpp"
VM = ROOT / "src" / "vm" / "vm.cpp"


def _read(p: Path) -> str:
    try:
        return p.read_text(encoding="utf-8", errors="replace")
    except Exception as e:
        raise SystemExit(f"error: failed to read {p}: {e}")


def parse_opcodes(bytecode_hpp: str) -> list[str]:
    # enum class Opcode { A, B, ... };
    m = re.search(r"enum\s+class\s+Opcode(?:\s*:\s*[A-Za-z0-9_:]+)?\s*\{(?P<body>[\s\S]*?)\};", bytecode_hpp)
    if not m:
        raise SystemExit("error: could not find enum class Opcode in bytecode.hpp")
    body = m.group("body")
    # remove comments before splitting
    body = re.sub(r"/\*[\s\S]*?\*/", "", body)
    body = re.sub(r"//.*?$", "", body, flags=re.MULTILINE)
    out: list[str] = []
    for raw in body.split(","):
        name = raw.strip()
        if not name:
            continue
        # drop assignments and comments
        name = name.split("=", 1)[0].strip()
        if name:
            out.append(name)
    return out


def parse_vm_cases(vm_cpp: str) -> set[str]:
    # case Opcode::FOO:
    cases = set(re.findall(r"case\s+Opcode::([A-Za-z0-9_]+)\s*:", vm_cpp))
    return cases


def main() -> int:
    bytecode = _read(BYTECODE)
    vm = _read(VM)

    opcodes = parse_opcodes(bytecode)
    cases = parse_vm_cases(vm)

    # Intentionally ignore sentinel/compile-time aliases if any ever appear later.
    missing = [op for op in opcodes if op not in cases]

    if missing:
        print("FAIL: VM opcode switch is missing handlers for:", file=sys.stderr)
        for op in missing:
            print(f"  - {op}", file=sys.stderr)
        print("", file=sys.stderr)
        print("Fix: implement the opcode in src/vm/vm.cpp or reject it explicitly.", file=sys.stderr)
        return 1

    print(f"OK: {len(opcodes)} opcodes handled")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

