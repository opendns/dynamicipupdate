!addplugindir bin
!include "MUI.nsh"
SetCompressor /SOLID lzma

!define NLPC              "OpenDNS Updater"
!define FULL_NAME         "OpenDNS Updater"
!define UI_EXE_NAME       "OpenDNSUpdater.exe"
!define SHORTCUT          "$SMPROGRAMS\${FULL_NAME}.lnk"

InstallDir "$PROGRAMFILES\${FULL_NAME}"
InstallDirRegKey HKCU "Software\${FULL_NAME}" ""

!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start OpenDNS Updater"
!define MUI_FINISHPAGE_RUN_FUNCTION "RunUI"

Function RunUI
    UAC::Exec "" "$INSTDIR\${UI_EXE_NAME}" "" ""
    ;ExecShell "" "$INSTDIR\${UI_EXE_NAME}" "" SW_SHOWNORMAL
FunctionEnd

Function GuiOnInstall
    ExecWait '"$INSTDIR\${UI_EXE_NAME}" /install'
FunctionEnd

Function un.GuiOnUninstall
    ExecWait '"$INSTDIR\${UI_EXE_NAME}" /uninstall'
FunctionEnd

; Installer strings
Name "${FULL_NAME}"
Caption "${FULL_NAME}"

; Name of the installer
OutFile "OpenDNS-Updater-${VERSION}.exe"

XPStyle on

; UAC: http://nsis.sourceforge.net/UAC_plug-in
RequestExecutionLevel user

Function .OnInit
UAC_Elevate:
    UAC::RunElevated 
    StrCmp 1223 $0 UAC_ElevationAborted ; UAC dialog aborted by user?
    StrCmp 1062 $0 UAC_ErrNoService ; 1062 error => Secondary Logon service not running
    StrCmp 0 $0 0 UAC_Err ; Error?
    StrCmp 1 $1 0 UAC_Success ;Are we the real deal or just the wrapper?
    Quit
UAC_Err:
    MessageBox mb_iconstop "Unable to elevate, error $0"
    Abort
UAC_ErrNoService:
    MessageBox mb_iconstop "Administrative rights are required to install the application. Please switch to an Administrator user to install it."
    Abort
UAC_ElevationAborted:
    MessageBox mb_iconstop "This installer requires admin access, aborting!"
    Abort
UAC_Success:
    StrCmp 1 $3 +4 ;Admin?
    StrCmp 3 $1 0 UAC_ElevationAborted ;Try again?
    MessageBox mb_iconstop "This installer requires admin access, try again"
    goto UAC_Elevate 
FunctionEnd

Function .OnInstFailed
    UAC::Unload
FunctionEnd 

Function .OnInstSuccess
    UAC::Unload
FunctionEnd

Function un.OnInit
UAC_Elevate:
    UAC::RunElevated 
    StrCmp 1223 $0 UAC_ElevationAborted ; UAC dialog aborted by user?
    StrCmp 1062 $0 UAC_ErrNoService ; 1062 error => Secondary Logon service not running
    StrCmp 0 $0 0 UAC_Err ; Error?
    StrCmp 1 $1 0 UAC_Success ;Are we the real deal or just the wrapper?
    Quit
UAC_Err:
    MessageBox mb_iconstop "Unable to elevate, error $0"
    Abort
UAC_ErrNoService:
    MessageBox mb_iconstop "Administrative rights are required to install the application. Please switch to an Administrator user to install it."
    Abort
UAC_ElevationAborted:
    MessageBox mb_iconstop "This installer requires admin access, aborting!"
    Abort
UAC_Success:
    StrCmp 1 $3 +4 ;Admin?
    StrCmp 3 $1 0 UAC_ElevationAborted ;Try again?
    MessageBox mb_iconstop "This installer requires admin access, try again"
    goto UAC_Elevate 
FunctionEnd

Function un.OnUninstFailed
    UAC::Unload
FunctionEnd 

Function un.OnUninstSuccess
    UAC::Unload
FunctionEnd

; Pages in the installer
!insertmacro MUI_PAGE_WELCOME
;!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Pages in the uninstaller
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Set languages (first is default language)
!insertmacro MUI_LANGUAGE English

!define REG_UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\${FULL_NAME}"

; Install files and registry keys
Section "Install" SecInstall
    SetOutPath "$INSTDIR"
    SetOverwrite on

    ; old client's name
    Processes::KillProcessAndWait "OpenDNS Updater.exe"

    ; our old name
    Processes::KillProcessAndWait "OpenDNSDynamicIp.exe"

    ; our current name
    Processes::KillProcessAndWait "${UI_EXE_NAME}"
    ; empirically we need to sleep a while before the file can be overwritten
    Sleep 1000

    File /oname=${UI_EXE_NAME} UpdaterUI\Release\OpenDNSUpdater.exe

    ;File /oname=${UI_EXE_NAME} UpdaterUI\Debug\OpenDNSUpdater.exe

    Call GuiOnInstall

    ; Uninstaller
    WriteRegStr   HKCU "Software\${FULL_NAME}" "" "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr   HKLM "${REG_UNINSTALL}" "DisplayName" "${FULL_NAME} ${VERSION}"
    WriteRegStr   HKLM "${REG_UNINSTALL}" "DisplayVersion" "${VERSION}"
    WriteRegStr   HKLM "${REG_UNINSTALL}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoModify" 1
    WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoRepair" 1

    ; Start Menu shortcut
    CreateShortCut "${SHORTCUT}" "$INSTDIR\${UI_EXE_NAME}" "" "$INSTDIR\${UI_EXE_NAME}" 0

SectionEnd

Section "Uninstall"
    Call un.GuiOnUninstall

    Processes::KillProcessAndWait "${UI_EXE_NAME}"
    Sleep 1000

    ; Remove registry keys
    DeleteRegKey HKLM "${REG_UNINSTALL}"
    DeleteRegKey HKCU "Software\${FULL_NAME}"

    ; Remove Start Menu shortcut
    Delete "${SHORTCUT}"

    ; Remove files and uninstaller
    RMDir /r "$INSTDIR"

    ; Remove application data folder
    ; This is safe because the user doesn't get a say in where this is installed
    ; TODO: temporarily (?) disable deleting appdata folder, to preserve logs
    ; maybe we should just never delete this folder (or just delete settings.dat)?
    ;RMDir /r "$APPDATA\${NLPC}"

SectionEnd

UninstallIcon "UpdaterUI\res\OpenDNSUpdater.ico"
Icon "UpdaterUI\res\OpenDNSUpdater.ico"
