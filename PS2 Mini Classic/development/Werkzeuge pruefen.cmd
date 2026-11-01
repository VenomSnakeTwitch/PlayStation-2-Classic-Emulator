@echo off
start "" /wait "%~dp0..\updates\ClassicMiniUpdater.exe" --project "%~dp0." --install "%~dp0.." --verify-tools
type "%~dp0..\updates\status.txt"
pause
