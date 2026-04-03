# Internals — virtual machine

## Execution model

- **Entry** — `VM::setBytecode`, `setStringConstants`, `setValueConstants`, optional `setActiveSourcePath`, then `VM::run()` in [`src/vm/vm.cpp`](../src/vm/vm.cpp).
- **Stack machine** — Operands and locals on an internal stack; call frames track function name, file path, line/column for errors and `stack_trace`.
- **Globals** — Script-level bindings and module surfaces; `getGlobalsSnapshot` supports import/export semantics.

## Runtime guards (`RuntimeGuardPolicy`)

Defined in [`vm.hpp`](../src/vm/vm.hpp) and enforced throughout builtins:

| Field | Purpose |
|-------|---------|
| `allowUnsafe` | Unlocks sensitive operations when true (e.g. `kern --unsafe`) |
| `enforcePermissions` | When true, sensitive builtins require `require(...)`, `unsafe {}`, or `--allow=` |
| `enforcePointerBounds` | FFI / raw memory bounds checking |
| `ffiEnabled` | Gate for dynamic library FFI |
| `sandboxEnabled` | Coarse sandbox toggle where applicable |
| `grantedPermissions` | Set of permission strings pre-approved for this run |

Permission helpers live in [`src/vm/permissions.hpp`](../src/vm/permissions.hpp). See [TRUST_MODEL.md](TRUST_MODEL.md) and [INTERNALS_MODULES_AND_SECURITY.md](INTERNALS_MODULES_AND_SECURITY.md).

## Limits and determinism hooks

- **`setStepLimit(n)`** — Instruction budget; `0` = unlimited. Throws `VMError` with `VM-STEP-LIMIT` when exceeded (see [`vm_error_registry.hpp`](../src/vm/vm_error_registry.hpp)).
- **`setMaxCallDepth`** — Caps recursion depth; tail-call behavior interacts with this when a limit is set (`vm.hpp` documents interaction).
- **`setCallbackStepGuard`** — Extra guard for nested native callbacks.
- **Cycles** — `getCycleCount()` / `resetCycleCount()` for profiling.

## Errors

- **`VMError`** — Carries message, span (`line`, `column`, optional end), `category`, and `code` (`VMErrorCode` in [`vm_error_codes.hpp`](../src/vm/vm_error_codes.hpp)).
- **Registry** — Central list of stable codes and hint/detail text in [`vm_error_registry.hpp`](../src/vm/vm_error_registry.hpp); keep user-facing codes synchronized with [ERROR_CODES.md](ERROR_CODES.md).
- **Stack snapshots** — `getCallStackSlice` (bounded by `kMaxCallStackSnapshotFrames`) feeds human and JSON output.

## Values and memory

- **`Value` / `ValuePtr`** — Reference-counted heap objects; see [MEMORY_MODEL.md](MEMORY_MODEL.md).
- **Concurrency** — Assume **one VM per thread** for normal scripts; async/task builtins are **cooperative** on the same VM unless a builtin explicitly documents otherwise.

## Debugging

- **VM trace** — `setVmTraceEnabled` / CLI `--trace` / REPL `trace on` for instruction-level logging (see [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)).

## See also

- [INTERNALS_COMPILER_AND_BYTECODE.md](INTERNALS_COMPILER_AND_BYTECODE.md)
- [BUILTIN_REFERENCE.md](BUILTIN_REFERENCE.md)
