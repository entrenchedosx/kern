# Internals — modules and security

## Module loading

- **Runtime import** — `import("path")` and related forms resolve through [`src/import_resolution.cpp`](../src/import_resolution.cpp) and VM support for `__import`.
- **Library roots** — `KERN_LIB` and packaged `lib/kern/` trees determine where standard modules load from; see [GETTING_STARTED.md](GETTING_STARTED.md) and repo layout.
- **Project metadata** — `kern.json` / lockfile (`kern.lock`) describe dependency names and versions for tooling; `kern verify` checks lock consistency (see [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)).

## Permissions (production default: deny sensitive ops)

Sensitive capabilities (filesystem, subprocess, environment, network) are gated by **permission strings** such as:

- `filesystem.read`, `filesystem.write`
- `system.exec`, `process.control`
- `env.access`
- `network.http`
- `network.tcp` — `tcp_connect`, `tcp_connect_start`, `tcp_connect_check`, `tcp_listen`, `tcp_accept`, `tcp_send`, `tcp_recv`, `tcp_close`, `socket_set_nonblocking`, `socket_select_read`, `socket_select_write` (see [NETWORKING_MULTIPLAYER.md](NETWORKING_MULTIPLAYER.md))
- `network.udp` — `udp_open`, `udp_bind`, `udp_send`, `udp_recv`, `udp_close`

Implementation: [`kern/runtime/vm/permissions.hpp`](../kern/runtime/vm/permissions.hpp) with `vmRequirePermission` / `vmPermissionAllowed`.

**Ways to grant** (pick one policy per deployment):

1. **Explicit in script** — `require("filesystem.read")` (and similar) where needed.
2. **CLI** — `kern --allow=filesystem.read` (repeatable) or `kern --unsafe` for full unlock (avoid in production for untrusted code).
3. **Unsafe regions** — `unsafe { ... }` for bounded blocks (when enforcement is on).
4. **Embedder** — Pre-fill `RuntimeGuardPolicy::grantedPermissions` (see [`registerAllStandardPermissions`](../kern/runtime/vm/permissions.hpp) for the “grant everything” test/REPL pattern—**not** a production default).

**Environment:** `KERN_ENFORCE_PERMISSIONS=0` disables enforcement (development/CI only; documented in permission errors).

## FFI and raw memory

- **FFI** — Controlled by `RuntimeGuardPolicy::ffiEnabled` and library allowlists; native calls must respect the same permission story as builtins.
- **`unsafe` bytecode** — `UNSAFE_BEGIN` / `UNSAFE_END` pair with raw memory opcodes; only valid inside approved contexts. See [TRUST_MODEL.md](TRUST_MODEL.md).

## Static assurance

- **`kern --scan`** — Catches drift between **documented** stdlib exports and **registered** builtins ([KERN_SCAN.md](KERN_SCAN.md)).
- **`kern --check`** — Compile-time validation; use `--json` in CI for machine-readable diagnostics.

## Embedding checklist (real projects)

1. Decide **permission policy** (whitelist vs `--unsafe` only in dev).
2. Set **step limits** / **max call depth** for untrusted scripts.
3. Pin **toolchain version** and run **`kern verify` + tests** in CI.
4. Never rely on **undefined** bytecode or private APIs under `src/` without fork maintenance—use documented CLI and language semantics.

## See also

- [INTERNALS_VM.md](INTERNALS_VM.md) — `RuntimeGuardPolicy` fields
- [TRUST_MODEL.md](TRUST_MODEL.md)
- [ERROR_CODES.md](ERROR_CODES.md)
- [PRODUCTION_VISION.md](PRODUCTION_VISION.md)
