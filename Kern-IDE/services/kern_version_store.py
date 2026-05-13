"""Install and resolve Kern compiler versions under portable kern_versions/."""

from __future__ import annotations

import shutil
import tempfile
import zipfile
from pathlib import Path
from typing import Callable

from .ide_logging import append_log

Progress = Callable[[int, int | None], None]  # bytes_read, total_or_none


def tag_to_dir_name(tag: str) -> str:
    t = tag.strip()
    if not t.startswith("v"):
        t = "v" + t.lstrip("v")
    return t


def version_dir(versions_root: Path, tag: str) -> Path:
    return versions_root / tag_to_dir_name(tag)


def find_kern_exe(root: Path, max_depth: int = 8) -> Path | None:
    """Locate kern.exe under an extracted tree (Windows)."""
    if not root.is_dir():
        return None
    direct = root / "kern.exe"
    if direct.is_file():
        return direct.resolve()
    depth = 0
    stack: list[tuple[Path, int]] = [(root, 0)]
    while stack:
        p, d = stack.pop()
        if d > max_depth:
            continue
        try:
            for ch in p.iterdir():
                if ch.name.lower() == "kern.exe" and ch.is_file():
                    return ch.resolve()
                if ch.is_dir():
                    stack.append((ch, d + 1))
        except OSError:
            continue
    return None


def _safe_extract_zip(zpath: Path, dest: Path) -> None:
    dest = dest.resolve()
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zpath, "r") as zf:
        for m in zf.infolist():
            name = m.filename
            if name.endswith("/") or not name.strip():
                continue
            target = (dest / name).resolve()
            try:
                target.relative_to(dest)
            except ValueError as exc:
                raise ValueError(f"unsafe zip path: {name!r}") from exc
        zf.extractall(dest)


def list_installed_tags(versions_root: Path) -> list[str]:
    if not versions_root.is_dir():
        return []
    tags: list[str] = []
    for p in sorted(versions_root.iterdir()):
        if not p.is_dir():
            continue
        if find_kern_exe(p) is not None:
            tags.append(p.name)
    return tags


def verify_installation(version_path: Path) -> tuple[bool, str]:
    exe = find_kern_exe(version_path)
    if exe is None:
        return False, "kern.exe not found under version folder"
    try:
        if exe.stat().st_size < 1024:
            return False, "kern.exe suspiciously small"
    except OSError as exc:
        return False, str(exc)
    return True, str(exe)


def install_zip(
    *,
    zip_url: str,
    tag: str,
    versions_root: Path,
    on_progress: Progress | None = None,
    portable_root: Path | None = None,
    replace: bool = False,
) -> tuple[bool, str]:
    import ssl
    import urllib.request

    tag_dir = version_dir(versions_root, tag)
    if tag_dir.exists() and not replace:
        ok, msg = verify_installation(tag_dir)
        if ok:
            return True, f"already installed: {msg}"

    append_log("install.log", f"download start tag={tag} url={zip_url}", portable_root=portable_root)

    ctx = ssl.create_default_context()
    req = urllib.request.Request(zip_url, headers={"User-Agent": "KernIDE/1.0"})

    tmp_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        total: int | None = None
        with urllib.request.urlopen(req, timeout=120, context=ctx) as resp:
            cl = resp.headers.get("Content-Length")
            if cl and cl.isdigit():
                total = int(cl)
            read = 0
            block = 256 * 1024
            with tmp_path.open("wb") as out:
                while True:
                    chunk = resp.read(block)
                    if not chunk:
                        break
                    out.write(chunk)
                    read += len(chunk)
                    if on_progress:
                        on_progress(read, total)

        work_path = Path(tempfile.mkdtemp(prefix="kern_extract_"))
        try:
            _safe_extract_zip(tmp_path, work_path)
            if tag_dir.exists():
                shutil.rmtree(tag_dir, ignore_errors=True)
            tag_dir.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(work_path), str(tag_dir))
        except Exception:
            if work_path.exists():
                shutil.rmtree(work_path, ignore_errors=True)
            raise
        finally:
            if tmp_path is not None and tmp_path.exists():
                tmp_path.unlink()

        ok, msg = verify_installation(tag_dir)
        if ok:
            append_log("install.log", f"install ok tag={tag} exe={msg}", portable_root=portable_root)
            return True, msg
        append_log("install.log", f"install verify failed tag={tag} {msg}", portable_root=portable_root)
        return False, msg
    except Exception as exc:
        if tmp_path is not None and tmp_path.exists():
            try:
                tmp_path.unlink()
            except OSError:
                pass
        err = f"{type(exc).__name__}: {exc}"
        append_log("install.log", f"install error tag={tag} {err}", portable_root=portable_root)
        return False, err
