@echo off
setlocal
set "REPO_ROOT=%~dp0.."
set "DIST_DIR=%REPO_ROOT%\dist"

where cl >nul 2>nul
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%VSWHERE%" exit /b 1
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
    if not defined VSROOT exit /b 1
    call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
    if errorlevel 1 exit /b 1
)

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
cl /nologo /W4 /O2 /MT /LD "%REPO_ROOT%\tests\test_original.c" /link /OUT:"%DIST_DIR%\mai2hook.dll" user32.lib
exit /b %errorlevel%
