;
; $Id: gnuplot.iss,v 1.10.2.4 2016/07/31 12:18:12 markisch Exp $
;
; GNUPLOT - gnuplot.iss
;
;[
; Copyright 2011,2012   Bastian Märkisch
;
; Permission to use, copy, and distribute this software and its
; documentation for any purpose with or without fee is hereby granted,
; provided that the above copyright notice appear in all copies and
; that both that copyright notice and this permission notice appear
; in supporting documentation.
;
; Permission to modify the software is granted, but not the right to
; distribute the complete modified source code.  Modifications are to
; be distributed as patches to the released version.  Permission to
; distribute binaries produced by compiling modified sources is granted,
; provided you
;   1. distribute the corresponding source modifications from the
;    released version in the form of a patch file along with the binaries,
;   2. add special version identification to distinguish your version
;    in addition to the base release version number,
;   3. provide your name and address as the primary contact for the
;    support of your modified version, and
;   4. retain our contact information in regard to use of the base
;    software.
; Permission to distribute the released version of the source code along
; with corresponding source modifications in the form of a patch file is
; granted with same provisions 2 through 4 for binary distributions.
;
; This software is provided "as is" without express or implied warranty
; to the extent permitted by applicable law.
;]

; 11/2011 Initial version by Bastian Märkisch,
;         Japanese translation by Shigeharu TAKENO
;

