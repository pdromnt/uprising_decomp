@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0source\scripts\build.ps1" %*
exit /b %errorlevel%
