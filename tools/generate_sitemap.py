#!/usr/bin/env python3
"""Emit site/sitemap.xml from built HTML under site/. Requires site_url in mkdocs.yml."""
from __future__ import annotations

import pathlib
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
SITE = ROOT / "site"
MKDOCS = ROOT / "mkdocs.yml"
XMLNS = "http://www.sitemaps.org/schemas/sitemap/0.9"


def load_site_url() -> str | None:
    text = MKDOCS.read_text(encoding="utf-8")
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("site_url:"):
            rest = stripped[len("site_url:") :].strip()
            if not rest or rest.startswith("#"):
                return None
            rest = rest.split("#", 1)[0].strip()
            return rest.strip().strip('"').strip("'")
    return None


def html_to_loc(base: str, rel: str) -> str:
    """Map site-relative HTML path to canonical absolute URL."""
    if rel == "404.html":
        return ""  # caller skips
    if rel == "index.html":
        return f"{base}/"
    if rel.endswith("/index.html"):
        path = rel[: -len("index.html")].rstrip("/")
        return f"{base}/{path}/" if path else f"{base}/"
    return f"{base}/{rel}"


def main() -> int:
    base = load_site_url()
    if not base:
        print("generate_sitemap: no site_url in mkdocs.yml; skip")
        return 0
    base = base.rstrip("/")
    if not SITE.is_dir():
        print("generate_sitemap: no site/; skip")
        return 0

    locs: set[str] = set()
    for html in SITE.rglob("*.html"):
        rel = html.relative_to(SITE).as_posix()
        if rel.startswith("overrides/"):
            continue
        if rel == "404.html":
            continue
        loc = html_to_loc(base, rel)
        if loc:
            locs.add(loc)

    urlset = ET.Element("urlset")
    urlset.set("xmlns", XMLNS)
    for loc in sorted(locs):
        u = ET.SubElement(urlset, "url")
        ET.SubElement(u, "loc").text = loc

    tree = ET.ElementTree(urlset)
    ET.indent(tree, space="  ")
    out = SITE / "sitemap.xml"
    tree.write(out, encoding="utf-8", xml_declaration=True)
    print(f"generate_sitemap: wrote {len(locs)} URL(s) -> {out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
