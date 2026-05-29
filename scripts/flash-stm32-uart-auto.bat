@echo off
setlocal
rem 无交互：自动扫描串口并完整烧录
powershell -ExecutionPolicy Bypass -File "%~dp0flash-stm32-uart.ps1" -Auto %*
exit /b %errorlevel%
