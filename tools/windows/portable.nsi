Unicode true

!ifndef VERSION
    !error "VERSION is required"
!endif
!ifndef STAGE_DIR
    !error "STAGE_DIR is required"
!endif
!ifndef PAYLOAD_ID
    !error "PAYLOAD_ID is required"
!endif
!ifndef SOURCE_ROOT
    !error "SOURCE_ROOT is required"
!endif
!ifndef OUTPUT_FILE
    !error "OUTPUT_FILE is required"
!endif

Name "Spool for Jellyfin Portable"
OutFile "${OUTPUT_FILE}"
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
SetCompressor /SOLID lzma
InstallDir "$LocalAppData\Spool for Jellyfin\Portable\${VERSION}-${PAYLOAD_ID}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "Spool for Jellyfin Portable"
VIAddVersionKey /LANG=1033 "FileDescription" "Spool for Jellyfin portable launcher"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Spool for Jellyfin contributors"

Section
    IfFileExists "$InstDir\.payload-complete" launch

    SetOutPath "$InstDir"
    File /r "${STAGE_DIR}\*"

    FileOpen $0 "$InstDir\.payload-complete" w
    FileWrite $0 "${PAYLOAD_ID}"
    FileClose $0

launch:
    ClearErrors
    Exec '"$InstDir\jellyfin-native.exe"'
    IfErrors 0 done
    MessageBox MB_ICONSTOP "Spool for Jellyfin could not be launched."
    SetErrorLevel 1

done:
SectionEnd
