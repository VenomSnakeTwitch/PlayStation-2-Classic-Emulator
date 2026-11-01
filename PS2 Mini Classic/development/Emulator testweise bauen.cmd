@echo off
echo Der vollstaendige Test-Build kann mehrere Minuten dauern.
start "" /wait "%~dp0..\updates\ClassicMiniUpdater.exe" --project "%~dp0." --install "%~dp0.." --verify-build
type "%~dp0..\updates\status.txt"
pause
