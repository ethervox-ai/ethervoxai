@echo off
REM Wrapper for installing alternative audio deps

setlocal
cd /d "%~dp0"

if not exist "scripts\setup\install-alternatives.bat" (
	echo ERROR: Target script not found: scripts\setup\install-alternatives.bat
	exit /b 1
)

call scripts\setup\install-alternatives.bat
exit /b %errorlevel%
