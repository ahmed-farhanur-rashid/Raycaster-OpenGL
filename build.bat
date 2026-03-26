@echo off
setlocal

:: Always run from the folder where build.bat lives (safe for double-click from Explorer)
cd /d "%~dp0"

:: Add local MinGW to PATH so make and g++ are found without any system install
set PATH=%~dp0mingw64\bin;%PATH%

:: Build
echo Building...
mingw32-make -f Makefile
if errorlevel 1 (
    echo.
    echo Build FAILED. See errors above.
    pause
    exit /b 1
)

echo.
echo Build succeeded! Running build\main.exe ...
echo.
build\main.exe
