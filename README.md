# Kern Programming Language

[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-2.0.3-blue.svg)](KERN_VERSION.txt)
[![Website](https://img.shields.io/badge/website-kerncode.art-blue)](https://kerncode.art)
[![Discord](https://img.shields.io/badge/discord-Kern-5865F2?logo=discord&logoColor=white)](https://discord.gg/JBa4RfT2tE)

**Kern** is a scripting language that feels like Python but can touch the system like C++. Write readable code, compile it to bytecode, and run it on a fast VM. Comes with built-in tools for debugging, package management, and deployment.

[Website](https://kerncode.art) — [Discord](https://discord.gg/JBa4RfT2tE) — [Docs](docs/getting-started.md)

## Why Kern?

Most scripting languages force you to choose between readable code and system access. Kern gives you both:

```kern
// Looks like Python...
let files = list_dir("./logs")
for f in files {
    if f.size > 1024 * 1024 {
        print("Big file: " + f.name)
    }
}

// ...but can call C libraries
let handle = ffi_open("libsystem.so")
ffi_call(handle, "get_system_info", [])
```

### What you get

- **Familiar syntax** - Variables, functions, classes, modules, pattern matching
- **Compiled bytecode** - Not interpreted line-by-line
- **Built-in tools** - Package manager (`kargo`), linter (`--check`), scanner (`--scan`)
- **Optional graphics** - 2D/3D when you need them (via Raylib)
- **Trust-based** - You control the safety level, not the language

### Compared to other languages

|  | Kern | Python | Bash |
|--|------|--------|------|
| Readable syntax | ✓ | ✓ | ✗ |
| Compiled bytecode | ✓ | ✗ | ✗ |
| System-level access | ✓ | Partial | ✓ |
| Built-in package manager | ✓ | ✗ | ✗ |

## Quick Start

```bash
# Download a release or build from source
git clone https://github.com/EntrenchedOSX/kern.git
cd kern
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kern

# Run a script
./build/kern examples/basic/01_hello_world.kn

# Or start a REPL
./build/kern
```

### A simple program

```kern
// Fibonacci in Kern
let fib = def(n) {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
print(fib(30))  // 832040
```

## Community

- **Website:** [kerncode.art](https://kerncode.art)
- **Discord:** [discord.gg/JBa4RfT2tE](https://discord.gg/JBa4RfT2tE)
- **GitHub:** [github.com/EntrenchedOSX/kern](https://github.com/EntrenchedOSX/kern)

## Installation

### Prebuilt binaries

Download from [GitHub Releases](https://github.com/EntrenchedOSX/kern/releases) (Windows, Linux, macOS) or [kerncode.art](https://kerncode.art).

### Install script (easiest)

**Windows:**
```powershell
.\install.ps1 -Mode User
```

**Linux/macOS:**
```bash
chmod +x install.sh && ./install.sh --mode user
```

### Build from source

**Requirements:** C++17 compiler, CMake 3.14+

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake

# macOS
brew install cmake

# Build
git clone https://github.com/EntrenchedOSX/kern.git
cd kern
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kern
```

For graphics support, see [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md).


---

## Common Commands

```bash
kern script.kn                   # Run a script
kern                             # Start REPL
kern --check script.kn           # Compile without running
kern --version                   # Show version

# Package management
kern add pkg@^1.0.0              # Add a package
kern install                     # Install dependencies
kern publish                     # Publish your package
```

See `kern --help` for all options or [docs/getting-started.md](docs/getting-started.md) for detailed usage.

---

## Project Structure

| Directory | What it contains |
|-----------|------------------|
| `kern/core/` | Compiler, bytecode, diagnostics |
| `kern/runtime/` | VM, builtins, verifier |
| `kern/modules/` | Native modules: `g2d/`, `g3d/`, `system/`, `game/` |
| `lib/kern/` | Standard library (`.kn` files) |
| `examples/` | Sample programs |
| `tests/` | Test suites |
| `docs/` | Documentation |

See [docs/architecture.md](docs/architecture.md) for the full technical breakdown.

---

## Documentation

- **[getting-started.md](docs/getting-started.md)** - Install, build, first steps
- **[language-guide.md](docs/language-guide.md)** - Syntax and features
- **[kargo-guide.md](docs/kargo-guide.md)** - Package management
- **[TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** - Common issues and fixes
- **[CHANGELOG.md](CHANGELOG.md)** - Version history

See `docs/` for the full list.

---

## Testing

```powershell
.\build\Release\kern.exe test tests\coverage
```

See [docs/TESTING.md](docs/TESTING.md) for example sweeps, `kernc` tests, and stress runs. A full **go/no-go** script for releases is described in [RELEASE.md](RELEASE.md).

---

## Releases

- **Version file:** `KERN_VERSION.txt` (semver)
- **Git tags:** `v*` (e.g., `v2.0.3`)
- **Release assets:** Windows (zip), Linux (tar.gz), macOS (tar.gz)
- **Changelog:** [CHANGELOG.md](CHANGELOG.md)

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to submit changes.

## Support

- **Discord:** [discord.gg/JBa4RfT2tE](https://discord.gg/JBa4RfT2tE)
- **Issues:** [GitHub Issues](https://github.com/EntrenchedOSX/kern/issues)

---

## License

[GNU GPL v3.0](LICENSE).
