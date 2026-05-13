#!/usr/bin/env python3
"""Run after `mkdocs build`: normalize search forms + regenerate sitemap.xml."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"


def main() -> int:
    steps = [
        TOOLS / "normalize_site_search_forms.py",
        TOOLS / "generate_sitemap.py",
    ]
    for script in steps:
        r = subprocess.run([sys.executable, str(script)], cwd=str(ROOT))
        if r.returncode != 0:
            return r.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
