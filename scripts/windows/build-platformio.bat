@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0build-platformio.ps1" %*

exit /b %errorlevel%
