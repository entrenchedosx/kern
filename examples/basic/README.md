# Basic examples

Run from the **repository root** so `lib/` resolves for imports.

Suggested order for learning the language:

| # | File | Topic |
|---|------|--------|
| 1 | `01_hello_world.kn` | `print`, top-level scripts |
| 2 | `04_loops.kn` | `for` / `range` / `while` |
| 3 | `05_functions.kn` | `def`, `return`, recursion |
| 4 | `06_arrays.kn` | arrays, `len`, indexing |
| 5 | `07_maps.kn` | maps / dicts |
| 6 | `08_strings.kn` | strings, slicing |
| 7 | `09_import_stdlib.kn` | `import("math")`, `import("sys")` |
| 8 | `10_file_io.kn` | read/write files |
| 9 | `11_try_catch.kn` | errors |
| 10 | `12_lambda.kn` | `lambda` |
| 11 | `14_match.kn` | `match` / `case` |
| 12 | `modules.kn` | small `import("math")` smoke |
| 13 | `15_net_url_parse.kn` | `net.url_parse`, `net.build_query` (no network) |

There is no `02`/`03` in this folder; control flow is covered in `04_loops.kn` and `05_functions.kn`.

```powershell
.\build\Release\kern.exe examples\basic\01_hello_world.kn
```

See **[../README.md](../README.md)** for golden, graphics, system, and advanced tiers.
