@echo off
setlocal
cd /d "%~dp0"
call tests\run_stable.cmd %*
