# Removed nested `kern/` directory

The historical duplicate tree at repository root `kern/` (parallel `cli/`, `compiler/`, `runtime/`, etc.) was **not** wired into the main CMake build; the real toolchain lives under `src/` and `lib/kern/`.

As part of the Kern / Kern-IDE split, that entire directory was removed. The Qt-based editor that lived under `kern/ide/qt-native/` now ships in the **Kern-IDE** repository as `native-qt/`.

If you still have a local `kern-ide/` folder at the repo root (duplicate of `Kern-IDE/`), delete it after closing any program that holds the path open, then use only `Kern-IDE/` for editor sources.
