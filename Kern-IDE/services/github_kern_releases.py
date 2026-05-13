"""Fetch Kern compiler releases from GitHub (no third-party deps)."""

from __future__ import annotations

import json
import ssl
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any

DEFAULT_REPO = "entrenchedosx/kern"
USER_AGENT = "KernIDE/1.0 (compatible; +https://github.com/entrenchedosx/kern-IDE)"


@dataclass(frozen=True)
class ReleaseAsset:
    name: str
    size: int
    download_url: str
    content_type: str


@dataclass(frozen=True)
class KernRelease:
    tag: str
    name: str
    published_at: str
    body_preview: str
    assets: tuple[ReleaseAsset, ...]
    html_url: str


def _req(url: str, timeout: float = 30.0) -> bytes:
    r = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "application/vnd.github+json"})
    ctx = ssl.create_default_context()
    with urllib.request.urlopen(r, timeout=timeout, context=ctx) as resp:
        return resp.read()


def fetch_releases(repo: str = DEFAULT_REPO, per_page: int = 30) -> list[KernRelease]:
    url = f"https://api.github.com/repos/{repo}/releases?per_page={per_page}"
    raw = _req(url)
    data = json.loads(raw.decode("utf-8"))
    if not isinstance(data, list):
        return []
    out: list[KernRelease] = []
    for item in data:
        if not isinstance(item, dict):
            continue
        tag = str(item.get("tag_name") or "").strip()
        if not tag:
            continue
        name = str(item.get("name") or tag).strip()
        published = str(item.get("published_at") or "")
        body = str(item.get("body") or "")
        preview = body.strip().replace("\r\n", "\n")
        if len(preview) > 220:
            preview = preview[:217] + "…"
        html_url = str(item.get("html_url") or "")
        assets_raw = item.get("assets")
        assets_list: list[ReleaseAsset] = []
        if isinstance(assets_raw, list):
            for a in assets_raw:
                if not isinstance(a, dict):
                    continue
                an = str(a.get("name") or "")
                if not an:
                    continue
                assets_list.append(
                    ReleaseAsset(
                        name=an,
                        size=int(a.get("size") or 0),
                        download_url=str(a.get("browser_download_url") or ""),
                        content_type=str(a.get("content_type") or ""),
                    )
                )
        out.append(
            KernRelease(
                tag=tag,
                name=name,
                published_at=published,
                body_preview=preview or "(no notes)",
                assets=tuple(assets_list),
                html_url=html_url,
            )
        )
    return out


def pick_windows_compiler_zip(assets: tuple[ReleaseAsset, ...]) -> ReleaseAsset | None:
    """Prefer standalone compiler zips; skip IDE / kern-to-exe bundles."""
    if not assets:
        return None
    scored: list[tuple[int, ReleaseAsset]] = []
    for a in assets:
        n = a.name.lower()
        if not n.endswith(".zip"):
            continue
        if "kern-ide" in n or "kern_to_exe" in n or "kern-to-exe" in n:
            continue
        score = 0
        if "kern-windows-x64" in n:
            score += 100
        elif "compiler" in n and "win" in n:
            score += 80
        elif "win64" in n or "windows" in n:
            score += 60
        elif n.startswith("kern-") and "win" in n:
            score += 40
        else:
            score += 10
        scored.append((score, a))
    if not scored:
        return None
    scored.sort(key=lambda x: -x[0])
    return scored[0][1]


def fetch_latest_release_tag(repo: str = DEFAULT_REPO) -> str | None:
    try:
        rels = fetch_releases(repo=repo, per_page=1)
        return rels[0].tag if rels else None
    except (urllib.error.URLError, OSError, json.JSONDecodeError, ValueError):
        return None
