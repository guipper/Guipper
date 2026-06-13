@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
del /f /s /q "D:\Programacion\of_v0.11.2_vs2017_release\apps\myApps\guipper4\obj\x64\Release\*.*" 2>nul
msbuild "D:\Programacion\of_v0.11.2_vs2017_release\apps\myApps\guipper4\guipper.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /m:1
