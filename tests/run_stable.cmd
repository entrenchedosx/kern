@echo off
rem Forward to tests/run_stable.ps1 (supports -Quick -BuildDir -WithExamples)
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_stable.ps1" %*
