@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM From repo root: run build\Release\kernc.exe (Windows launcher rebuilds kernc then runs kern-impl).
set "ROOT=%~dp0"
set "KERN=%ROOT%build\Release\kernc.exe"
if not exist "%KERN%" (
  echo ERROR: %KERN% not found. Run: cmake -B build -S . and build target kern_launcher or kernc
  echo.
  pause
  exit /b 1
)
"%KERN%" %*
set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo kernc failed with exit code !EC!  (Is CMake on PATH? Is the build directory configured?)
  pause
)
exit /b !EC!
