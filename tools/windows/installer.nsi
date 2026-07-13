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

Name "Jellyfin Native"
OutFile "${OUTPUT_FILE}"
InstallDir "$LocalAppData\Programs\Jellyfin Native"
InstallDirRegKey HKCU "Software\Jellyfin Native" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "Jellyfin Native"
VIAddVersionKey /LANG=1033 "FileDescription" "Jellyfin Native installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Jellyfin Native contributors"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$InstDir\jellyfin-native.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Jellyfin Native"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_ROOT}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Jellyfin Native" SEC_APP
    SectionIn RO
    SetShellVarContext current
    SetOutPath "$InstDir"
    File /r "${STAGE_DIR}\*"
    File /oname=LICENSE.txt "${SOURCE_ROOT}\LICENSE"

    WriteUninstaller "$InstDir\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\Jellyfin Native"
    CreateShortcut "$SMPROGRAMS\Jellyfin Native\Jellyfin Native.lnk" "$InstDir\jellyfin-native.exe"
    CreateShortcut "$SMPROGRAMS\Jellyfin Native\Uninstall Jellyfin Native.lnk" "$InstDir\Uninstall.exe"

    WriteRegStr HKCU "Software\Jellyfin Native" "InstallDir" "$InstDir"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native" \
        "DisplayName" "Jellyfin Native"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native" \
        "DisplayIcon" "$InstDir\jellyfin-native.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native" \
        "InstallLocation" "$InstDir"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native" \
        "UninstallString" '"$InstDir\Uninstall.exe"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native" \
        "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native" \
        "NoRepair" 1
SectionEnd

Section "Uninstall"
    SetShellVarContext current
    Delete "$SMPROGRAMS\Jellyfin Native\Jellyfin Native.lnk"
    Delete "$SMPROGRAMS\Jellyfin Native\Uninstall Jellyfin Native.lnk"
    RMDir "$SMPROGRAMS\Jellyfin Native"

    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Jellyfin Native"
    DeleteRegKey HKCU "Software\Jellyfin Native"

    Delete "$InstDir\Uninstall.exe"
    RMDir /r "$InstDir"
SectionEnd
