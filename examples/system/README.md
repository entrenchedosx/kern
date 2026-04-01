# System examples

Samples that touch **processes, input, FFI, vision hooks, or the oskit** layer. Some are **Windows-only** (`ffi_windows_*.kn`): use `kern --check` on other platforms, or skip those files.

| Area | Examples |
|------|-----------|
| **FFI (Windows)** | `ffi_windows_sleep.kn`, `ffi_windows_phase1.kn` — `extern def` + `unsafe { }` |
| **Processes** | `system_process_*.kn`, `system_render_ticker.kn` |
| **Input** | `system_input_events.kn`, `system_input_reactor.kn` |
| **Vision / probes** | `system_vision_*.kn` |
| **Oskit** | `oskit_minimal_kernel.kn`, `oskit_pointer_demo.kn` |
| **Probe** | `os_runtime_probe.kn` |

Run from repo root; set **`KERN_LIB`** to the repo `lib` folder when imports under `lib/kern/` fail.

See **[../README.md](../README.md)** for the full examples layout.
