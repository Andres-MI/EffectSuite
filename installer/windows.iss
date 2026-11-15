; Instalador de Windows (Inno Setup). Se compila en GitHub Actions con: iscc installer\windows.iss
#define AppVersion GetEnv("EFFECTSUITE_VERSION")
#if AppVersion == ""
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{6E1B6C0B-6C0E-4C53-9E55-0E7A3B0F5A11}
AppName=EffectSuite
AppVersion={#AppVersion}
AppPublisher=EffectSuite
DefaultDirName={autopf}\EffectSuite
DefaultGroupName=EffectSuite
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=EffectSuite-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\EffectSuite.exe

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Aplicación y plugin VST3"
Name: "custom"; Description: "Personalizada"; Flags: iscustom

[Components]
Name: "app"; Description: "Aplicación independiente"; Types: full custom; Flags: fixed
Name: "vst3"; Description: "Plugin VST3 (para usar en un DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Crear un icono en el escritorio"; Components: app; Flags: unchecked

[Files]
Source: "..\build\EffectSuite_artefacts\Release\Standalone\EffectSuite.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "..\docs\MANUAL.md"; DestDir: "{app}"; DestName: "Manual.md"; Components: app; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "Licencia.txt"; Components: app; Flags: ignoreversion
Source: "..\build\EffectSuite_artefacts\Release\VST3\EffectSuite.vst3\*"; DestDir: "{commoncf64}\VST3\EffectSuite.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\EffectSuite"; Filename: "{app}\EffectSuite.exe"
Name: "{group}\Manual de EffectSuite"; Filename: "{app}\Manual.md"
Name: "{autodesktop}\EffectSuite"; Filename: "{app}\EffectSuite.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\EffectSuite.exe"; Description: "Abrir EffectSuite"; Flags: nowait postinstall skipifsilent
