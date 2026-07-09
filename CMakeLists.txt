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
option(JELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS "Log artwork decode requests whose target aspect would stretch the source" OFF)
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
    set(WEBOS_NATIVE_LIB_DIR "${CMAKE_SYSROOT}/usr/local/webos-native/lib")
    set(WEBOS_SYSROOT_LIB_DIR "${CMAKE_SYSROOT}/usr/lib")
    set(WEBOS_SYSROOT_BASE_LIB_DIR "${CMAKE_SYSROOT}/lib")

    function(find_webos_sysroot_library out_var library_name)
        foreach(candidate
                "${WEBOS_SYSROOT_LIB_DIR}/lib${library_name}.so"
                "${WEBOS_SYSROOT_BASE_LIB_DIR}/lib${library_name}.so")
            if(EXISTS "${candidate}" OR IS_SYMLINK "${candidate}")
                set(${out_var} "${candidate}" PARENT_SCOPE)
                return()
            endif()
        endforeach()

        find_library(${out_var}
            NAMES ${library_name}
            PATHS
                "${WEBOS_SYSROOT_LIB_DIR}"
                "${WEBOS_SYSROOT_BASE_LIB_DIR}"
            NO_DEFAULT_PATH
            NO_CMAKE_FIND_ROOT_PATH
        )
        if(NOT ${out_var})
            file(GLOB matches
                "${WEBOS_SYSROOT_LIB_DIR}/lib${library_name}.so.*"
                "${WEBOS_SYSROOT_BASE_LIB_DIR}/lib${library_name}.so.*"
            )
            if(matches)
                list(SORT matches)
                list(GET matches 0 resolved_library)
                set(${out_var} "${resolved_library}" PARENT_SCOPE)
                return()
            endif()
        endif()
        if(NOT ${out_var})
            message(FATAL_ERROR "Could not find webOS sysroot library: ${library_name}")
        endif()
        set(${out_var} "${${out_var}}" PARENT_SCOPE)
    endfunction()

    find_webos_sysroot_library(EGL_LIB EGL)
    find_webos_sysroot_library(GLES2_LIB GLESv2)
    find_webos_sysroot_library(WAYLAND_EGL_LIB wayland-egl)
    find_webos_sysroot_library(WAYLAND_CLIENT_LIB wayland-client)
    find_webos_sysroot_library(WAYLAND_WEBOS_CLIENT_LIB wayland-webos-client)
    find_webos_sysroot_library(WEBOS_HELPERS_LIB helpers)
    find_webos_sysroot_library(LUNA_SERVICE_LIB luna-service2)
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
    if(UNIX AND NOT APPLE)
        find_package(OpenGL REQUIRED)
    endif()
endif()

