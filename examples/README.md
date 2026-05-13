# Kern Examples Gallery

Welcome to the Kern examples! This is your hands-on guide to discovering what makes Kern special—from elegant scripting to powerful systems programming.

## Quick Start

**Run any example from the repository root:**

```bash
# On Windows
.\build\Release\kern.exe examples\basic\01_hello_world.kn

# On Linux/macOS
./build/kern examples/basic/01_hello_world.kn
```

**Or use a packaged compiler:**
```bash
./shareable-ide/compiler/kern examples/basic/01_hello_world.kn
```

**Pro tip:** Set `KERN_LIB` to this repo's `lib` folder when examples import under `lib/kern/` (many advanced samples do).

---

## Learning Path: From Zero to Hero

### Level 1: Basic Mastery (`basic/`)
**What you'll learn:** Core syntax, data structures, standard library
**Start here:** `01_hello_world.kn` → follow the numbered sequence

**Highlights:**
- Clean, Python-like syntax that's easy to read
- Functions, arrays, maps, and control flow
- Modern features: pattern matching, comprehensions, pipelines

**Must-see examples:**
- `01_hello_world.kn` - Your first Kern program
- `05_functions.kn` - Functions and recursion
- `18_modern_syntax.kn` - Map comprehensions, spread, pipelines
- `22_pipeline_funtools.kn` - Functional programming style

**Complete Guide →** [basic/README.md](basic/README.md)

---

### Level 2: Modern Kern (`golden/`)
**What you'll learn:** Async programming, events, decorators, modern runtime
**Prerequisites:** Comfortable with basic syntax

**Highlights:**
- **Async/await** for concurrent programming
- **Decorators** for metaprogramming (`@command`, `@event`)
- **Event-driven architecture** with built-in event bus
- **Modern runtime** with command registration and execution

**Must-see examples:**
- `golden_async_spawn_await.kn` - Async programming patterns
- `golden_modern_runtime_commands_events.kn` - Commands and events
- `golden_event_runtime_dashboard.kn` - Full event-driven application

**Golden Examples →** [golden/README.md](golden/README.md)

---

### Level 3: Graphics & Gaming (`graphics/`)
**What you'll learn:** 2D/3D graphics, game development, interactive applications
**Prerequisites:** Basic syntax + modern features

**Highlights:**
- **2D graphics** with immediate mode rendering
- **3D graphics** with models, cameras, and lighting
- **Input handling** for keyboard and mouse
- **Game loops** and real-time rendering

**Must-see examples:**
- `01_2d_window.kn` - Your first graphics window
- `10_3d_cube.kn` - Basic 3D rendering
- `20_input_demo.kn` - Interactive input handling

**Graphics Guide →** [graphics/README.md](graphics/README.md)

---

### Level 4: Systems Programming (`system/`)
**What you'll learn:** FFI, process management, OS integration, system calls
**Prerequisites:** Comfortable with all previous levels

**Highlights:**
- **FFI (Foreign Function Interface)** - Call C libraries directly
- **Process management** - Spawn and control external processes
- **System integration** - Work with the operating system
- **Memory management** - Direct memory access with `unsafe` blocks

**Must-see examples:**
- `ffi_windows_sleep.kn` - Call Windows APIs
- `system_process_spawn.kn` - Process management
- `unsafe_demo.kn` - Direct memory manipulation

**System Guide →** [system/README.md](system/README.md)

---

### Level 5: Advanced Applications (`advanced/`)
**What you'll learn:** Large-scale applications, frameworks, real-world projects
**Prerequisites:** Mastery of all previous levels

**Highlights:**
- **BrowserKit** - Web-like interfaces
- **GameKit** - Game development framework
- **HTTP servers** - Network programming
- **Complex applications** - Real-world examples

**Advanced Guide →** [advanced/README.md](advanced/README.md)

---

## Specialized Collections

| Collection | Focus | Skill Level |
|------------|-------|------------|
| **`math/`** | Mathematical equation solver | Beginner |
| **`algorithms/`** | Data structures and algorithms | Intermediate |
| **`games/`** | Complete game examples | Advanced |
| **`network/`** | Network programming | Advanced |
| **`kargo/`** | Package management examples | Intermediate |
| **`language/`** | Language processing examples | Advanced |

---

## Kern's Unique Features in Action

### Trust-the-Programmer Philosophy
```kern
// Unlike sandboxed languages, Kern gives you full control
unsafe {
    let ptr = allocate_memory(1024)
    defer free_memory(ptr)
    // Direct memory manipulation - when you need it
}
```

### Modern Language Features
```kern
// Async/await for concurrency
async def fetch_data(url) {
    let response = await http_get(url)
    return parse_json(response)
}

// Decorators for metaprogramming
@command("analyze", description="Analyze files")
def analyze(ctx, args) {
    return process_files(args["directory"])
}
```

### Graphics Without Complexity
```kern
// Simple 2D graphics
init_window(800, 600, "My Game")
while !window_should_close() {
    begin_drawing()
    draw_circle(400, 300, 50, BLUE)
    end_drawing()
}
```

### System-Level Access
```kern
// Call C libraries directly
let lib = ffi_open("libc.so.6")
ffi_call(lib, "printf", ["Hello from C!\n"])

// Process management
let proc = spawn("ls", ["-la"])
let output = wait_for_process(proc)
```

---

## Development Tools

### Validate All Examples
```powershell
# Run the full test suite
.\scripts\check_examples.ps1
```

### Individual Testing
```bash
# Compile without running
kern --check examples/basic/01_hello_world.kn

# See AST structure
kern --ast examples/basic/05_functions.kn

# Profile performance
kern --profile examples/advanced/large_app.kn
```

---

## What Makes These Examples Special

**Real, runnable code** - Every example works out of the box
**Progressive difficulty** - Start simple, build complexity gradually
**Modern patterns** - Showcase best practices and idiomatic Kern
**Comprehensive coverage** - From hello world to advanced applications
**Unique features** - Highlight what makes Kern different from other languages

---

## Your Journey Starts Here

**New to Kern?** Start with `basic/01_hello_world.kn` and follow the numbered sequence.

**Know programming?** Jump to `basic/18_modern_syntax.kn` to see what makes Kern special.

**Ready for advanced?** Explore `golden/` for modern patterns or `graphics/` for visual applications.

**Remember:** Kern grows with your ambition. Start simple, add complexity as you need it!

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
