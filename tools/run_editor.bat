@echo off
setlocal

:: Always run from the project root (one level up from tools/)
cd /d "%~dp0.."

:: Add local MinGW to PATH
set PATH=%cd%\mingw64\bin;%PATH%

:: Build the editor if not already built
if not exist build\map_editor.exe (
    echo Building map editor...
    mingw32-make -f Makefile editor
    if errorlevel 1 (
        echo.
        echo Build FAILED. See errors above.
        pause
        exit /b 1
    )
)

echo.
build\map_editor.exe
