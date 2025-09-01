@echo off
REM EthervoxAI Windows demo starter (wrapper)
REM Forwards to scripts\demo\start-windows-demo.bat

setlocal
cd /d "%~dp0"

if not exist "scripts\demo\start-windows-demo.bat" (
	echo ERROR: Target script not found: scripts\demo\start-windows-demo.bat
	exit /b 1
)

call scripts\demo\start-windows-demo.bat
exit /b %errorlevel%
