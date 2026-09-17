@echo off
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
if not exist obj mkdir obj
cl /nologo /EHsc /std:c++17 /O2 tests\audio_core_tests.cpp src\JPutils\jp_audio_analyzer.cpp /Fe:obj\audio_core_tests.exe /Fo:obj\ /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
obj\audio_core_tests.exe
if errorlevel 1 exit /b 1
if not "%~1"=="--loopback" exit /b 0
cl /nologo /EHsc /std:c++17 /O2 tests\audio_loopback_windows.cpp src\JPutils\jp_audio_loopback.cpp /Fe:obj\audio_loopback_windows.exe /Fo:obj\ /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
obj\audio_loopback_windows.exe
exit /b %errorlevel%
