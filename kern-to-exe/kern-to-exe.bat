@echo off
setlocal
cd /d "%~dp0"
python -m kern_to_exe %*
