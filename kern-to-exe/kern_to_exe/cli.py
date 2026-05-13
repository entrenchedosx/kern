"""Headless argparse entry for kern-to-exe (recipe or explicit flags)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from kern_to_exe.build_runner import run_kernc_build
from kern_to_exe.spec import Kern2ExeSpec, load_recipe


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="python -m kern_to_exe",
        description="Package Kern .kn programs into a native .exe (headless mode). "
        "Without these options, `python -m kern_to_exe` opens the GUI.",
    )
    p.add_argument("--recipe", type=Path, metavar="FILE", help="Path to .kern2exe.json")
    p.add_argument("--entry", default="", help="Main .kn entry (if not using --recipe)")
    p.add_argument("--output", default="", help="Output .exe path")
    p.add_argument("--project-root", default="", dest="project_root", help="Project root / cwd for kernc")
    p.add_argument("--extra-kn", action="append", default=[], metavar="PATH", help="Extra .kn modules (repeatable)")
    p.add_argument("--asset", action="append", default=[], metavar="PATH", help="Asset paths (repeatable)")
    p.add_argument("--icon", default="", help="Path to .ico")
    p.add_argument("--no-release", action="store_true", help="Debug build (kernconfig release=false)")
    p.add_argument("--opt", type=int, default=2, metavar="N", help="Optimization 0–3 (default 2)")
    p.add_argument("--no-console", action="store_true", help="Windowed subsystem (console=false)")
    p.add_argument("--force", action="store_true", help="Delete previous output exe and .kern-cache")
    p.add_argument("--kern-repo-root", default="", dest="kern_repo_root", help="KERN_REPO_ROOT if kernc cannot find sources")
    p.add_argument("--machine-json", action="store_true", dest="machine_json", help="Pass --json to kernc")
    p.add_argument("--diagnostics-json", default="", dest="diagnostics_json", metavar="PATH", help="kernc --build-diagnostics-json")
    p.add_argument("--pre-build", action="append", default=[], metavar="CMD", help="Pre-build plugin command (repeatable)")
    p.add_argument("--post-build", action="append", default=[], metavar="CMD", help="Post-build plugin command (repeatable)")
    return p


def spec_from_ns(ns: argparse.Namespace) -> Kern2ExeSpec:
    if ns.recipe:
        spec = load_recipe(Path(ns.recipe))
        if ns.force:
            spec.force_rebuild = True
        return spec
    return Kern2ExeSpec(
        entry=ns.entry,
        output=ns.output,
        project_root=ns.project_root,
        extra_kn_files=list(ns.extra_kn),
        assets=list(ns.asset),
        icon=ns.icon,
        release=not ns.no_release,
        opt=int(ns.opt),
        console=not ns.no_console,
        force_rebuild=bool(ns.force),
        kern_repo_root=ns.kern_repo_root,
        machine_json=bool(ns.machine_json),
        pre_build=list(ns.pre_build),
        post_build=list(ns.post_build),
        diagnostics_json=ns.diagnostics_json,
    )


def try_headless(argv: list[str] | None = None) -> int | None:
    """
    If argv requests a headless build, run it and return exit code.
    Return None to fall back to the GUI (no headless args).
    """
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv:
        return None
    parser = _build_parser()
    ns, rest = parser.parse_known_args(argv)
    if rest:
        print(f"Unknown arguments: {' '.join(rest)}", file=sys.stderr)
        return 2

    use_recipe = ns.recipe is not None
    use_flags = bool(ns.entry and ns.output and ns.project_root)
    if not use_recipe and not use_flags:
        return None
    if use_recipe and use_flags:
        print("Use either --recipe or explicit --entry/--output/--project-root, not both.", file=sys.stderr)
        return 2

    spec = spec_from_ns(ns)
    return run_kernc_build(spec)


def main(argv: list[str] | None = None) -> int:
    """Always headless (for `python -m kern_to_exe.cli`)."""
    argv = list(sys.argv[1:] if argv is None else argv)
    parser = _build_parser()
    ns = parser.parse_args(argv)
    use_recipe = ns.recipe is not None
    use_flags = bool(ns.entry and ns.output and ns.project_root)
    if not use_recipe and not use_flags:
        parser.error("Provide --recipe or --entry, --output, and --project-root")
    if use_recipe and use_flags:
        parser.error("Use either --recipe or explicit --entry/--output/--project-root, not both")
    return run_kernc_build(spec_from_ns(ns))


if __name__ == "__main__":
    raise SystemExit(main())
