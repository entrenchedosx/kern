@echo off
cd /d "%~dp0"
python -m kern_build_gui
if errorlevel 1 pause
