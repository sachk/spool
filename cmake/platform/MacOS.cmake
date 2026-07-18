function(jellyfin_resolve_macos_dependencies)
    pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
endfunction()

function(jellyfin_configure_macos_targets native_target core_target)
    target_sources(${core_target} PRIVATE
        src/platform/macos/MacOSSettingsPolicy.cpp
        src/platform/macos/MacOSSystemProbes.cpp
        src/platform/desktop/UnsupportedPerformanceSampler.cpp
    )
    target_sources(${native_target} PRIVATE
        src/platform/macos/MacOSPlatformCapabilities.cpp
        src/platform/macos/MacOSPlatformPaths.cpp
        src/platform/macos/MacOSScreenSaverInhibitor.cpp
        src/platform/macos/MacOSPlatformStartup.cpp
        src/platform/common/UnixProcessIntegration.cpp
    )
    target_link_libraries(${core_target} PUBLIC PkgConfig::MPV)
    target_link_libraries(${native_target} PRIVATE "-framework IOKit")
    set_target_properties(${native_target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Jellyfin Native"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.sachk.tern"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endfunction()
