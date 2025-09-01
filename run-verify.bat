@echo off
REM Wrapper to run verification checks

setlocal
cd /d "%~dp0"

if not exist "scripts\testing\run-verify.bat" (
	echo ERROR: Target script not found: scripts\testing\run-verify.bat
	exit /b 1
)

call scripts\testing\run-verify.bat
exit /b %errorlevel%
