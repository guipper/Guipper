@echo off
cd /d "%~dp0"
msbuild guipper.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Rebuild /m

