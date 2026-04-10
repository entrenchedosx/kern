@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM Forward to kernc.exe beside this file (launcher on Windows rebuilds then runs kern-impl.exe).
"%~dp0kernc.exe" %*
set "EC=!ERRORLEVEL!"
if not "!EC!"=="0" (
  echo.
  echo kernc.exe failed with exit code !EC!
  pause
)
exit /b !EC!
