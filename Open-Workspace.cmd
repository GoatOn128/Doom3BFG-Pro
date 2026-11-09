@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT="

if exist "%SCRIPT_DIR%CMakeLists.txt" if exist "%SCRIPT_DIR%.vscode\settings.json" set "PROJECT_ROOT=%SCRIPT_DIR%"

if not defined PROJECT_ROOT if exist "%SCRIPT_DIR%..\CMakeLists.txt" if exist "%SCRIPT_DIR%..\.vscode\settings.json" for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_ROOT=%%~fI"

if not defined PROJECT_ROOT (
    echo [Open-Workspace] ERROR: Could not locate the project root.
    exit /b 1
)

where code >nul 2>nul
if not errorlevel 1 (
    start "" code --new-window "%PROJECT_ROOT%"
    exit /b 0
)
if exist "%LOCALAPPDATA%\Programs\Microsoft VS Code\Code.exe" (
    start "" "%LOCALAPPDATA%\Programs\Microsoft VS Code\Code.exe" --new-window "%PROJECT_ROOT%"
    exit /b 0
)
if exist "%ProgramFiles%\Microsoft VS Code\Code.exe" (
    start "" "%ProgramFiles%\Microsoft VS Code\Code.exe" --new-window "%PROJECT_ROOT%"
    exit /b 0
)
if defined ProgramFiles(x86) if exist "%ProgramFiles(x86)%\Microsoft VS Code\Code.exe" (
    start "" "%ProgramFiles(x86)%\Microsoft VS Code\Code.exe" --new-window "%PROJECT_ROOT%"
    exit /b 0
)

exit /b 1