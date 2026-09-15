@echo off
cd /d "%~dp0"
if exist "obj\x64\Release" rmdir /s /q "obj\x64\Release"
msbuild guipper.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Rebuild /m

