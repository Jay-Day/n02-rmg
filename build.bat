@echo off
REM Build script for n02 Kaillera DLL
REM This builds the 64-bit Release version

set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
set PROJECT="%~dp0n02p.vcxproj"

echo Building n02 (64-bit Release)...
%MSBUILD% %PROJECT% /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0.19041.0 /p:PlatformToolset=v142

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
