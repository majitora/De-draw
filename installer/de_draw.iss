[Setup]
AppId={{6F0C8E1B-DE0D-4A55-9C3E-0123456789AB}
AppName=de_draw VST3 (by maji)
AppVersion=1.0.0
CreateAppDir=no
OutputDir=output
OutputBaseFilename=de_draw_setup
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2

[Files]
Source: "..\build\DeDraw_artefacts\Release\VST3\de_draw.vst3\*"; DestDir: "{commoncf64}\VST3\de_draw.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
