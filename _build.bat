@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86
msbuild "D:\Programacion\of_v0.11.2_vs2017_release\apps\myApps\guipper4\guipper.sln" /p:Platform=Win32 /p:Configuration=Release /t:Build
exit /b %ERRORLEVEL%
