"""Strip absolute paths in .kern-last-analysis.json so the repo can be moved.

Run from anywhere: python tools/relativize_kern_analysis_json.py
Paths under the repo root become relative (forward slashes). projectRoot becomes ".".
"""
import json
import pathlib
import sys


def relativize_str(s: str, repo: pathlib.Path) -> str:
    if not isinstance(s, str):
        return s
    if s in (".", ""):
        return s
    try:
        p = pathlib.Path(s)
        if p.is_absolute():
            rel = p.relative_to(repo.resolve())
            return rel.as_posix()
    except ValueError:
        pass
    return s


def walk(x, repo: pathlib.Path):
    if isinstance(x, str):
        return relativize_str(x, repo)
    if isinstance(x, list):
        return [walk(y, repo) for y in x]
    if isinstance(x, dict):
        return {k: walk(v, repo) for k, v in x.items()}
    return x


def main() -> int:
    repo = pathlib.Path(__file__).resolve().parent.parent
    p = repo / ".kern-last-analysis.json"
    if not p.is_file():
        print("skip: no file", p, file=sys.stderr)
        return 0
    with p.open(encoding="utf-8") as f:
        data = json.load(f)
    data = walk(data, repo)
    if isinstance(data, dict):
        data["projectRoot"] = "."
    with p.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, ensure_ascii=False)
        f.write("\n")
    print("updated", p)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
