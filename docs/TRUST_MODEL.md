# Kern’s trust model

Kern is designed primarily as a **trust-the-programmer** language: the core toolchain does **not** pretend to be a browser-grade sandbox. That matches common “systems” languages: **full control by default**, **clear responsibility** on the author and the **host process** that runs `kern`.

This document is **policy**, not a formal proof. It explains how Kern is meant to be used and how optional safety layers fit.

---

## What “trust the programmer” means here

| Idea | Kern’s stance |
|------|----------------|
| **Core language** | Small surface: compile to bytecode, run on the VM. No mandatory permission system in the grammar. |
| **Power** | File I/O, process helpers, memory/FFI (where enabled), and graphics are **explicit** APIs and flags—not hidden magic. |
| **Security boundary** | Mostly **outside** the language: OS user, file permissions, antivirus, and **how** you invoke `kern` (CLI flags, embedded host). |
| **Bugs & malicious code** | Can read files, call the network, crash the process, etc., in line with what the **OS allows** that process to do. |

So: **Kern does not enforce a capability system on every script.** Libraries under `lib/kern/` (for example `runtime/modern/capabilities.kn`) can implement **optional**, **opt-in** policy objects for **your** tooling—but that is **library discipline**, not a kernel-enforced sandbox.

---

## What the VM still does (explicit, not “hidden nanny”)

These are **mechanical** guards, not a high-level permission model:

- **FFI** can be gated (`ffi_call` disabled unless you enable it; optional library allowlists in some configurations).
- **`unsafe` blocks** for low-level memory operations are explicit in source.
- **CLI flags** such as `--allow-unsafe`, `--ffi`, `--no-sandbox` change what the **binary** exposes—documented in `--help`, not silent.

None of this replaces **OS security**; it narrows what **this** `kern` process will expose by default.

---

## Optional safety layers (recommended pattern)

You can build **stronger** policies **on top** of Kern without making them mandatory for everyone:

1. **Libraries** — e.g. `lib/kern/runtime/modern/` — capability-style maps, logging, scoped checks. **Opt-in**: import and use them in programs that need that structure.
2. **Hosts** — IDEs, servers, or packagers can run `kern` with restricted cwd, read-only dirs, or no FFI.
3. **Documentation** — warn clearly when an API touches disk, network, or native code.

That way Kern stays **flexible and fast** for trusted tools, while **constrained deployments** can add layers where needed.

---

## Tradeoffs (choose consciously)

**Trust-based (default posture)**

- **Pros:** Simple core, predictable performance, full control for the author.
- **Cons:** No automatic protection against buggy or hostile `.kn` if the OS lets the process do damage.

**Mandatory sandbox (not Kern’s default)**

- **Pros:** Stronger default for untrusted code.
- **Cons:** Heavier runtime, more surprises, harder to use for system scripting.

Kern aims to stay in the **first** camp at the language core, while **allowing** the second style to be built **around** it where required.

---

## For maintainers and contributors

- Prefer **explicit** names and docs for powerful builtins.
- Treat **new** “safety” features as **optional** (flags, modules, hosts) unless there is a strong reason to change defaults.
- When adding examples that look like “admin” or capabilities, label them as **demos** and describe the **trust boundary** (see module headers in `lib/kern/runtime/modern/`).

---

## See also

- [GETTING_STARTED.md](GETTING_STARTED.md) — how to build and run.
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — FFI, imports, runtime errors.
- `kern --help` — runtime guard flags.
