@echo off
REM Forward to kernc.exe beside this file (launcher on Windows rebuilds then runs kern-impl.exe).
"%~dp0kernc.exe" %*
exit /b %ERRORLEVEL%
