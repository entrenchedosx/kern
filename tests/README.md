# Kern Tests

Run tests from project root (so `lib/kern` and paths resolve), or set `KERN_LIB` to the project root.

```powershell
# From project root
.\BUILD\bin\kern.exe tests\test_parser.kn
.\BUILD\bin\kern.exe tests\test_builtins.kn
```

Or use the project's test runner:

```powershell
.\run_all_tests.ps1 -Examples examples -Exe .\BUILD\bin\kern.exe
```

- **test_parser.kn** – expressions, arrays, maps; no runtime errors.
- **test_builtins.kn** – asserts on sqrt, floor, min, max, type (requires `assert_eq` in scope; use `import("lib/kern/testing.kn")` if your build doesn't provide `assert_eq` globally).

If `assert_eq` is not a core builtin, run test_builtins after importing the testing module, or use a script that only prints and checks output.

Additional coverage tests live under `tests/coverage/`:

- `test_lexer_behavior.kn`
- `test_parser_behavior.kn`
- `test_runtime_execution.kn`
- `test_module_loading.kn`
- `test_graphics_init.kn`

For g3d-specific regression in one command:

```powershell
.\tests\run_g3d_regression.ps1 -SplExe .\BUILD\Release\kern.exe
```

For Wave 1-3 feature regression (runtime/process/diagnostics/language+stdlib additions):

```powershell
.\tests\run_wave_regression.ps1 -Exe .\build\Release\kern.exe
```
