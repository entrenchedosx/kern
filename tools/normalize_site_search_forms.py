#!/usr/bin/env python3
"""Post-process MkDocs HTML output: fix search form id/404 action. Safe to run after every `mkdocs build`."""
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SITE = ROOT / "site"


def main() -> int:
    if not SITE.is_dir():
        print("normalize_site_search_forms: no site/ directory; skip", file=sys.stderr)
        return 0
    changed = 0
    for p in SITE.rglob("*.html"):
        # Theme override fragments (Jinja) are copied here; do not rewrite.
        if "overrides" in p.parts:
            continue
        t = p.read_text(encoding="utf-8")
        n = t.replace('id ="rtd-search-form"', 'id="rtd-search-form"')
        n = n.replace('action="//search.html"', 'action="./search.html"')
        if n != t:
            p.write_text(n, encoding="utf-8")
            changed += 1
            print(f"normalize_site_search_forms: updated {p.relative_to(ROOT)}")
    if changed:
        print(f"normalize_site_search_forms: {changed} file(s) patched")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
