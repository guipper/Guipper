param([string]$OfRoot = (Resolve-Path "$PSScriptRoot/../../../../..").Path,
      [string]$Python = 'python', [string]$Output = '')
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path "$PSScriptRoot/../..").Path
& $Python "$PSScriptRoot/verify_dependencies.py" --of-root $OfRoot
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed' }
if ($env:GUIPPER_WINSPARKLE_SDK) {
    $Channels = Get-Content "$ProjectRoot/release/windows-update-sdk.json" | ConvertFrom-Json
    & $Python "$PSScriptRoot/configure_updates.py" --platform windows --stable $Channels.stable --beta $Channels.beta --public-key "$ProjectRoot/$($Channels.public_key)"
    if ($LASTEXITCODE -ne 0) { throw 'Update configuration failed' }
}
$VsWhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$MSBuild = & $VsWhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
if (!$MSBuild) { throw 'Visual Studio with C++ build tools is required' }
$BuildArgs = @("$ProjectRoot/guipper.vcxproj", '/p:Configuration=Release', '/p:Platform=x64', '/m:2')
if ($Output) {
    $Output = [System.IO.Path]::GetFullPath($Output)
    $BuildArgs += "/p:OutDir=$Output/"
}
& $MSBuild @BuildArgs
if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }
if ($env:GUIPPER_WINSPARKLE_SDK) {
    $Destination = if ($Output) { $Output } else { "$ProjectRoot/bin" }
    Copy-Item "$env:GUIPPER_WINSPARKLE_SDK/x64/Release/WinSparkle.dll" $Destination
}
