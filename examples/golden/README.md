# Golden examples

Start here for **curated**, up-to-date samples that match current Kern syntax and the modern runtime.

Run from the **repository root** (so `lib/` resolves):

```powershell
.\shareable-ide\compiler\kern.exe examples\golden\golden_modern_runtime_commands_events.kn
.\shareable-ide\compiler\kern.exe examples\golden\golden_async_spawn_await.kn
.\shareable-ide\compiler\kern.exe examples\golden\golden_event_runtime_dashboard.kn
```

| Script | What it shows |
|--------|----------------|
| `golden_modern_runtime_commands_events.kn` | `@command` / `@event`, `create_runtime`, command run + aliases |
| `golden_async_spawn_await.kn` | `async def`, `spawn`, `await`, task maps |
| `golden_event_runtime_dashboard.kn` | Full stack: decorators, `emit` diagnostics, `inspect_event` / `inspect_all_events`, circuit-style listener |

Set **`KERN_LIB`** to this repo’s `lib` folder if imports fail. Validate everything under `examples/` with:

```powershell
.\scripts\check_examples.ps1
```

See also **[../README.md](../README.md)** for the full tier layout (`basic/`, `graphics/`, `system/`, `advanced/`).
