"""Resolve import(\"...\") paths to files or embedded/native module kinds (mirrors VM import order)."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from services.stdlib_exports import EMBEDDED_STDLIB_NAMES, NATIVE_MODULE_NAMES


@dataclass(frozen=True)
class ImportTarget:
    kind: str  # "file" | "embedded" | "native" | "unknown"
    path: Path | None = None
    module_key: str = ""
    import_literal: str = ""


def normalize_import_base(import_path: str) -> str:
    p = import_path.strip().replace("\\", "/")
    if p.endswith(".kn"):
        p = p[:-3]
    if "::" in p:
        return p.split("::")[-1]
    return p.split("/")[-1] or p


def lib_search_roots(workspace_root: Path, kern_exe: str | None) -> list[Path]:
    """Directories used like KERN_LIB / cwd: imports are relative to these (e.g. lib/kern/algo.kn)."""
    roots: list[Path] = []
    candidates = [
        workspace_root,
        workspace_root / "shareable-ide" / "compiler",
        workspace_root / "shareable-kern-compiler",
        workspace_root / "FINAL",
        workspace_root / "build" / "Release",
    ]
    env = os.environ.get("KERN_LIB", "").strip()
    if env:
        candidates.append(Path(env).expanduser())
    if kern_exe:
        candidates.append(Path(kern_exe).resolve().parent)
    seen: set[str] = set()
    for c in candidates:
        try:
            r = c.resolve()
        except OSError:
            continue
        key = str(r)
        if key in seen:
            continue
        if r.is_dir():
            seen.add(key)
            roots.append(r)
    return roots


def resolve_import_target(import_literal: str, workspace_root: Path, kern_exe: str | None) -> ImportTarget:
    lit = import_literal.strip().replace("\\", "/")
    if not lit:
        return ImportTarget(kind="unknown", import_literal=import_literal)
    base = normalize_import_base(lit)
    path_like = "/" in lit or "\\" in lit or "::" in lit

    if not path_like and base in EMBEDDED_STDLIB_NAMES:
        return ImportTarget(kind="embedded", module_key=base, import_literal=lit)
    if base in NATIVE_MODULE_NAMES and not path_like:
        return ImportTarget(kind="native", module_key=base, import_literal=lit)
    if "::" in lit:
        return ImportTarget(kind="native", module_key=base, import_literal=lit)

    path_variants: list[str] = []
    if lit.endswith(".kn"):
        path_variants.append(lit)
    else:
        if "." in lit.split("/")[-1]:
            path_variants.append(lit)
        else:
            path_variants.append(lit + ".kn")
        path_variants.append(lit)

    roots = lib_search_roots(workspace_root, kern_exe)
    for root in roots:
        try:
            root_res = root.resolve()
        except OSError:
            continue
        for rel in path_variants:
            cand = (root / rel.replace("/", os.sep))
            try:
                cand_res = cand.resolve()
            except OSError:
                continue
            if not cand_res.is_file():
                continue
            try:
                cand_res.relative_to(root_res)
            except ValueError:
                continue
            return ImportTarget(kind="file", path=cand_res, module_key=base, import_literal=lit)
    return ImportTarget(kind="unknown", module_key=base, import_literal=lit)


def scan_lib_module_paths(lib_roots: list[Path], *, max_files: int = 8000) -> list[str]:
    """Paths relative to each lib root, de-duplicated (posix, without .kn)."""
    out: set[str] = set()
    n = 0
    for root in lib_roots:
        if not root.is_dir():
            continue
        try:
            for p in root.rglob("*.kn"):
                n += 1
                if n > max_files:
                    return sorted(out)
                try:
                    rel = p.relative_to(root).as_posix()
                except ValueError:
                    continue
                if rel.endswith(".kn"):
                    rel = rel[:-3]
                if rel:
                    out.add(rel)
        except OSError:
            continue
    return sorted(out)
