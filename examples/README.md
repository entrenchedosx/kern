# Kern examples

Run from the **repository root** so `lib/` and relative paths resolve.

```powershell
.\build\Release\kern.exe examples\basic\01_hello_world.kn
```

Or use the packaged compiler:

```powershell
.\shareable-ide\compiler\kern.exe examples\basic\01_hello_world.kn
```

Set **`KERN_LIB`** to this repo’s `lib` folder when examples use imports under `lib/kern/` (many advanced samples do).

## Layout (quality tiers)

| Folder | Purpose |
|--------|---------|
| `golden/` | Best starting points: modern runtime, async, flagship demos |
| `basic/` | Language basics: syntax, stdlib, small scripts |
| `graphics/` | `g3d`, `g2d`, `2dgraphics`, Raylib-backed samples (e.g. `graphics/pong.kn`) |
| `system/` | OS-facing modules (`kern::process`, `kern::input`, FFI, oskit) |
| `advanced/` | BrowserKit, GameKit, HTTP, projects, large smoke tests |

## Golden examples (recommended)

Short index: **[golden/README.md](golden/README.md)**.

```powershell
.\shareable-ide\compiler\kern.exe examples\golden\golden_modern_runtime_commands_events.kn
.\shareable-ide\compiler\kern.exe examples\golden\golden_async_spawn_await.kn
.\shareable-ide\compiler\kern.exe examples\golden\golden_event_runtime_dashboard.kn
```

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
