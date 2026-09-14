@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\release\build-windows.ps1" %*
exit /b %errorlevel%
