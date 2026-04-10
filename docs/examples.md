# Examples

Examples live under [`examples/`](../examples/).

## Tiers

1. **Headless / CI** — `examples/basic/`, `examples/tour/`, `examples/math/` (avoid `g2d`/`g3d`/`game`).
2. **Graphics** — `examples/graphics/`; requires `KERN_BUILD_GAME=ON` and a display.
3. **Advanced** — networking, kits, and large demos; read each file’s header before running.

## Runner script

From the repo root (Windows):

```powershell
pwsh -File scripts/run_examples_headless.ps1 -KernExe .\build\Release\kern.exe
```

The stable CI suite uses [`tests/run_stable.ps1`](../tests/run_stable.ps1) with `KERN_COVERAGE_SKIP_GRAPHICS=1`.

## Kargo sample

See [`examples/kargo/README.md`](../examples/kargo/README.md).
