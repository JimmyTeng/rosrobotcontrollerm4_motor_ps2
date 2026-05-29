@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0install-stm32flash.ps1" %*
exit /b %errorlevel%
