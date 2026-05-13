# Troubleshooting

## Build fails

- Reconfigure and rebuild:
  ```powershell
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --config Release --target kern kern_repl kernc
  ```
- If a binary is locked on Windows, close running `kern.exe` / `kern_repl.exe` first.

## `kern.exe` not found

- Verify `.\build\Release\kern.exe` exists.
- Run commands from repo root, or pass absolute paths.

## Import path errors

- Run from repo root so `lib/kern/` resolves predictably.
- Keep project imports relative to your repo/project layout.

## IDE launches but run/check fails

- Ensure **`kern.exe`** is on `PATH` or set **`KERN_EXE`**, or place `kern.exe` under **`Kern-IDE/desktop-tk/kern_version/`**.
- Run from **`Kern-IDE/desktop-tk`**: `python main.py`
- For imports from **`lib/kern/`**, set **`KERN_LIB`** to the Kern repository root. See [Kern-IDE/docs/INTEGRATION.md](../Kern-IDE/docs/INTEGRATION.md).

## Graphics or game scripts fail

- Build with game/graphics support enabled, then rebuild and re-run tests.

## VM / bytecode edge cases

The VM defines reserved opcodes such as `NEW_INSTANCE`, `INVOKE_METHOD`, and `LOAD_THIS` for possible future class dispatch. The **compiler does not emit them** in normal builds. If you see `Opcode not implemented` at runtime, the bytecode did not come from this compiler pipeline (or you are on a mismatched toolchain).

## `run_all_tests.ps1` finds no scripts

`run_all_tests.ps1` walks **`examples/` recursively**. Run it from the repo root and pass `-Examples` if your tree lives elsewhere.

