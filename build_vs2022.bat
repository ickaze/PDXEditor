@echo off
setlocal

cd /d "%~dp0"

set "SOLUTION=PDXEditor.sln"
set "CONFIGURATION=Release"
set "PLATFORM=x64"
set "MSBUILD="

rem 1. MSBuild.exe already available in PATH
for %%I in (MSBuild.exe) do (
    if not "%%~$PATH:I"=="" (
        set "MSBUILD=%%~$PATH:I"
        goto :found
    )
)

rem 2. Explicit Visual Studio 2022 installation path
if defined VS2022_PATH (
    if exist "%VS2022_PATH%\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=%VS2022_PATH%\MSBuild\Current\Bin\MSBuild.exe"
        goto :found
    )
)

rem 3. Visual Studio Developer Command Prompt environment
if defined VSINSTALLDIR (
    if exist "%VSINSTALLDIR%MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=%VSINSTALLDIR%MSBuild\Current\Bin\MSBuild.exe"
        goto :found
    )
)

rem 4. Standard Visual Studio 2022 installation paths
if defined ProgramFiles (
    call :check_vs "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    if defined MSBUILD goto :found
    call :check_vs "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
    if defined MSBUILD goto :found
    call :check_vs "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
    if defined MSBUILD goto :found
    call :check_vs "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
    if defined MSBUILD goto :found
)

if defined ProgramFiles(x86) (
    call :check_vs "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
    if defined MSBUILD goto :found
    call :check_vs "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional"
    if defined MSBUILD goto :found
    call :check_vs "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise"
    if defined MSBUILD goto :found
    call :check_vs "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
    if defined MSBUILD goto :found
)

echo.
echo ERROR: Visual Studio 2022 MSBuild.exe was not found.
echo.
echo Run this file from a Visual Studio 2022 Developer Command Prompt,
echo or set VS2022_PATH to the Visual Studio 2022 installation directory.
echo.
echo Example:
echo   set VS2022_PATH=D:\VisualStudio\2022\Community
echo   build_vs2022.bat
echo.
pause
exit /b 1

:found
echo.
echo MSBuild:
echo   "%MSBUILD%"
echo.
echo Building %SOLUTION%
echo Configuration: %CONFIGURATION%
echo Platform:      %PLATFORM%
echo.

"%MSBUILD%" "%SOLUTION%" /m /t:Rebuild /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM%

if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    pause
    exit /b 1
)

echo.
echo BUILD SUCCEEDED.
echo Output:
echo   x64\Release\PDXEditor.exe
echo.
pause
exit /b 0

:check_vs
if exist "%~1\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=%~1\MSBuild\Current\Bin\MSBuild.exe"
)
exit /b
