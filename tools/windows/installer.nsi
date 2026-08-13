Unicode true

!include "MUI2.nsh"

!ifndef VERSION
    !error "VERSION is required"
!endif
!ifndef STAGE_DIR
    !error "STAGE_DIR is required"
!endif
!ifndef SOURCE_ROOT
    !error "SOURCE_ROOT is required"
!endif
!ifndef OUTPUT_FILE
    !error "OUTPUT_FILE is required"
!endif

Name "Spool for Jellyfin"
OutFile "${OUTPUT_FILE}"
InstallDir "$LocalAppData\Programs\Spool for Jellyfin"
InstallDirRegKey HKCU "Software\Spool for Jellyfin" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "Spool for Jellyfin"
VIAddVersionKey /LANG=1033 "FileDescription" "Spool for Jellyfin installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Spool for Jellyfin contributors"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$InstDir\jellyfin-native.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Spool for Jellyfin"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_ROOT}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Spool for Jellyfin" SEC_APP
    SectionIn RO
    SetShellVarContext current
    SetOutPath "$InstDir"
    File /r "${STAGE_DIR}\*"

    WriteUninstaller "$InstDir\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\Spool for Jellyfin"
    CreateShortcut "$SMPROGRAMS\Spool for Jellyfin\Spool for Jellyfin.lnk" "$InstDir\jellyfin-native.exe"
    CreateShortcut "$SMPROGRAMS\Spool for Jellyfin\Uninstall Spool for Jellyfin.lnk" "$InstDir\Uninstall.exe"

    WriteRegStr HKCU "Software\Spool for Jellyfin" "InstallDir" "$InstDir"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin" \
        "DisplayName" "Spool for Jellyfin"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin" \
        "DisplayIcon" "$InstDir\jellyfin-native.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin" \
        "InstallLocation" "$InstDir"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin" \
        "UninstallString" '"$InstDir\Uninstall.exe"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin" \
        "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin" \
        "NoRepair" 1
SectionEnd

Section "Uninstall"
    SetShellVarContext current
    Delete "$SMPROGRAMS\Spool for Jellyfin\Spool for Jellyfin.lnk"
    Delete "$SMPROGRAMS\Spool for Jellyfin\Uninstall Spool for Jellyfin.lnk"
    RMDir "$SMPROGRAMS\Spool for Jellyfin"

    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Spool for Jellyfin"
    DeleteRegKey HKCU "Software\Spool for Jellyfin"

    Delete "$InstDir\Uninstall.exe"
    RMDir /r "$InstDir"
SectionEnd
