@echo off
setlocal EnableExtensions
REM From repo root: run build\Release\kernc.exe (Windows launcher rebuilds kernc then runs kern-impl).
set "ROOT=%~dp0"
set "KERN=%ROOT%build\Release\kernc.exe"
if not exist "%KERN%" (
  echo ERROR: %KERN% not found. Run: cmake -B build -S . and build target kern_launcher or kernc >&2
  exit /b 1
)
"%KERN%" %*
exit /b %ERRORLEVEL%
