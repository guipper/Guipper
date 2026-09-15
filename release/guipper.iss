#ifndef StageDir
#error StageDir must name a reviewed staging directory
#endif
#ifndef AppVersion
#error AppVersion must match VERSION
#endif
#if !FileExists(StageDir + "\WINDOWS-RUNTIME-VERIFIED")
#error Windows runtime staging has not been verified; use package-windows.ps1
#endif
[Setup]
AppId={{055458A8-61D7-489E-AB54-BFE382604D3A}
AppName=Guipper
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\Guipper\{#AppVersion}
UsePreviousAppDir=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=Guipper-{#AppVersion}-windows-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Guipper.exe
CloseApplications=no
[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Excludes: "WINDOWS-RUNTIME-VERIFIED"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{autoprograms}\Guipper"; Filename: "{app}\Guipper.exe"; WorkingDir: "{app}"
[Run]
Filename: "{app}\Guipper.exe"; Description: "Open Guipper"; Flags: postinstall nowait skipifsilent
; Versions install side by side. Never delete %LOCALAPPDATA%\Guipper on uninstall.