set(JELLYFIN_CORE_SOURCES
    src/api/JellyfinApiFacade.cpp
    src/api/JellyfinApiFacade.h
    src/api/ItemsQuery.cpp
    src/api/ItemsQuery.h
    src/api/PlaybackNegotiation.cpp
    src/api/PlaybackNegotiation.h
    src/api/HttpRequestPolicy.h
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/ArtworkImageProvider.cpp
    src/app/ArtworkImageProvider.h
    src/app/ArtworkService.cpp
    src/app/ArtworkService.h
    src/app/ArtworkPrefetcher.h
    src/app/ContentModelController.cpp
    src/app/ContentModelController.h
    src/app/CpuTopology.cpp
    src/app/CpuTopology.h
    src/app/BrowseSessionController.cpp
    src/app/BrowseSessionController.h
    src/app/HomeModelController.cpp
    src/app/HomeModelController.h
    src/app/LibraryManagementController.cpp
    src/app/LibraryManagementController.h
    src/app/LibraryQuery.cpp
    src/app/LibraryQuery.h
    src/app/LibraryPrefetchController.cpp
    src/app/LibraryPrefetchController.h
    src/app/MemoryBudget.cpp
    src/app/MemoryBudget.h
    src/app/RouterController.cpp
    src/app/RouterController.h
    src/app/NativeAppWindowCommon.cpp
    src/app/QuickConnectController.cpp
    src/app/QuickConnectController.h
    src/app/SettingsController.cpp
    src/app/SettingsController.h
    src/app/SettingsSchema.cpp
    src/app/SettingsSchema.h
    src/app/SearchController.cpp
    src/app/SearchController.h
    src/app/SessionController.cpp
    src/app/SessionController.h
    src/app/SyncPlayController.cpp
    src/app/SyncPlayController.h
    src/app/SyncPlayClock.cpp
    src/app/SyncPlayClock.h
    src/app/UserItemStateController.cpp
    src/app/UserItemStateController.h
    src/app/LocalizationManager.cpp
    src/app/LocalizationManager.h
    src/cache/DatabaseManager.cpp
    src/cache/DatabaseManager.h
    src/common/JellyfinTypes.cpp
    src/common/JellyfinTypes.h
    src/common/MetaJson.h
    src/common/LogRotation.h
    src/common/AsyncTask.h
    src/common/RequestGeneration.h
    src/common/VariantUtils.cpp
    src/common/VariantUtils.h
    src/discovery/DiscoveryController.cpp
    src/discovery/DiscoveryController.h
    src/diagnostics/Diagnostics.cpp
    src/diagnostics/Diagnostics.h
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
    src/player/PlayQueueController.cpp
    src/player/PlayQueueController.h
    src/player/PlaybackReporter.cpp
    src/player/PlaybackReporter.h
    src/player/PlaybackPositionTracker.cpp
    src/player/PlaybackPositionTracker.h
    src/player/PlaybackTimeline.cpp
    src/player/PlaybackTimeline.h
    src/player/PlaybackTrackParser.cpp
    src/player/PlaybackTrackParser.h
    src/player/PlaybackTrackState.cpp
    src/player/PlaybackTrackState.h
    src/player/MpvVideoItem.cpp
    src/player/MpvVideoItem.h
    src/player/MpvOptionProfile.cpp
    src/player/MpvOptionProfile.h
)

add_library(jellyfin-core STATIC ${JELLYFIN_CORE_SOURCES})

qt_add_executable(jellyfin-native
    src/main.cpp
)

set(JELLYFIN_NATIVE_TARGETS jellyfin-core jellyfin-native)

if(JELLYFIN_NATIVE_WEBOS)
    target_sources(jellyfin-native PRIVATE
        src/app/NativeAppWindow.cpp
        src/app/NativeAppWindow.h
        src/player/MpvRuntime.cpp
        src/player/MpvRuntime.h
    )
    foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
        target_compile_definitions(${target} PRIVATE JELLYFIN_NATIVE_WEBOS=1)
    endforeach()

    # Static input context plugin that drives the stock webOS on-screen
    # keyboard (MaliitServer) via the compositor's text_model protocol.
    # Selected at runtime through QT_IM_MODULE=webosim in main.cpp.
    #
    # The protocol stubs are generated from src/platform/webos/protocol/
    # text.xml, whose request order matches the TV firmware tables — NOT the
    # SDK sysroot's libwayland-webos-client, whose text_model opcodes differ
    # (see the comment in that text.xml). Do not link the sysroot lib here.
    enable_language(C)
    qt_add_plugin(webos-im-plugin STATIC
        CLASS_NAME WebOSPlatformInputContextPlugin
        PLUGIN_TYPE platforminputcontexts
    )
    target_sources(webos-im-plugin PRIVATE
        src/platform/webos/WebOSInputContext.cpp
        src/platform/webos/WebOSInputContext.h
        src/platform/webos/WebOSInputContextPlugin.cpp
        src/platform/webos/WebOSKeysymMap.h
        src/platform/webos/protocol/wayland-text-client-protocol.c
        src/platform/webos/protocol/wayland-text-client-protocol.h
    )
    target_link_libraries(webos-im-plugin PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        ${WAYLAND_CLIENT_LIB}
    )
    target_link_libraries(jellyfin-native PRIVATE webos-im-plugin)
