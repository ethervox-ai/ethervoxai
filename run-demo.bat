@echo off
REM EthervoxAI launcher (wrapper)
REM Forwards to scripts\demo\run-demo.bat

setlocal
cd /d "%~dp0"

if not exist "scripts\demo\run-demo.bat" (
	echo ERROR: Target script not found: scripts\demo\run-demo.bat
	exit /b 1
)

call scripts\demo\run-demo.bat
exit /b %errorlevel%
