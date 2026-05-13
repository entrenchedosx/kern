# Stress / adversarial tests

Hand-written `.kn` files plus **`run_stress_suite.ps1`**, which generates pathological sources (long `??` chains, long unary prefixes, oversized files) and runs **`kern --check`**.

Every run also checks **encoding behavior** the lexer enforces: a **UTF-8 with BOM** sample must **`--check`** successfully, and a **UTF-16 BOM** stub must fail **`--check`** (see [`src/compiler/lexer.cpp`](../../kern/core/compiler/lexer.cpp) and [`source_encoding.hpp`](../../kern/core/compiler/source_encoding.hpp)).

The suite runs **`stress_vm_call_depth_overflow.kn`** with **`kern`** (not `--check`) and expects a **non-zero exit**: the VM must report max call depth exceeded instead of crashing. That script calls **`set_max_call_depth(1024)`** so the test does not depend on CLI defaults: the `kern` executable sets **`2048`** in debug mode (default) and **`8192`** in **`--release`** mode ([`kern/tools/main.cpp`](../../kern/tools/main.cpp)), which is higher than the **`VM`** class default **`1024`** in [`kern/runtime/vm/vm.hpp`](../../kern/runtime/vm/vm.hpp). The internal **`kern test`** harness uses a plain `VM` without that override, so it still uses **1024**. When `maxCallDepth_` is non-zero, the VM **disables tail-call frame reuse** so depth limits also apply to tail recursion.

### Modes

- **Default:** Hand-written `*.kn` checks, UTF-8/UTF-16 BOM checks, generated coalesce (12k segments) and unary (8k), oversized source rejection, VM depth overflow run.
- **Aggressive (`-Aggressive`):** Larger generated chains (50k coalesce, 35k unary), **`kern --release`** on the depth script (must still exit non-zero), and full **`kern`** runs on the other stress scripts (expect exit **0**).

From repo root:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\stress\run_stress_suite.ps1
powershell -ExecutionPolicy Bypass -File .\tests\stress\run_stress_suite.ps1 -Aggressive
```

Requires `build\Release\kern.exe` or `shareable-ide\compiler\kern.exe`.
