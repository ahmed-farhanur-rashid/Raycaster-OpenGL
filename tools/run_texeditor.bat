@echo off
cd /d "%~dp0\.."
if not exist "build\texture_editor.exe" (
    echo Building texture editor...
    mingw64\bin\mingw32-make texeditor
    if errorlevel 1 (
        echo Build failed!
        pause
        exit /b 1
    )
)
build\texture_editor.exe %*
