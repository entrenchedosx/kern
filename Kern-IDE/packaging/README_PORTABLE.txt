Kern IDE — Windows x64 portable
================================

Run kern-ide.exe (no installer). First launch creates next to the exe:

  kern_versions\   — downloaded Kern compilers
  config\          — settings.json (active compiler, auto-update)
  logs\            — install.log, runtime.log
  projects\        — default workspace when no folder is chosen

Optional: set environment variable KERN_IDE_HOME to override the data root.

Use the "Kern versions" tab to download compilers from GitHub; or choose
"(development PATH)" and set KERN_EXE to a local kern.exe.
