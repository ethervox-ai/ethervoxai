@echo off
REM EthervoxAI Windows demo starter (ALT) wrapper
REM Forwards to scripts\demo\start-windows-demo-alt.bat

setlocal
cd /d "%~dp0"

if not exist "scripts\demo\start-windows-demo-alt.bat" (
	echo ERROR: Target script not found: scripts\demo\start-windows-demo-alt.bat
	exit /b 1
)

call scripts\demo\start-windows-demo-alt.bat
exit /b %errorlevel%
