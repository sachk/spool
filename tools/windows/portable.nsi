Unicode true

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

Name "Jellyfin Native Portable"
OutFile "${OUTPUT_FILE}"
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "Jellyfin Native Portable"
VIAddVersionKey /LANG=1033 "FileDescription" "Jellyfin Native portable launcher"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Jellyfin Native contributors"

Section
    InitPluginsDir
    SetOutPath "$PluginsDir\Jellyfin Native"
    File /r "${STAGE_DIR}\*"
    File /oname=LICENSE.txt "${SOURCE_ROOT}\LICENSE"

    ExecWait '"$PluginsDir\Jellyfin Native\jellyfin-native.exe"' $0
    SetErrorLevel $0
SectionEnd
