@echo off
cd /d D:\Programacion\of_v0.11.2_vs2017_release\apps\myApps\guipper4
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe" guipper.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Build
