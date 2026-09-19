param(
    [string]$RuntimeRoot = $env:GUIPPER_WINDOWS_RUNTIME_ROOT,
    [string]$Manifest = "$PSScriptRoot/../../release/windows-runtime.json",
    [string]$Output = "$PSScriptRoot/../../dist/windows",
    [string]$Iscc = $env:ISCC,
    [string]$Python = 'python',
    [string]$Binary = "$PSScriptRoot/../../bin/Guipper.exe"
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../..").Path
if (!$RuntimeRoot -or !(Test-Path $RuntimeRoot -PathType Container)) { throw 'Set GUIPPER_WINDOWS_RUNTIME_ROOT to the reviewed runtime directory' }
if (!$Iscc -or !(Test-Path $Iscc -PathType Leaf)) { throw 'Set ISCC to the Inno Setup compiler' }
$VsWhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$Dumpbin = & $VsWhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC/Tools/MSVC/**/bin/Hostx64/x64/dumpbin.exe' | Select-Object -First 1
if (!$Dumpbin) { throw 'Visual Studio x64 C++ tools (dumpbin) are required' }
& $Python "$PSScriptRoot/windows_runtime.py" --binary $Binary --output $Output --runtime-root $RuntimeRoot --manifest $Manifest --dumpbin $Dumpbin
if ($LASTEXITCODE -ne 0) { throw 'Windows runtime validation or staging failed' }
$Version = (Get-Content "$ProjectRoot/VERSION").Trim()
& $Iscc "/DStageDir=$Output" "/DAppVersion=$Version" "/O$ProjectRoot/dist" "$ProjectRoot/release/guipper.iss"
if ($LASTEXITCODE -ne 0) { throw 'Windows installer compilation failed' }
