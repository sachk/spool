function(jellyfin_resolve_android_dependencies)
    set(ANDROID_DEPS_PREFIX "" CACHE PATH "Android media dependency prefix")
    find_path(MPV_INCLUDE_DIR mpv/client.h
        HINTS "${ANDROID_DEPS_PREFIX}/include"
        REQUIRED
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    find_library(MPV_LIBRARY
        NAMES mpv libmpv
        HINTS "${ANDROID_DEPS_PREFIX}/lib"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        REQUIRED
    )
    if(NOT TARGET MPV::MPV)
        add_library(MPV::MPV SHARED IMPORTED)
        set_target_properties(MPV::MPV PROPERTIES
            IMPORTED_LOCATION "${MPV_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
        )
    endif()
endfunction()

function(jellyfin_configure_android_targets native_target core_target)
    target_sources(${core_target} PRIVATE
        src/platform/android/AndroidPlatform.cpp
        src/platform/desktop/UnsupportedPerformanceSampler.cpp
        src/platform/desktop/DesktopMpvConfigPolicy.cpp
        src/platform/desktop/DesktopPlaybackSurface.cpp
    )
    target_sources(${native_target} PRIVATE
        src/platform/android/AndroidNativeAppWindow.cpp
        src/platform/android/AndroidApplicationServices.cpp
        src/platform/android/AndroidProcessIntegration.cpp
        src/platform/desktop/DesktopPlaybackRuntime.cpp
    )
    target_link_libraries(${core_target} PUBLIC MPV::MPV)
    # liblog carries diagnostics to logcat, the only readable output on Android.
    target_link_libraries(${native_target} PRIVATE log)
    target_compile_definitions(${core_target} PRIVATE SPOOL_ANDROID=1)
    target_compile_definitions(${native_target} PRIVATE SPOOL_ANDROID=1)
    if(SPOOL_ANDROID_TV)
        set(android_package_source "${CMAKE_CURRENT_SOURCE_DIR}/app/android-tv")
        set(android_package_name "com.sachk.spool.tv")
        target_compile_definitions(${core_target} PRIVATE SPOOL_ANDROID_TV=1)
        target_compile_definitions(${native_target} PRIVATE SPOOL_ANDROID_TV=1)
    else()
        set(android_package_source "${CMAKE_CURRENT_SOURCE_DIR}/app/android-phone")
        set(android_package_name "com.sachk.spool")
    endif()

    # Android rejects versioned sonames, so bundle only plain .so files, and only
    # from the Android dependency prefix.
    file(GLOB ANDROID_EXTRA_LIBS "${ANDROID_DEPS_PREFIX}/lib/*.so")
    set_property(TARGET ${native_target} PROPERTY QT_ANDROID_EXTRA_LIBS "${ANDROID_EXTRA_LIBS}")
    set_property(TARGET ${native_target} PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR "${android_package_source}")
    set_property(TARGET ${native_target} PROPERTY QT_ANDROID_PACKAGE_NAME "${android_package_name}")
    set_property(TARGET ${native_target} PROPERTY QT_ANDROID_APP_NAME "Spool for Jellyfin")
    set_property(TARGET ${native_target} PROPERTY QT_ANDROID_MIN_SDK_VERSION 28)
    set_property(TARGET ${native_target} PROPERTY QT_ANDROID_TARGET_SDK_VERSION 36)
    set_property(TARGET ${native_target} PROPERTY QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}")
endfunction()
