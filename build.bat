@echo off
REM Build script for n02 Kaillera DLL
REM This builds the 64-bit Release version

set PROJECT="%~dp0n02p.vcxproj"

REM Try to find MSBuild in common locations
set MSBUILD=

REM VS2026 (v18) Community
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2026 (v18) Professional
if exist "C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2026 (v18) Enterprise
if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2022 Community
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2022 Professional
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2022 Enterprise
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2022 Build Tools
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2019 Build Tools
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

REM VS2019 Community
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
    goto :found
)

echo ERROR: Could not find MSBuild.exe
echo Please install Visual Studio 2019 or 2022 with C++ build tools
pause
exit /b 1

:found
echo Found MSBuild: %MSBUILD%
echo.
echo Building n02 (64-bit Release)...
%MSBUILD% %PROJECT% /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0 /p:PlatformToolset=v145

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Output: %~dp0x64\Release\n02p.dll
    echo.
    echo To use with RMG, copy n02p.dll to your RMG Bin\Release folder as kailleraclient.dll
) else (
    echo.
    echo Build failed with error code %ERRORLEVEL%
)

pause
