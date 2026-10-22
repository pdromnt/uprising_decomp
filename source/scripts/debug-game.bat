@echo off
where.exe pwsh.exe >nul 2>nul
if errorlevel 1 (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0debug-game.ps1" %*
) else (
    pwsh.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0debug-game.ps1" %*
)
set "debug_result=%errorlevel%"
echo.
pause
exit /b %debug_result%
