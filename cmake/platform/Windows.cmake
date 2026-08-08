function(jellyfin_resolve_windows_dependencies)
    set(MPV_ROOT "" CACHE PATH "Windows libmpv SDK prefix")
    find_path(MPV_INCLUDE_DIR mpv/client.h
        HINTS "${MPV_ROOT}/include"
        REQUIRED
    )
    find_library(MPV_LIBRARY
        NAMES mpv libmpv
        HINTS "${MPV_ROOT}/lib"
        REQUIRED
    )
    if(NOT TARGET MPV::MPV)
        add_library(MPV::MPV UNKNOWN IMPORTED)
        set_target_properties(MPV::MPV PROPERTIES
            IMPORTED_LOCATION "${MPV_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
        )
    endif()
endfunction()

function(jellyfin_configure_windows_targets native_target core_target)
    target_sources(${core_target} PRIVATE
        src/platform/windows/WindowsSettingsPolicy.cpp
        src/platform/windows/WindowsCredentialStore.cpp
        src/platform/windows/WindowsSystemProbes.cpp
        src/platform/windows/WindowsPlatformPaths.cpp
        src/platform/desktop/UnsupportedPerformanceSampler.cpp
    )
    target_sources(${native_target} PRIVATE
        src/platform/windows/WindowsPlatformCapabilities.cpp
        src/platform/windows/WindowsScreenSaverInhibitor.cpp
        src/platform/windows/WindowsPlatformStartup.cpp
        src/platform/windows/WindowsProcessIntegration.cpp
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform/windows-version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/jellyfin-native-version.rc"
        @ONLY
    )
    target_sources(${native_target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/jellyfin-native-version.rc")
    target_link_libraries(${core_target} PUBLIC MPV::MPV Advapi32)
endfunction()
