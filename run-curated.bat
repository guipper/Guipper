@echo off
setlocal
set "PROJECT_DIR=%~dp0"
if not defined GUIPPER_CURATED_LIST set "GUIPPER_CURATED_LIST=%PROJECT_DIR%release\shader-curated.json"
if not defined GUIPPER_USER_ROOT set "GUIPPER_USER_ROOT=%PROJECT_DIR%dist\curated-profile"
set "GUIPPER_USER_LIST="
if not exist "%PROJECT_DIR%bin\Guipper.exe" (
  echo Guipper.exe not found. Build Release x64 with scripts\release\build-windows.ps1 first.
  pause
  exit /b 1
)
pushd "%PROJECT_DIR%bin"
Guipper.exe %*
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
