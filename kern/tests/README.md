# Kern Tests

Run tests from project root (so `lib/kern` and paths resolve), or set `KERN_LIB` to the project root.

```powershell
# From project root
.\build\Release\kern.exe tests\test_parser.kn
.\build\Release\kern.exe tests\test_builtins.kn
```

Or use the project's test runner:

```powershell
.\run_all_tests.ps1 -Examples examples -Exe .\build\Release\kern.exe
```

- **test_parser.kn** – expressions, arrays, maps; no runtime errors.
- **test_builtins.kn** – asserts on sqrt, floor, min, max, type (requires `assert_eq` in scope; use `import("lib/kern/testing.kn")` if your build doesn't provide `assert_eq` globally).

If `assert_eq` is not a core builtin, run test_builtins after importing the testing module, or use a script that only prints and checks output.
