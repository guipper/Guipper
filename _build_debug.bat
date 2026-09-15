@echo off
cd /d "%~dp0"
if exist "obj\x64\Debug" rmdir /s /q "obj\x64\Debug"
msbuild guipper.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Rebuild /m