else()
    target_sources(jellyfin-core PRIVATE
        src/app/HostNativeAppWindow.cpp
        src/app/NativeAppWindow.h
    )
endif()
if(JELLYFIN_DIAGNOSTICS)
    foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
        target_compile_definitions(${target} PRIVATE JELLYFIN_DIAGNOSTICS=1)
    endforeach()
endif()
if(JELLYFIN_DIAGNOSTICS_STACKDUMP)
    foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
        target_compile_definitions(${target} PRIVATE JELLYFIN_DIAGNOSTICS_STACKDUMP=1)
    endforeach()
endif()
if(JELLYFIN_DIAGNOSTICS_ABORT_ON_HANG)
    foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
        target_compile_definitions(${target} PRIVATE JELLYFIN_DIAGNOSTICS_ABORT_ON_HANG=1)
    endforeach()
endif()
if(JELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS)
    foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
        target_compile_definitions(${target} PRIVATE JELLYFIN_ARTWORK_ASPECT_DIAGNOSTICS=1)
    endforeach()
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
        qml/pages/LibraryGridPage.qml
        qml/pages/LoginPage.qml
        qml/pages/PersonDetailsPage.qml
        qml/pages/PlayerAudioSyncPanel.qml
        qml/pages/PlayerOverlayChrome.qml
        qml/pages/PlayerOverlayInput.qml
        qml/pages/PlayerOverlayMenu.qml
        qml/pages/PlayerOverlayPage.qml
        qml/pages/PlayerSeekBar.qml
        qml/pages/PlayerSkipSegmentCard.qml
        qml/pages/PlayerTransportBar.qml
        qml/pages/PlayerTrickplayPreview.qml
        qml/pages/SearchPage.qml
        qml/pages/SettingsPage.qml
        qml/primitives/ActionButton.qml
        qml/primitives/AppText.qml
        qml/primitives/EmptyPlaceholder.qml
        qml/primitives/FastWheelHandler.qml
        qml/primitives/HomeHorizontalRow.qml
        qml/primitives/IconButton.qml
        qml/primitives/ImageCard.qml
        qml/primitives/InputKeys.qml
        qml/primitives/LandscapeCard.qml
        qml/primitives/MediaItemActions.qml
        qml/primitives/MediaItemCard.qml
        qml/primitives/MediaPosterScrollerRow.qml
        qml/primitives/MetadataChip.qml
        qml/primitives/MenuListView.qml
        qml/primitives/MenuRow.qml
        qml/primitives/MonoText.qml
        qml/primitives/NavGrid.qml
        qml/primitives/NavList.qml
        qml/primitives/MaterialIcon.qml
        qml/primitives/PersonScrollerRow.qml
        qml/primitives/PopupMenuPanel.qml
        qml/primitives/SectionHeader.qml
        qml/primitives/SelectRow.qml
        qml/primitives/SettingRow.qml
        qml/primitives/SliderRow.qml
        qml/primitives/Surface.qml
        qml/primitives/TechMetadataLine.qml
        qml/primitives/TextFieldRow.qml
        qml/primitives/ToggleRow.qml
        qml/shell/AppShell.qml
        qml/shell/BusyOverlay.qml
        qml/shell/DiagnosticsOverlay.qml
        qml/shell/ItemContextMenu.qml
        qml/shell/ManagementDialog.qml
        qml/shell/MediaInfoOverlay.qml
        qml/shell/OverlayDialog.qml
        qml/shell/RoutePolicy.js
        qml/shell/RouteStack.qml
        qml/shell/SyncPlayMenu.qml
        qml/shell/ToastLayer.qml
        qml/shell/TopBar.qml
        qml/shell/VideoSurface.qml
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

