# Advanced examples

Larger demos: **BrowserKit**, **GameKit**, HTTP, **modern runtime** (capabilities, streams, plugins), and **project-style** scripts (security, observability, automation). Good for reading after `examples/basic/` and `examples/golden/`.

| Theme | Examples (representative) |
|--------|---------------------------|
| **Smoke / full stack** | `00_full_feature_smoke.kn` |
| **BrowserKit** | `browserkit_load_page.kn`, `browserkit_render_demo.kn`, `kern_mini_browser.kn` |
| **GameKit** | `gamekit_simple_game.kn`, `gamekit_gui_app.kn` |
| **Modern runtime** | `runtime_modern_demo.kn`, `runtime_stream_plugin_demo.kn`, `runtime_capability_admin_demo.kn` |
| **HTTP** | `http_smoke.kn` |
| **Concurrency / workers** | `advanced_module_concurrency.kn`, `project_concurrency_worker_supervisor.kn` |
| **Domain modules** | `advanced_module_*.kn` (netops, security, observability, …) |
| **Project templates** | `project_*.kn` (pipelines, orchestration, guards) |

Many scripts need **`KERN_LIB`** pointing at this repo’s `lib/` directory.

**Honest note:** `kern_mini_browser` is a small GUI + HTTP helper, not a full HTML engine.

See **[../README.md](../README.md)** and **[../golden/README.md](../golden/README.md)**.
