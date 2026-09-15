param([string]$OfRoot = (Resolve-Path "$PSScriptRoot/../../../../..").Path)
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../..").Path
python "$PSScriptRoot/verify_dependencies.py" --of-root $OfRoot
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed' }
$VsWhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$MSBuild = & $VsWhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
if (!$MSBuild) { throw 'Visual Studio with C++ build tools is required' }
& $MSBuild "$ProjectRoot/guipper.vcxproj" /p:Configuration=Release /p:Platform=x64 /m:2
if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }
