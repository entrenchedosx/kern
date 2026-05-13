# Kern feature tour

Short, **runnable** scripts that each focus on one area. Run from the **repository root** so `lib/kern` resolves.

```powershell
.\build\Release\kern.exe examples\tour\01_import_styles.kn
```

Set **`KERN_LIB`** to the repo root (the folder that contains `lib/kern`) if you run from another directory.

## Order

| # | File | Topics |
|---|------|--------|
| 01 | `01_import_styles.kn` | `import "math"` / `import "sys"`, `from "math" import …`, `math.*` vs globals |
| 02 | `02_stdlib_json.kn` | `import "json"` — `json.json_parse` / `json.json_stringify` |
| 03 | `03_strings_and_split.kn` | `import "string"` — `string.split`, `string.join`, `string.trim` |
| 04 | `04_collections_array.kn` | `import("array")` as `Arr` — `Arr.array`, `Arr.map`, `Arr.filter` |
| 05 | `05_control_flow.kn` | `if` / `else`, `for` / `range`, `while`, `break` |
| 06 | `06_errors_try_catch.kn` | `try` / `catch` / `throw`, `Error`, `error_message` |
| 07 | `07_lambda.kn` | `lambda (x) => …`, nested lambdas, `map` with a function |
| 08 | `08_sys_cli_time.kn` | `import "sys"` — `kern_version`, `cli_args`, `monotonic_time` |
| 09 | `09_random.kn` | `import "random" as rnd` — `random_int`, `random_choice` |

After this folder, continue with **`../basic/README.md`** (full basics table) and **`../golden/README.md`** (async, events).