#define MyAppName "gnuplot"
#define MyAppVersionShort "5.0"
#define MyAppVersion "5.0 patchlevel 0"
#define MyAppNumVersion "5.0.0"
#define MyAppPublisher "gnuplot development team"
#define MyAppURL "http://www.gnuplot.info/"
#define MyAppExeName "wgnuplot.exe"
#define MyInstallerName "gp500-win32-setup"
#define MyDocuments "%USERPROFILE%\Documents"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppID={{AB419AC3-9BC1-4EC5-A75B-4D8870DD651F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppComments=gnuplot, a famous scientific plotting package.
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
;AppReadme=
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=true
LicenseFile=Copyright
;InfoBeforeFile: README-testing.txt
InfoBeforeFile=README-Windows.txt
InfoAfterFile=RELEASE_NOTES
OutputBaseFilename={#MyInstallerName}
SetupIconFile=bin\grpicon.ico
Compression=lzma2/Max
SolidCompression=true
MinVersion=0,5.01
Uninstallable=true
ChangesEnvironment=true
PrivilegesRequired=admin
UseSetupLdr=true
WindowStartMaximized=true
VersionInfoVersion={#MyAppNumVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=Famous scientific plotting package.
VersionInfoProductName=gnuplot
WindowResizable=false
WindowVisible=false
OutputDir=.
UninstallLogMode=append
AlwaysShowDirOnReadyPage=true
ChangesAssociations=true
ArchitecturesAllowed=
ArchitecturesInstallIn64BitMode=
DisableDirPage=no
DisableProgramGroupPage=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
;Name: ja; MessagesFile: compiler:Languages\Japanese.isl; InfoBeforeFile: README-testing-ja.txt; LicenseFile: Copyright-ja.txt;
Name: ja; MessagesFile: compiler:Languages\Japanese.isl; InfoBeforeFile: README-Windows-ja.txt; LicenseFile: Copyright-ja.txt;
Name: de; MessagesFile: compiler:Languages\German.isl;

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1
Name: defaulttermwin; Description: "windows"; GroupDescription: {cm:defaultterm}; Flags: unchecked exclusive;
Name: defaulttermwxt; Description: "wxt"; GroupDescription: {cm:defaultterm}; Flags: unchecked exclusive;
Name: defaulttermqt; Description: "qt"; GroupDescription: {cm:defaultterm}; Flags: unchecked exclusive;
Name: defaulttermpreserve; Description: {cm:termpreserve}; GroupDescription: {cm:defaultterm}; Flags: exclusive;
Name: associate; Description: "{cm:setassociations}"; GroupDescription: "{cm:other}";
Name: associate\plt; Description: {cm:AssocFileExtension,{#MyAppName},.plt}; GroupDescription: "{cm:other}";
Name: associate\gp; Description: {cm:AssocFileExtension,{#MyAppName},.gp}; GroupDescription: "{cm:other}";
Name: associate\gpl; Description: {cm:AssocFileExtension,{#MyAppName},.gpl}; GroupDescription: "{cm:other}";
Name: associate\dem; Description: {cm:AssocFileExtension,{#MyAppName},.dem}; GroupDescription: "{cm:other}"; Flags: unchecked dontinheritcheck;
Name: modifypath; Description: {cm:path}; GroupDescription: "{cm:other}"; Flags: unchecked

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; core files
Source: "bin\wgnuplot.exe"; DestDir: "{app}\bin\"; Flags: ignoreversion; Components: core
Source: "bin\wgnuplot_pipes.exe"; DestDir: "{app}\bin\"; Flags: ignoreversion skipifsourcedoesntexist; Components: core;
Source: "bin\gnuplot.exe"; DestDir: "{app}\bin\"; Flags: ignoreversion; Components: core
; qt terminal
Source: "bin\gnuplot_qt.exe"; DestDir: "{app}\bin\"; Flags: skipifsourcedoesntexist ignoreversion; Components: core
Source: "bin\platforms\*.dll"; DestDir: "{app}\bin\platforms\"; Flags: skipifsourcedoesntexist ignoreversion; Components: core
; core support files
Source: "bin\*.dll"; DestDir: "{app}\bin\"; Flags: skipifsourcedoesntexist ignoreversion; Components: core
Source: "bin\wgnuplot.mnu"; DestDir: {app}\bin\; Components: core
Source: "bin\wgnuplot.chm"; DestDir: {app}\bin\; Components: core
Source: "share\*"; DestDir: {app}\share\; Flags: recursesubdirs; Components: core
Source: "etc\*"; DestDir: {app}\etc\; Flags: skipifsourcedoesntexist recursesubdirs;  Components: core
; demo files / contrib
Source: "contrib\*"; DestDir: {app}\contrib\; Flags: recursesubdirs; Components: demo
Source: "demo\*"; DestDir: {app}\demo\; Flags: recursesubdirs; Components: demo
; documentation
Source: "NEWS"; DestDir: {app}; Components: core
Source: "README"; DestDir: {app}\docs\; Components: core
Source: "README-Windows.txt"; DestDir: {app}; Components: core
Source: "RELEASE_NOTES"; DestDir: {app}; Components: core
Source: "README-testing.txt"; DestDir: {app}; Flags: skipifsourcedoesntexist; Components: core
Source: "BUGS"; DestDir: {app}\docs\; Components: core
Source: "ChangeLog"; DestDir: {app}\docs\; Components: core
Source: "docs\*"; DestDir: {app}\docs\; Flags: recursesubdirs; Components: docs
; licenses
Source: "Copyright"; DestDir: {app}\license\; Components: core
Source: license\*; DestDir: {app}\license\; Flags: recursesubdirs skipifsourcedoesntexist; Components: license;
; Japanese support
Source: "README-Windows-ja.txt"; DestDir: {app}; Components: ja
Source: "README-testing-ja.txt"; DestDir: {app}; Flags: skipifsourcedoesntexist; Components: ja
Source: "Copyright-ja.txt"; DestDir: {app}; Components: ja
Source: "bin\wgnuplot-ja.chm"; DestDir: {app}\bin; Components: ja
Source: "bin\wgnuplot-ja.mnu"; DestDir: {app}\bin; Components: ja

[Dirs]

[Icons]
Name: "{group}\{#MyAppName} {#MyAppVersionShort}"; Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: {#MyDocuments}; Components: core;
Name: "{group}\{#MyAppName} {#MyAppVersionShort} - console version"; Filename: "{app}\bin\gnuplot.exe"; WorkingDir: {#MyDocuments}; Components: core;
Name: "{group}\{#MyAppName} Help"; Filename: {app}\bin\wgnuplot.chm; Components: core;
Name: "{group}\{#MyAppName} Help (Japanese)"; Filename: {app}\bin\wgnuplot-ja.chm; Components: ja; Flags: CreateOnlyIfFileExists;
Name: "{group}\{#MyAppName} Documentation"; Filename: {app}\docs\gnuplot.pdf; Components: docs; Flags: CreateOnlyIfFileExists;
Name: "{group}\{#MyAppName} FAQ"; Filename: {app}\docs\FAQ.pdf; Components: docs; Flags: CreateOnlyIfFileExists;
Name: "{group}\{#MyAppName} Quick Reference"; Filename: {app}\docs\gpcard.pdf; Components: docs; Flags: CreateOnlyIfFileExists;
Name: "{group}\{#MyAppName} LaTeX Tutorial"; Filename: {app}\docs\tutorial.pdf; Components: docs; Flags: CreateOnlyIfFileExists;
Name: "{group}\{cm:ProgramOnTheWeb,{#MyAppName}}"; Filename: "{#MyAppURL}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName} {#MyAppVersionShort}"; Filename: "{app}\bin\{#MyAppExeName}"; Tasks: desktopicon; WorkingDir: {#MyDocuments}; Components: core;
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName} {#MyAppVersionShort}"; Filename: "{app}\bin\{#MyAppExeName}"; Tasks: quicklaunchicon; WorkingDir: {#MyDocuments}; Components: core;
Name: "{group}\{#MyAppName} Demo Directory"; Filename: {app}\demo; Flags: FolderShortcut; Components: demo;

[Components]
Name: "core"; Description: "{cm:core}"; Types: full compact custom; Flags: fixed
Name: "docs"; Description: "{cm:docs}"; Types: full
Name: "demo"; Description: "{cm:demo}"; Types: full
Name: "license"; Description: "{cm:license}"; Types: full
Name: "ja";  Description: "{cm:japanese}";

[Run]
; view README
Filename: {win}\notepad.exe; Description: {cm:view,README-Windows.txt}; Flags: nowait postinstall skipifsilent Unchecked RunAsOriginalUser ShellExec SkipIfDoesntExist; Parameters: {app}\README-Windows.txt; Languages: en de;
Filename: {win}\notepad.exe; Description: "{cm:view,README-Windows-ja.txt}"; Flags: nowait postinstall skipifsilent Unchecked RunAsOriginalUser ShellExec SkipIfDoesntExist; Parameters: {app}\README-Windows-ja.txt; Languages: ja;
; view RELEASE-NOTES
Filename: {win}\notepad.exe; Description: {cm:view,RELEASE_NOTES}; Flags: nowait postinstall skipifsilent Unchecked RunAsOriginalUser ShellExec SkipIfDoesntExist; Parameters: {app}\RELEASE_NOTES; Languages: en de ja;
; launch gnuplot
Filename: "{app}\bin\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, "&", "&&")}}"; Flags: nowait postinstall skipifsilent Unchecked RunAsOriginalUser; WorkingDir: {#MyDocuments};

[Registry]
; set some environment variables
; set default terminal
Root: HKLM; SubKey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: GNUTERM; ValueData: "windows"; Flags: NoError UninsDeleteValue; Tasks: defaulttermwin;
Root: HKLM; SubKey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: GNUTERM; ValueData: "wxt"; Flags: NoError UninsDeleteValue; Tasks: defaulttermwxt;
Root: HKLM; SubKey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: GNUTERM; ValueData: "qt"; Flags: NoError UninsDeleteValue; Tasks: defaulttermqt;
; include demo directory in gnuplot's search path
Root: HKLM; SubKey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: GNUPLOT_LIB; ValueData: "{app}\demo;{app}\demo\games;{app}\share"; Flags: CreateValueIfDoesntExist NoError UninsDeleteValue; Components: demo;
; easy start in explorer's run dialog
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\wgnuplot.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\wgnuplot.exe"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\gnuplot.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\gnuplot.exe"; Flags: uninsdeletekey
; file associations
Root: HKCR; SubKey: .plt; ValueType: string; ValueData: gnuplot; Flags: uninsdeletevalue; Tasks: associate\plt;
Root: HKCR; SubKey: .gp; ValueType: string; ValueData: gnuplot; Flags: uninsdeletevalue; Tasks: associate\gp;
Root: HKCR; SubKey: .gpl; ValueType: string; ValueData: gnuplot; Flags: uninsdeletevalue; Tasks: associate\gpl;
Root: HKCR; SubKey: .dem; ValueType: string; ValueData: gnuplot; Flags: uninsdeletevalue; Tasks: associate\dem;
Root: HKCR; SubKey: gnuplot; ValueType: string; ValueData: {cm:filetype}; Flags: uninsdeletekey; Tasks: associate;
Root: HKCR; SubKey: gnuplot\DefaultIcon; ValueType: string; ValueData: {app}\bin\{#MyAppExeName},0; Tasks: associate;
Root: HKCR; SubKey: gnuplot\shell\open\command; ValueType: string; ValueData: """{app}\bin\{#MyAppExeName}"" -p ""%1"""; Tasks: associate;

[Code]
(* Modification of the PATH environment variable requires Jared Breland's <jbreland@legroom.net>
   modpath.iss package available at http://www.legroom.net/software/modpath *)
const
    ModPathName = 'modifypath';
    ModPathType = 'system';

function ModPathDir(): TArrayOfString;
begin
    setArrayLength(Result, 1)
    Result[0] := ExpandConstant('{app}\bin');
end;
#include "modpath.iss"

[CustomMessages]
; --------------------------------------------------
; English, default
; --------------------------------------------------
; Components
core=gnuplot Core Components
docs=gnuplot Documentation
demo=gnuplot Demos
license=Third Party License Information
japanese=Japanese Language Support
; tasks
defaultterm=Select gnuplot's default terminal:
termpreserve=Don't change my GNUTERM environment variable
other=Other tasks:
setassociations=Set file associations:
path=Add application directory to your PATH environment variable
; actions
view=View %1
; registry
filetype=gnuplot command script
; --------------------------------------------------
; Japanese
; --------------------------------------------------
; components
ja.core=gnuplot の必要最小限のコンポーネント
; In English, "minimum of gnuplot components"
ja.docs=gnuplot 付属文書
ja.demo=gnuplot サンプルデモスクリプト
; In English, "gnuplot sample demo scripts"
ja.license=使用する外部ライブラリ等のライセンス群
ja.japanese=日本語対応"
; In English, "Japanese language support"
; tasks
ja.defaultterm=gnuplot のデフォルト出力形式 (terminal) の選択:
ja.termpreserve=GNUTERM 環境変数を変更しない
ja.other=Other tasks:
ja.setassociations=ファイルの関連づけを行う:
ja.path=実行ファイルのディレクトリを PATH 環境変数に追加する
; actions
ja.view=%1 を表示する
; registry
ja.filetype=gnuplot コマンドスクリプト
; --------------------------------------------------
; German
; --------------------------------------------------
; components
de.core=gnuplot Kernkomponenten
de.docs=gnuplot Dokumentation
de.demo=gnuplot Demos
de.license=Lizenz-Dateien benutzter Bibliotheken
de.japanese=Japanische Sprachunterstützung
; tasks
de.defaultterm=Standard-Terminal für gnuplot:
de.termpreserve=Umgebungsvariable GNUTERM nicht ändern
de.other=Weitere Aufgaben:
de.setassociations=Verknüpfungen erstellen
de.path=Anwendungsverzeichnis dem Suchpfad PATH hinzufügen
; actions
de.view=%1 anzeigen
; registry
de.filetype=gnuplot Skript
