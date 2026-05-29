@echo off
setlocal

powershell -ExecutionPolicy Bypass -File "%~dp0flash-stm32-uart.ps1" %*

exit /b %errorlevel%
