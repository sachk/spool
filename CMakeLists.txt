cmake_minimum_required(VERSION 3.22)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" JELLYFIN_VERSION)
string(STRIP "${JELLYFIN_VERSION}" JELLYFIN_VERSION)
if(NOT JELLYFIN_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version such as 0.2.1")
endif()

project(JellyfinNativeWebOS VERSION "${JELLYFIN_VERSION}" LANGUAGES CXX)

set(JELLYFIN_NATIVE_WEBOS_DEFAULT OFF)
if(CMAKE_CROSSCOMPILING)
    set(JELLYFIN_NATIVE_WEBOS_DEFAULT ON)
endif()
option(JELLYFIN_NATIVE_WEBOS "Build the webOS Starfish/native-surface client" ${JELLYFIN_NATIVE_WEBOS_DEFAULT})
option(JELLYFIN_NATIVE_DEV_BUILD "Favor faster local development builds over smallest release artifacts" OFF)
option(JELLYFIN_DIAGNOSTICS "Build debug lifecycle/process/watchdog diagnostics" OFF)
option(JELLYFIN_DIAGNOSTICS_STACKDUMP "Allow diagnostics to request gdb stack dumps" OFF)
option(JELLYFIN_DIAGNOSTICS_ABORT_ON_HANG "Abort after watchdog hang diagnostics are written" OFF)
include(CTest)

set(MPV_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv" CACHE PATH "desktop mpv source tree")
set(MPV_WEBOS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv_webos" CACHE PATH "webOS mpv source tree")
set(MPV_WEBOS_BUILD_DIR "${MPV_WEBOS_SOURCE_DIR}/build/webos-libmpv" CACHE PATH "webOS libmpv build directory")
if(JELLYFIN_NATIVE_WEBOS)
    set(MPV_ACTIVE_SOURCE_DIR "${MPV_WEBOS_SOURCE_DIR}")
else()
    set(MPV_ACTIVE_SOURCE_DIR "${MPV_SOURCE_DIR}")
endif()

if(JELLYFIN_NATIVE_DEV_BUILD)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

set(JELLYFIN_QT_COMPONENTS
    Core
    Gui
    Network
    Qml
    Quick
    QuickControls2
    Sql
    OpenGL
    LinguistTools
    VirtualKeyboard
    VirtualKeyboardQml
    VirtualKeyboardSettings
    WebSockets
)
if(JELLYFIN_NATIVE_WEBOS)
    set(JELLYFIN_QT_MIN_VERSION 6.11)
    list(APPEND JELLYFIN_QT_COMPONENTS
        GuiPrivate
        QuickLayouts
        Svg
        WaylandClient
    )
else()
    set(JELLYFIN_QT_MIN_VERSION 6.11)
endif()
find_package(Qt6 ${JELLYFIN_QT_MIN_VERSION} REQUIRED COMPONENTS ${JELLYFIN_QT_COMPONENTS})
find_package(QCoro6 0.12 REQUIRED COMPONENTS Core Network)

qt_standard_project_setup(
    REQUIRES ${JELLYFIN_QT_MIN_VERSION}
    I18N_TRANSLATED_LANGUAGES en_US en_GB en_AU
)

if(JELLYFIN_NATIVE_WEBOS)
    pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
    find_library(EGL_LIB EGL REQUIRED)
    find_library(GLES2_LIB GLESv2 REQUIRED)
    find_library(WAYLAND_EGL_LIB wayland-egl REQUIRED)
    find_library(WAYLAND_CLIENT_LIB wayland-client REQUIRED)
    find_library(WAYLAND_WEBOS_CLIENT_LIB wayland-webos-client REQUIRED)
    find_library(WEBOS_HELPERS_LIB helpers REQUIRED)
    find_library(LUNA_SERVICE_LIB luna-service2 REQUIRED)
    set(WEBOS_NATIVE_LIB_DIR "${CMAKE_SYSROOT}/usr/local/webos-native/lib")
    set(WEBOS_SYSROOT_LIB_DIR "${CMAKE_SYSROOT}/usr/lib")
    set(WEBOS_SYSROOT_BASE_LIB_DIR "${CMAKE_SYSROOT}/lib")
    set(AVCODEC_LIB "${WEBOS_NATIVE_LIB_DIR}/libavcodec.so")
    set(AVFILTER_LIB "${WEBOS_NATIVE_LIB_DIR}/libavfilter.so")
    set(AVFORMAT_LIB "${WEBOS_NATIVE_LIB_DIR}/libavformat.so")
    set(AVUTIL_LIB "${WEBOS_NATIVE_LIB_DIR}/libavutil.so")
    set(LUA_LIB "${WEBOS_NATIVE_LIB_DIR}/liblua5.2.so")
    set(SWRESAMPLE_LIB "${WEBOS_NATIVE_LIB_DIR}/libswresample.so")
    set(SWSCALE_LIB "${WEBOS_NATIVE_LIB_DIR}/libswscale.so")
    set(MPV_LIB "${MPV_WEBOS_BUILD_DIR}/libmpv.so")
    if(NOT EXISTS "${MPV_LIB}")
        message(FATAL_ERROR "Expected libmpv at ${MPV_LIB}")
    endif()
    foreach(required_lib
            AVCODEC_LIB
            AVFILTER_LIB
            AVFORMAT_LIB
            AVUTIL_LIB
            LUA_LIB
            SWRESAMPLE_LIB
            SWSCALE_LIB)
        if(NOT EXISTS "${${required_lib}}")
            message(FATAL_ERROR "Expected dependency at ${${required_lib}}")
        endif()
    endforeach()
else()
    pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
endif()

qt_add_executable(jellyfin-native
    src/api/JellyfinApiFacade.cpp
    src/api/JellyfinApiFacade.h
    src/api/HttpRequestPolicy.h
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/LibraryPrefetchController.cpp
    src/app/LibraryPrefetchController.h
    src/app/NavigationState.cpp
    src/app/NavigationState.h
    src/app/NativeAppWindow.h
    src/app/NativeAppWindowCommon.cpp
    src/app/QuickConnectController.cpp
    src/app/QuickConnectController.h
    src/app/SettingsController.cpp
    src/app/SettingsController.h
    src/app/SessionController.cpp
    src/app/SessionController.h
    src/app/QmlNetworkAccessManagerFactory.cpp
    src/app/QmlNetworkAccessManagerFactory.h
    src/app/SyncPlayController.cpp
    src/app/SyncPlayController.h
    src/app/LocalizationManager.cpp
    src/app/LocalizationManager.h
    src/cache/DatabaseManager.cpp
    src/cache/DatabaseManager.h
    src/common/JellyfinTypes.cpp
    src/common/JellyfinTypes.h
    src/common/AsyncTask.h
    src/common/RequestGeneration.h
    src/discovery/DiscoveryController.cpp
    src/discovery/DiscoveryController.h
    src/diagnostics/Diagnostics.cpp
    src/diagnostics/Diagnostics.h
    src/main.cpp
    src/models/DiscoveredServerModel.cpp
    src/models/DiscoveredServerModel.h
    src/models/LibraryListModel.cpp
    src/models/LibraryListModel.h
    src/models/MovieGridModel.cpp
    src/models/MovieGridModel.h
    src/player/MpvLifecycle.cpp
    src/player/MpvLifecycle.h
    src/player/PlayerController.cpp
    src/player/PlayerController.h
    src/player/PlaybackReporter.cpp
    src/player/PlaybackReporter.h
    src/player/PlaybackPositionTracker.cpp
    src/player/PlaybackPositionTracker.h
    src/player/PlaybackTimeline.cpp
    src/player/PlaybackTimeline.h
    src/player/PlaybackTrackParser.cpp
    src/player/PlaybackTrackParser.h
    src/player/MpvVideoItem.cpp
    src/player/MpvVideoItem.h
    src/player/MpvOptionProfile.cpp
    src/player/MpvOptionProfile.h
)

if(JELLYFIN_NATIVE_WEBOS)
    target_sources(jellyfin-native PRIVATE src/app/NativeAppWindow.cpp)
    target_compile_definitions(jellyfin-native PRIVATE JELLYFIN_NATIVE_WEBOS=1)
else()
    target_sources(jellyfin-native PRIVATE src/app/HostNativeAppWindow.cpp)
endif()
if(JELLYFIN_DIAGNOSTICS)
    target_compile_definitions(jellyfin-native PRIVATE JELLYFIN_DIAGNOSTICS=1)
endif()
if(JELLYFIN_DIAGNOSTICS_STACKDUMP)
    target_compile_definitions(jellyfin-native PRIVATE JELLYFIN_DIAGNOSTICS_STACKDUMP=1)
endif()
if(JELLYFIN_DIAGNOSTICS_ABORT_ON_HANG)
    target_compile_definitions(jellyfin-native PRIVATE JELLYFIN_DIAGNOSTICS_ABORT_ON_HANG=1)
endif()

set_source_files_properties(
    qml/primitives/InputKeys.qml
    qml/theme/Metrics.qml
    qml/theme/Theme.qml
    qml/theme/Typography.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_translations(jellyfin-native
    TS_FILES
        i18n/qtfin_en_US.ts
        i18n/qtfin_en_GB.ts
    RESOURCE_PREFIX /i18n
)

set(JELLYFIN_QML_MODULE_OPTIONS)
if(QT6_IS_SHARED_LIBS_BUILD)
    list(APPEND JELLYFIN_QML_MODULE_OPTIONS NO_IMPORT_SCAN)
endif()

qt_add_qml_module(jellyfin-native
    ${JELLYFIN_QML_MODULE_OPTIONS}
    URI JellyfinWebOS
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/HomePage.qml
        qml/pages/ItemDetailsPage.qml
        qml/pages/LibrariesPage.qml
        qml/pages/LibraryGridPage.qml
        qml/pages/LoginPage.qml
        qml/pages/PersonDetailsPage.qml
        qml/pages/PlayerAudioSyncPanel.qml
        qml/pages/PlayerOverlayMenu.qml
        qml/pages/PlayerOverlayPage.qml
        qml/pages/PlayerSkipSegmentCard.qml
        qml/pages/PlayerTrickplayPreview.qml
        qml/pages/SearchPage.qml
        qml/pages/SettingsPage.qml
        qml/primitives/ActionButton.qml
        qml/primitives/AudioDelayRow.qml
        qml/primitives/AppText.qml
        qml/primitives/ChipScrollerRow.qml
        qml/primitives/EmptyPlaceholder.qml
        qml/primitives/FastWheelHandler.qml
        qml/primitives/FocusFrame.qml
        qml/primitives/HomeHorizontalRow.qml
        qml/primitives/IconButton.qml
        qml/primitives/ImageCard.qml
        qml/primitives/InputKeys.qml
        qml/primitives/LandscapeCard.qml
        qml/primitives/MediaItemActions.qml
        qml/primitives/MediaItemCard.qml
        qml/primitives/MediaPosterScrollerRow.qml
        qml/primitives/MetadataChip.qml
        qml/primitives/MonoText.qml
        qml/primitives/ModelCountObserver.qml
        qml/primitives/MaterialIcon.qml
        qml/primitives/PersonScrollerRow.qml
        qml/primitives/PosterCard.qml
        qml/primitives/SectionHeader.qml
        qml/primitives/SelectRow.qml
        qml/primitives/SettingRow.qml
        qml/primitives/SliderRow.qml
        qml/primitives/Surface.qml
        qml/primitives/TechMetadataLine.qml
        qml/primitives/TextFieldRow.qml
        qml/primitives/ToggleRow.qml
        qml/shell/AppShell.qml
        qml/shell/DiagnosticsOverlay.qml
        qml/shell/MediaInfoOverlay.qml
        qml/shell/RoutePolicy.js
        qml/shell/RouteStack.qml
        qml/shell/ShortcutOverlay.qml
        qml/shell/SyncPlayMenu.qml
        qml/shell/ToastLayer.qml
        qml/shell/TopBar.qml
        qml/theme/Metrics.qml
        qml/theme/Theme.qml
        qml/theme/Typography.qml
    RESOURCES
        qml/fonts/MaterialIcons-Regular.ttf
)

set_target_properties(jellyfin-native PROPERTIES
    QT_QMLCACHEGEN_DIRECT_CALLS ON
)

# qmlimportscanner also scans the target's SOURCE_DIR recursively. This project
# keeps build trees and vendored sources below the repository root, so exclude
# them and let the generated QRC files provide the application's exact QML set.
set_property(TARGET jellyfin-native APPEND PROPERTY
    QT_QML_IMPORT_SCANNER_EXTRA_ARGS
        -exclude build
        -exclude build-arm
        -exclude build-linux-dev
        -exclude dist
        -exclude mpv
        -exclude mpv_webos
        -exclude squashfs-root
        -exclude third_party
)

target_include_directories(jellyfin-native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/player"
    "${MPV_ACTIVE_SOURCE_DIR}"
    "${MPV_ACTIVE_SOURCE_DIR}/include"
)

target_link_libraries(jellyfin-native PRIVATE
    QCoro::Core
    QCoro::Network
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::OpenGL
    Qt6::WebSockets
)

if(JELLYFIN_NATIVE_WEBOS)
    target_link_libraries(jellyfin-native PRIVATE
        PkgConfig::GLIB
        Qt6::GuiPrivate
        Qt6::WaylandClient
        ${EGL_LIB}
        ${GLES2_LIB}
        ${WAYLAND_EGL_LIB}
        ${WAYLAND_CLIENT_LIB}
        ${WAYLAND_WEBOS_CLIENT_LIB}
        ${WEBOS_HELPERS_LIB}
        ${LUNA_SERVICE_LIB}
        ${MPV_LIB}
        ${AVCODEC_LIB}
        ${AVFILTER_LIB}
        ${AVFORMAT_LIB}
        ${AVUTIL_LIB}
        ${LUA_LIB}
        ${SWRESAMPLE_LIB}
        ${SWSCALE_LIB}
    )

    target_link_options(jellyfin-native PRIVATE
        "-Wl,-rpath-link,${MPV_WEBOS_BUILD_DIR}"
        "-Wl,-rpath-link,${WEBOS_NATIVE_LIB_DIR}"
        "-Wl,-rpath-link,${WEBOS_SYSROOT_LIB_DIR}"
        "-Wl,-rpath-link,${WEBOS_SYSROOT_BASE_LIB_DIR}"
    )
else()
    target_link_libraries(jellyfin-native PRIVATE PkgConfig::MPV)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(playback-timeline-tests
        tests/player/PlaybackTimelineTest.cpp
        src/player/PlaybackTimeline.cpp
        src/player/PlaybackTimeline.h
    )
    target_include_directories(playback-timeline-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(playback-timeline-tests PRIVATE Qt6::Core)
    add_test(NAME playback-timeline COMMAND playback-timeline-tests)

    add_executable(playback-track-parser-tests
        tests/player/PlaybackTrackParserTest.cpp
        src/player/PlaybackTrackParser.cpp
        src/player/PlaybackTrackParser.h
    )
    target_include_directories(playback-track-parser-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(playback-track-parser-tests PRIVATE
        Qt6::Core
        PkgConfig::MPV
    )
    add_test(NAME playback-track-parser COMMAND playback-track-parser-tests)

    add_executable(playback-position-tracker-tests
        tests/player/PlaybackPositionTrackerTest.cpp
        src/player/PlaybackPositionTracker.cpp
        src/player/PlaybackPositionTracker.h
    )
    target_include_directories(playback-position-tracker-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(playback-position-tracker-tests PRIVATE Qt6::Core)
    add_test(NAME playback-position-tracker COMMAND playback-position-tracker-tests)

    add_executable(mpv-option-profile-tests
        tests/player/MpvOptionProfileTest.cpp
        src/player/MpvOptionProfile.cpp
        src/player/MpvOptionProfile.h
    )
    target_include_directories(mpv-option-profile-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(mpv-option-profile-tests PRIVATE Qt6::Core)
    add_test(NAME mpv-option-profile COMMAND mpv-option-profile-tests)

    add_executable(navigation-state-tests
        tests/app/NavigationStateTest.cpp
        src/app/NavigationState.cpp
        src/app/NavigationState.h
    )
    target_include_directories(navigation-state-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(navigation-state-tests PRIVATE Qt6::Core)
    add_test(NAME navigation-state COMMAND navigation-state-tests)

    add_executable(request-generation-tests
        tests/common/RequestGenerationTest.cpp
        src/common/RequestGeneration.h
    )
    target_include_directories(request-generation-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    add_test(NAME request-generation COMMAND request-generation-tests)

    add_executable(http-request-policy-tests
        tests/api/HttpRequestPolicyTest.cpp
        src/api/HttpRequestPolicy.h
    )
    target_include_directories(http-request-policy-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(http-request-policy-tests PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME http-request-policy COMMAND http-request-policy-tests)

    add_executable(database-manager-tests
        tests/cache/DatabaseManagerTest.cpp
        src/cache/DatabaseManager.cpp
        src/cache/DatabaseManager.h
        src/common/JellyfinTypes.cpp
        src/common/JellyfinTypes.h
        src/diagnostics/Diagnostics.cpp
        src/diagnostics/Diagnostics.h
    )
    target_include_directories(database-manager-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(database-manager-tests PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Sql
    )
    add_test(NAME database-manager COMMAND database-manager-tests)

    find_program(QMLTESTRUNNER_EXECUTABLE NAMES qmltestrunner)
    if(QMLTESTRUNNER_EXECUTABLE)
        add_test(NAME route-policy
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_RoutePolicy.qml"
        )
        set_tests_properties(route-policy PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
        )
    endif()
endif()

# Static Qt: import plugins that the linker can't auto-discover. BUILD_SHARED_LIBS
# controls this project's libraries, not whether the imported Qt build is static.
get_target_property(JELLYFIN_QT_CORE_TARGET_TYPE Qt6::Core TYPE)
if(JELLYFIN_QT_CORE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
    find_library(XKBCOMMON_LIB xkbcommon REQUIRED)
    target_link_libraries(jellyfin-native PRIVATE ${XKBCOMMON_LIB})

    if(JELLYFIN_NATIVE_WEBOS)
        qt_import_plugins(jellyfin-native
            NO_DEFAULT
            INCLUDE
                Qt6::QWaylandIntegrationPlugin
                Qt6::QWaylandEglClientBufferPlugin
                Qt6::QWaylandWlShellIntegrationPlugin
                Qt6::QSQLiteDriverPlugin
                Qt6::QVirtualKeyboardPlugin
                Qt6::QJpegPlugin
                Qt6::QWebpPlugin
        )

        # Qt VirtualKeyboard's QML qmldir files name nested static plugin link
        # targets. Import their generated package files before qmlimportscanner
        # asks CMake to link them, otherwise Qt warns that the targets are not
        # visible in this scope and silently omits the plugins.
        foreach(plugin IN ITEMS
            VirtualKeyboardplugin
            qtvkbplugin
            qtvkbsettingsplugin
            qtvkbcomponentsplugin
            qtvkblayoutsplugin
            qtvkbbuiltinstylesplugin
            qtvkbstylesplugin
            qtvkbhangulplugin
            qtvkbhunspellplugin
            qtvkbopenwnnplugin
            qtvkbpinyinplugin
            qtvkbtcimeplugin
            qtvkbthaiplugin
            qtvkbpluginsplugin
        )
            if(NOT TARGET Qt6::${plugin})
                find_package(Qt6${plugin} CONFIG QUIET
                    PATHS "${Qt6Qml_DIR}/QmlPlugins"
                    NO_DEFAULT_PATH
                )
            endif()
        endforeach()
    endif()

    # Let qmlimportscanner derive the static QML plugin link set from the
    # application's imports. New imports now fail when their plugin is absent.
    qt_import_qml_plugins(jellyfin-native)

    # Add linker GC to strip unused code from static libs.
    # macOS ld64 doesn't accept --gc-sections (uses -dead_strip instead, on by
    # default at -O), so only apply this on non-Apple platforms.
    if(NOT APPLE)
        target_link_options(jellyfin-native PRIVATE
            "-Wl,--gc-sections"
        )
        target_compile_options(jellyfin-native PRIVATE
            "-ffunction-sections"
            "-fdata-sections"
        )
    endif()
endif()

set_target_properties(jellyfin-native PROPERTIES
    OUTPUT_NAME "jellyfin-native"
    INSTALL_RPATH "$ORIGIN/../lib"
)
target_compile_definitions(jellyfin-native PRIVATE
    JELLYFIN_VERSION="${PROJECT_VERSION}"
)

if(APPLE)
    set_target_properties(jellyfin-native PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Jellyfin Native"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.codex.jellyfinwebosnative"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

install(TARGETS jellyfin-native
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
