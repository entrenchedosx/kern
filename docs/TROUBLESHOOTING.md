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

- Ensure the runtime binary exists in `build\Release\`.
- Launch the IDE from repo root:
  ```powershell
  python .\kern-ide\main.py
  ```

## Graphics or game scripts fail

- Build with game/graphics support enabled, then rebuild and re-run tests.

