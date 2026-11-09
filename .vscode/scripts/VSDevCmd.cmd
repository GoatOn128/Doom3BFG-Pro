@echo off
rem Initialize the Visual Studio 2022 developer environment for Win32 (x86).
rem
rem Usage:
rem   call .\.vscode\scripts\VSDevCmd.cmd

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [VSDevCmd] ERROR: vswhere.exe not found. Install Visual Studio 2022 with C++ tools.
    exit /b 1
)

set "VS_PATH="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%I"
if not defined VS_PATH (
    echo [VSDevCmd] ERROR: No Visual Studio 2022 installation with the x86/x64 C++ toolset found.
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x86 -no_logo
exit /b %ERRORLEVEL%