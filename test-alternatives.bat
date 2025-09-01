@echo off
REM Wrapper to run audio alternatives test

setlocal
cd /d "%~dp0"

if not exist "scripts\testing\test-alternatives.bat" (
	echo ERROR: Target script not found: scripts\testing\test-alternatives.bat
	exit /b 1
)

call scripts\testing\test-alternatives.bat
exit /b %errorlevel%