get_target_property(JELLYFIN_NATIVE_INCLUDE_DIRS jellyfin-native INCLUDE_DIRECTORIES)
if(JELLYFIN_NATIVE_INCLUDE_DIRS)
    list(REMOVE_ITEM JELLYFIN_NATIVE_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_property(TARGET jellyfin-native PROPERTY
        INCLUDE_DIRECTORIES "${JELLYFIN_NATIVE_INCLUDE_DIRS}"
    )
endif()

target_include_directories(jellyfin-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/player"
    "${MPV_ACTIVE_SOURCE_DIR}"
    "${MPV_ACTIVE_SOURCE_DIR}/include"
)

target_link_libraries(jellyfin-core PUBLIC
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

target_link_libraries(jellyfin-native PRIVATE jellyfin-core)

if(JELLYFIN_NATIVE_WEBOS)
    target_link_libraries(jellyfin-native PRIVATE
        PkgConfig::GLIB
        Qt6::GuiPrivate
        Qt6::WaylandClient
        Qt6::Svg
        ${EGL_LIB}
        ${GLES2_LIB}
        ${WAYLAND_EGL_LIB}
        ${WAYLAND_CLIENT_LIB}
        ${WAYLAND_WEBOS_CLIENT_LIB}
        ${WEBOS_HELPERS_LIB}
        ${LUNA_SERVICE_LIB}
        # libmpv (and its ffmpeg/lua dependencies) is deliberately NOT linked:
        # src/player/MpvRuntime.cpp dlopen()s it after first frame so the
        # dynamic linker does not map + relocate the ~40 MB player stack
        # before main(). The staged libs above are still runtime requirements
        # (existence-checked earlier); they just load lazily now.
        ${CMAKE_DL_LIBS}
    )

    target_link_options(jellyfin-native PRIVATE
        "-Wl,-rpath-link,${WEBOS_SYSROOT_LIB_DIR}"
        "-Wl,-rpath-link,${WEBOS_SYSROOT_BASE_LIB_DIR}"
    )
else()
    target_link_libraries(jellyfin-core PUBLIC PkgConfig::MPV)
    if(UNIX AND NOT APPLE)
        target_link_libraries(jellyfin-core PUBLIC OpenGL::GL)
        target_include_directories(jellyfin-core PUBLIC "${OPENGL_INCLUDE_DIR}")
        target_compile_options(jellyfin-core PUBLIC "-I${OPENGL_INCLUDE_DIR}")
    endif()
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    set(JELLYFIN_TEST_SOURCES
        playback-timeline tests/player/PlaybackTimelineTest.cpp
        playback-track-parser tests/player/PlaybackTrackParserTest.cpp
        playback-track-state tests/player/PlaybackTrackStateTest.cpp
        playback-position-tracker tests/player/PlaybackPositionTrackerTest.cpp
        mpv-option-profile tests/player/MpvOptionProfileTest.cpp
        play-queue-controller tests/player/PlayQueueControllerTest.cpp
        browse-session-controller tests/app/BrowseSessionControllerTest.cpp
        settings-schema tests/app/SettingsSchemaTest.cpp
        router-controller tests/app/RouterControllerTest.cpp
        library-query tests/app/LibraryQueryTest.cpp
        content-model-controller tests/app/ContentModelControllerTest.cpp
        sync-play-clock tests/app/SyncPlayClockTest.cpp
        request-generation tests/common/RequestGenerationTest.cpp
        browse-descriptor tests/common/BrowseDescriptorTest.cpp
        meta-json tests/common/MetaJsonTest.cpp
        http-request-policy tests/api/HttpRequestPolicyTest.cpp
        items-query tests/api/ItemsQueryTest.cpp
        playback-negotiation tests/api/PlaybackNegotiationTest.cpp
        jellyfin-api-facade-url tests/api/JellyfinApiFacadeUrlTest.cpp
        database-manager tests/cache/DatabaseManagerTest.cpp
    )


    get_target_property(JELLYFIN_QT_CORE_LOCATION Qt6::Core IMPORTED_LOCATION_RELEASE)
    if(NOT JELLYFIN_QT_CORE_LOCATION)
        get_target_property(JELLYFIN_QT_CORE_LOCATION Qt6::Core IMPORTED_LOCATION)
    endif()
    set(JELLYFIN_TEST_ENV)
    if(JELLYFIN_QT_CORE_LOCATION)
        get_filename_component(JELLYFIN_QT_LIB_DIR "${JELLYFIN_QT_CORE_LOCATION}" DIRECTORY)
        get_filename_component(JELLYFIN_QT_PREFIX "${JELLYFIN_QT_LIB_DIR}" DIRECTORY)
        set(JELLYFIN_QT_PLUGIN_DIR "${JELLYFIN_QT_PREFIX}/lib/qt-6/plugins")
        if(EXISTS "${JELLYFIN_QT_PLUGIN_DIR}")
            list(APPEND JELLYFIN_TEST_ENV "QT_PLUGIN_PATH=${JELLYFIN_QT_PLUGIN_DIR}")
        endif()
    endif()
    list(LENGTH JELLYFIN_TEST_SOURCES JELLYFIN_TEST_SOURCE_COUNT)
    math(EXPR JELLYFIN_TEST_SOURCE_LAST "${JELLYFIN_TEST_SOURCE_COUNT} - 1")
    foreach(test_index RANGE 0 ${JELLYFIN_TEST_SOURCE_LAST} 2)
        math(EXPR source_index "${test_index} + 1")
        list(GET JELLYFIN_TEST_SOURCES ${test_index} test_name)
        list(GET JELLYFIN_TEST_SOURCES ${source_index} test_source)
        set(test_target "${test_name}-tests")
        add_executable(${test_target} "${test_source}")
        target_link_libraries(${test_target} PRIVATE jellyfin-core)
        add_test(NAME ${test_name} COMMAND ${test_target})
        if(JELLYFIN_TEST_ENV)
            set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "${JELLYFIN_TEST_ENV}")
        endif()
    endforeach()

    find_program(QMLTESTRUNNER_EXECUTABLE NAMES qmltestrunner)
    if(QMLTESTRUNNER_EXECUTABLE)
        add_test(NAME route-policy
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_RoutePolicy.qml"
        )
        set(JELLYFIN_ROUTE_POLICY_ENV
            "QT_QPA_PLATFORM=offscreen"
            "QT_QUICK_BACKEND=software"
            ${JELLYFIN_TEST_ENV}
        )
        set_tests_properties(route-policy PROPERTIES
            ENVIRONMENT "${JELLYFIN_ROUTE_POLICY_ENV}"
        )
    endif()
endif()

# Static Qt: import plugins that the linker can't auto-discover. BUILD_SHARED_LIBS
# controls this project's libraries, not whether the imported Qt build is static.
get_target_property(JELLYFIN_QT_CORE_TARGET_TYPE Qt6::Core TYPE)
if(JELLYFIN_QT_CORE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
    if(JELLYFIN_NATIVE_WEBOS)
        find_webos_sysroot_library(XKBCOMMON_LIB xkbcommon)
    else()
        find_library(XKBCOMMON_LIB xkbcommon REQUIRED)
    endif()
    target_link_libraries(jellyfin-native PRIVATE ${XKBCOMMON_LIB})

    if(JELLYFIN_NATIVE_WEBOS)
        qt_import_plugins(jellyfin-native
            NO_DEFAULT
            INCLUDE
                Qt6::QWaylandIntegrationPlugin
                Qt6::QWaylandEglClientBufferPlugin
                Qt6::QWaylandWlShellIntegrationPlugin
                Qt6::QSQLiteDriverPlugin
                Qt6::QJpegPlugin
                Qt6::QWebpPlugin
                Qt6::QSvgPlugin
        )

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
