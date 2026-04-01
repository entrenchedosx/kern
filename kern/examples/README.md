# Kern Examples

Run from project root (so `lib` and paths resolve):

```powershell
.\build\Release\kern.exe examples\01_hello.kn
```

| File | Description |
|------|-------------|
| 01_hello.kn | Minimal: print |
| 02_variables.kn | let, const, types |
| 03_conditionals.kn | if, elif, else |
| 04_loops.kn | for range(), while |
| 05_functions.kn | def, return, recursion |
| 06_arrays.kn | array(), push, len, index |
| 07_maps.kn | map literal, keys, access |
| 08_strings.kn | str, len, slice |
| 09_import_stdlib.kn | import("math"), import("sys") |
| 10_file_io.kn | write_file, read_file, delete_file |
| 11_try_catch.kn | try/catch, throw, error_message |
| 12_lambda.kn | lambda, call |
| 13_graphics.kn | g2d (requires Raylib build) |
| 14_match.kn | match/case statement |
| 15_bouncy_ball.kn | g2d bouncy ball (requires Raylib) |

Graphics examples (13, 15) need Kern built with Raylib; they exit with a message if g2d is not available.
