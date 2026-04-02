# Kern examples

Run from the **repository root** so `lib/` and relative paths resolve.

```powershell
.\build\Release\kern.exe examples\basic\01_hello_world.kn
```

Or use a packaged compiler:

```powershell
.\shareable-ide\compiler\kern.exe examples\basic\01_hello_world.kn
```

Set **`KERN_LIB`** to this repo’s `lib` folder when examples import under `lib/kern/` (many advanced samples do).

## Suggested learning path

1. **`basic/`** — syntax, stdlib, small scripts. Start with `01_hello_world.kn`, then follow **[basic/README.md](basic/README.md)**.
2. **`golden/`** — modern runtime, async, events (curated “best current” samples). See **[golden/README.md](golden/README.md)**.
3. **`graphics/`** — 2D/3D when you have a game-enabled build. See **[graphics/README.md](graphics/README.md)**.
4. **`system/`** — OS/FFI/process samples; some Windows-only. See **[system/README.md](system/README.md)**.
5. **`advanced/`** — BrowserKit, GameKit, large demos. See **[advanced/README.md](advanced/README.md)**.

## Layout (folders)

| Folder | Purpose |
|--------|---------|
| `basic/` | Language basics: syntax, stdlib, small scripts |
| `golden/` | Curated demos: modern runtime, async, events |
| `graphics/` | `g2d`, `g3d`, games (Raylib-backed when enabled) |
| `system/` | Processes, input, FFI, oskit |
| `advanced/` | BrowserKit, GameKit, HTTP, project-style scripts |
| `math/` | Equation solver REPL: linear & quadratic (`equation_solver_demo.kn`) |

## Golden examples (recommended after basics)

```powershell
.\shareable-ide\compiler\kern.exe examples\golden\golden_modern_runtime_commands_events.kn
.\shareable-ide\compiler\kern.exe examples\golden\golden_async_spawn_await.kn
.\shareable-ide\compiler\kern.exe examples\golden\golden_event_runtime_dashboard.kn
```

Index: **[golden/README.md](golden/README.md)**.

## Validate all examples (CI-style)

```powershell
.\scripts\check_examples.ps1
```

Runs `kern.exe --check` on every `examples/**/*.kn` and fails on the first error. The publish script (`scripts\publish_shareable_drops.ps1`) runs this step automatically.

## Event bus inspection (IDE / dashboards)

The runtime `events` module exposes (names avoid the VM builtin `inspect`):

- `events.inspect_event(bus, event_name)` — listeners + circuit state snapshot for one event
- `events.inspect_all_events(bus)` — map of event name → snapshot

Snapshots are **stable for diffs**: listener rows are sorted by `id`; `inspect_all_events` lists events in **lexicographic** name order. Handler bodies are not included; only `handler_type` (e.g. `"function"`).

See `golden/golden_event_runtime_dashboard.kn`.

## Learn more

- **[../docs/GETTING_STARTED.md](../docs/GETTING_STARTED.md)** — build, run, and tooling basics
- **[../docs/TROUBLESHOOTING.md](../docs/TROUBLESHOOTING.md)** — quick fixes for common setup and runtime issues

## Requirements

- **Graphics / game:** samples that use `import("g3d")`, `import("2dgraphics")`, or `import("game")` need Kern built with **Raylib** (`KERN_BUILD_GAME=ON`). They usually print a clear message if graphics are unavailable.
- **kern_mini_browser** (under `advanced/`): GameKit-style GUI + `http_get`; it does not render full HTML—plain text and simple UI.
- **Windows FFI** (`system/ffi_windows_*.kn`): `extern def` is declared at module scope; **calls** must be inside `unsafe { ... }`. On non-Windows targets, run only `--check` or skip those examples.

## Honest limitations

- Nested `def` inside blocks is not supported; use `lambda` or top-level `def` + `main()`.
- Module top-level `return` is invalid; use `if/else` or wrap in `def main()`.
