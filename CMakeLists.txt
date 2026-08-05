cmake_minimum_required(VERSION 3.22)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" JELLYFIN_VERSION)
string(STRIP "${JELLYFIN_VERSION}" JELLYFIN_VERSION)
if(NOT JELLYFIN_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version such as 0.2.1")
endif()

project(JellyfinNativeWebOS VERSION "${JELLYFIN_VERSION}" LANGUAGES C CXX)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

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
option(JELLYFIN_IMAGE_SUBTITLE_DIAGNOSTICS
    "Log image subtitle crop, segmentation, geometry, and recoloring per subtitle event" OFF)
option(JELLYFIN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(JELLYFIN_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
include(CTest)

set(MPV_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv" CACHE PATH "mpv source tree")
set(MPV_ACTIVE_SOURCE_DIR "${MPV_SOURCE_DIR}")
set(MPV_WEBOS_BUILD_DIR "${MPV_SOURCE_DIR}/build/webos-libmpv" CACHE PATH "webOS libmpv build directory")

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

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()

set(JELLYFIN_QT_COMPONENTS
    Core
    Gui
    Network
    Qml
    Quick
    Sql
    OpenGL
    LinguistTools
    WebSockets
)
if(JELLYFIN_NATIVE_WEBOS)
    set(OPENSSL_USE_STATIC_LIBS FALSE)
    set(JELLYFIN_QT_MIN_VERSION 6.11)
    list(APPEND JELLYFIN_QT_COMPONENTS
        GuiPrivate
        QuickLayouts
        WaylandClient
    )
else()
    set(JELLYFIN_QT_MIN_VERSION 6.11)
    list(APPEND JELLYFIN_QT_COMPONENTS Svg)
    if(UNIX AND NOT APPLE)
        list(APPEND JELLYFIN_QT_COMPONENTS DBus)
    endif()
endif()
find_package(Qt6 ${JELLYFIN_QT_MIN_VERSION} REQUIRED COMPONENTS ${JELLYFIN_QT_COMPONENTS})
find_package(QCoro6 0.13 REQUIRED COMPONENTS Core Network)

qt_standard_project_setup(
    REQUIRES ${JELLYFIN_QT_MIN_VERSION}
    I18N_TRANSLATED_LANGUAGES en_US en_GB en_AU
)
include(cmake/platform/Common.cmake)

jellyfin_resolve_platform_dependencies()

set(JELLYFIN_CORE_SOURCES
    src/api/JellyfinApiFacade.cpp
    src/api/JellyfinApiFacade.h
    src/api/PlaybackBandwidthPolicy.cpp
    src/api/PlaybackBandwidthPolicy.h
    src/api/ItemsQuery.cpp
    src/api/ItemsQuery.h
    src/api/PlaybackNegotiation.cpp
    src/api/PlaybackNegotiation.h
    src/api/HttpRequestPolicy.h
    src/app/AccountProfile.cpp
    src/app/AccountProfile.h
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
    src/platform/NativeAppWindow.h
    src/platform/PlatformApplicationServices.h
    src/platform/PlatformCapabilities.h
    src/platform/MpvConfigPolicy.h
    src/platform/PlatformPaths.h
    src/platform/PlatformPlaybackRuntime.h
    src/platform/PlatformPerformanceSampler.h
    src/platform/PlatformPlaybackSurface.h
    src/platform/PlatformProcess.h
    src/platform/PlatformSettingsPolicy.h
    src/platform/PlatformStartup.h
    src/platform/PlatformSystemProbes.h
    src/platform/ScreenSaverInhibitor.h
    src/platform/common/ScreenSaverInhibitor.cpp
    src/platform/common/NativeAppWindowCommon.cpp
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
    src/common/SeriesAudioSelection.cpp
    src/common/SeriesAudioSelection.h
    src/common/MetaJson.h
    src/common/TlsTrust.cpp
    src/common/TlsTrust.h
    src/common/LogRotation.h
    src/common/AsyncTask.h
    src/common/RequestGeneration.h
    src/common/VariantUtils.cpp
    src/common/VariantUtils.h
    src/discovery/DiscoveryController.cpp
    src/discovery/DiscoveryController.h
    src/diagnostics/Diagnostics.cpp
    src/diagnostics/Diagnostics.h
    src/diagnostics/InputLatencyMonitor.cpp
    src/diagnostics/InputLatencyMonitor.h
    src/diagnostics/SystemPerformanceMonitor.cpp
    src/diagnostics/SystemPerformanceMonitor.h
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
    src/player/PlaybackFailurePolicy.cpp
    src/player/PlaybackFailurePolicy.h
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
jellyfin_configure_platform_targets(jellyfin-native jellyfin-core)
foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(JELLYFIN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(JELLYFIN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(JELLYFIN_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()

if(JELLYFIN_SANITIZERS AND MSVC)
    message(FATAL_ERROR "JELLYFIN_SANITIZERS requires GCC or Clang")
endif()


if(NOT JELLYFIN_NATIVE_DEV_BUILD)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT JELLYFIN_IPO_SUPPORTED OUTPUT JELLYFIN_IPO_ERROR)
    if(JELLYFIN_IPO_SUPPORTED)
        foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
            set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
            set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
        endforeach()
    else()
        message(STATUS "Interprocedural optimization unavailable: ${JELLYFIN_IPO_ERROR}")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
            target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
            if(UNIX AND NOT APPLE)
                if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM)")
                    target_compile_options(${target} PRIVATE -fPIC)
                    if(target STREQUAL "jellyfin-native")
                        target_link_options(${target} PRIVATE -fPIC)
                    endif()
                else()
                    target_compile_options(${target} PRIVATE -fPIE)
                endif()
                target_link_options(${target} PRIVATE -pie -Wl,--gc-sections -Wl,--as-needed -Wl,-z,relro,-z,now)
            elseif(APPLE)
                target_link_options(${target} PRIVATE -Wl,-dead_strip)
            endif()
        endforeach()
    endif()
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
if(JELLYFIN_IMAGE_SUBTITLE_DIAGNOSTICS)
    foreach(target IN LISTS JELLYFIN_NATIVE_TARGETS)
        target_compile_definitions(${target} PRIVATE JELLYFIN_IMAGE_SUBTITLE_DIAGNOSTICS=1)
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
else()
    list(APPEND JELLYFIN_QML_MODULE_OPTIONS DISCARD_QML_CONTENTS)
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
        qml/pages/LibraryNavigation.js
        qml/pages/LoginPage.qml
        qml/pages/ProfileNavigation.js
        qml/pages/OpenSourceNoticesPage.qml
        qml/pages/PersonDetailsPage.qml
        qml/pages/PlayerAudioSyncPanel.qml
        qml/pages/PlayerOverlayChrome.qml
        qml/pages/PlayerOverlayInput.qml
        qml/pages/PlayerOverlayPage.qml
        qml/pages/PlayerSeekBar.qml
        qml/pages/PlayerSkipSegmentCard.qml
        qml/pages/PlayerTransportBar.qml
        qml/pages/PlayerTrickplayPreview.qml
        qml/pages/SearchPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/SettingsNavigation.js
        qml/pages/SubtitleSettingsPanel.qml
        qml/primitives/ActionButton.qml
        qml/primitives/AppText.qml
        qml/primitives/AtomicViewReveal.qml
        qml/primitives/BusySpinner.qml
        qml/primitives/ChoiceStrip.qml
        qml/primitives/ConfirmationDialog.qml
        qml/primitives/EmptyPlaceholder.qml
        qml/primitives/FastWheelHandler.qml
        qml/primitives/GroupHeader.qml
        qml/primitives/HoldNavigationController.qml
        qml/primitives/InlineSlider.qml
        qml/primitives/IconButton.qml
        qml/primitives/ImageCard.qml
        qml/primitives/LazyMenuPanel.qml
        qml/primitives/OptionPickerDialog.qml
        qml/primitives/InputKeys.qml
        qml/primitives/MediaItemCard.qml
        qml/primitives/MediaRow.qml
        qml/primitives/MetadataChip.qml
        qml/primitives/MenuListView.qml
        qml/primitives/ListScrollBar.qml
        qml/primitives/MenuRow.qml
        qml/primitives/MonoText.qml
        qml/primitives/NavGrid.qml
        qml/primitives/MaterialIcon.qml
        qml/primitives/PopupMenuPanel.qml
        qml/primitives/SectionHeader.qml
        qml/primitives/SelectRow.qml
        qml/primitives/SettingRow.qml
        qml/primitives/SliderRow.qml
        qml/primitives/Surface.qml
        qml/primitives/TechMetadataLine.qml
        qml/primitives/TechnicalDetailsBar.qml
        qml/primitives/TextFieldRow.qml
        qml/primitives/ToggleRow.qml
        qml/shell/AppShell.qml
        qml/shell/KeyRouter.qml
        qml/shell/BusyOverlay.qml
        qml/shell/DiagnosticsOverlay.qml
        qml/shell/InputLatencyWarning.qml
        qml/shell/ItemContextMenu.qml
        qml/shell/ManagementDialog.qml
        qml/shell/RememberedCertificatesDialog.qml
        qml/shell/MediaInfoOverlay.qml
        qml/shell/OverlayDialog.qml
        qml/shell/RoutePolicy.js
        qml/shell/RouteStack.qml
        qml/shell/SyncPlayMenu.qml
        qml/shell/TlsTrustDialog.qml
        qml/shell/ToastLayer.qml
        qml/shell/TopBar.qml
        qml/shell/VideoSurface.qml
        qml/theme/Metrics.qml
        qml/theme/Theme.qml
        qml/theme/Typography.qml
    RESOURCES
        qml/fonts/AtkinsonHyperlegible-Bold.otf
        qml/fonts/AtkinsonHyperlegible-Regular.otf
        qml/fonts/IBMPlexSans-Variable.ttf
        qml/fonts/MaterialIcons-Regular.ttf
)
if(NOT JELLYFIN_NATIVE_WEBOS)
    qt_target_qml_sources(jellyfin-native
        QML_FILES
            qml/pages/DesktopFolderDialog.qml
    )
endif()

if(JELLYFIN_NATIVE_WEBOS)
    qt_add_resources(jellyfin-native "platform_branding"
        PREFIX "/branding"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/icons/png/spool"
        FILES app/icons/png/spool/64.png
    )
endif()

if(NOT JELLYFIN_NATIVE_WEBOS)
    qt_add_resources(jellyfin-native "application_icons"
        PREFIX "/icons"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/icons"
        FILES
            app/icons/spool.svg
            app/icons/spool-film.svg
    )
endif()

qt_add_resources(jellyfin-native "startup_splash_qml"
    PREFIX "/startup"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml/shell"
    FILES qml/shell/StartupSplash.qml
)
qt_add_resources(jellyfin-native "startup_splash_image"
    PREFIX "/startup"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/app"
    FILES app/splash.png
)


add_custom_target(jellyfin-native_qmllint_strict
    COMMAND Qt6::qmllint
        --unqualified disable
        --missing-property disable
        --property-override warning
        "@${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmllint/jellyfin-native.rsp"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Linting QML property overrides"
    VERBATIM
)
add_dependencies(jellyfin-native_qmllint_strict all_qmltyperegistrations)

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
        -exclude tests
        -exclude build-linux-dev
        -exclude dist
        -exclude mpv
        -exclude squashfs-root
        -exclude third_party
)
if(JELLYFIN_NATIVE_WEBOS)
    set_property(TARGET jellyfin-native APPEND PROPERTY
        QT_QML_IMPORT_SCANNER_EXTRA_ARGS
            -exclude qml/pages/DesktopFolderDialog.qml
    )
endif()


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
    Qt6::Sql
    Qt6::OpenGL
    Qt6::WebSockets
)

target_link_libraries(jellyfin-native PRIVATE jellyfin-core)
if(NOT JELLYFIN_NATIVE_WEBOS)
    target_link_libraries(jellyfin-native PRIVATE Qt6::Svg)
endif()


if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    set(JELLYFIN_TEST_SOURCES
        playback-timeline tests/player/PlaybackTimelineTest.cpp
        playback-track-parser tests/player/PlaybackTrackParserTest.cpp
        playback-track-state tests/player/PlaybackTrackStateTest.cpp
        playback-position-tracker tests/player/PlaybackPositionTrackerTest.cpp
        audio-sync-policy tests/player/AudioSyncPolicyTest.cpp
        mpv-option-profile tests/player/MpvOptionProfileTest.cpp
        playback-failure-policy tests/player/PlaybackFailurePolicyTest.cpp
        play-queue-controller tests/player/PlayQueueControllerTest.cpp
        series-audio-selection tests/player/SeriesAudioSelectionTest.cpp
        browse-session-controller tests/app/BrowseSessionControllerTest.cpp
        settings-schema tests/app/SettingsSchemaTest.cpp
        settings-controller tests/app/SettingsControllerTest.cpp
        session-controller tests/app/SessionControllerTest.cpp
        screensaver-inhibitor tests/platform/ScreenSaverInhibitorTest.cpp
        router-controller tests/app/RouterControllerTest.cpp
        native-window-input tests/platform/NativeAppWindowInputTest.cpp
        library-query tests/app/LibraryQueryTest.cpp
        content-model-controller tests/app/ContentModelControllerTest.cpp
        discovery-controller tests/discovery/DiscoveryControllerTest.cpp
        sync-play-clock tests/app/SyncPlayClockTest.cpp
        sync-play-queue-handoff tests/app/SyncPlayQueueHandoffTest.cpp
        request-generation tests/common/RequestGenerationTest.cpp
        diagnostic-redaction tests/common/DiagnosticRedactionTest.cpp
        tls-trust tests/common/TlsTrustTest.cpp
        browse-descriptor tests/common/BrowseDescriptorTest.cpp
        meta-json tests/common/MetaJsonTest.cpp
        http-request-policy tests/api/HttpRequestPolicyTest.cpp
        playback-bandwidth-policy tests/api/PlaybackBandwidthPolicyTest.cpp
        items-query tests/api/ItemsQueryTest.cpp
        playback-negotiation tests/api/PlaybackNegotiationTest.cpp
        jellyfin-api-facade-url tests/api/JellyfinApiFacadeUrlTest.cpp
        database-manager tests/cache/DatabaseManagerTest.cpp
        input-latency-monitor tests/diagnostics/InputLatencyMonitorTest.cpp
        system-performance-monitor tests/diagnostics/SystemPerformanceMonitorTest.cpp
        linux-memory-policy tests/platform/LinuxMemoryPolicyTest.cpp
    )
    if(UNIX AND NOT APPLE)
        list(APPEND JELLYFIN_TEST_SOURCES
            mpv-video-item tests/player/MpvVideoItemTest.cpp
        )
    endif()


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
        if(test_name STREQUAL "audio-sync-policy")
            target_sources(${test_target} PRIVATE src/platform/webos/WebOSAudioSyncPolicy.cpp)
        endif()
        if(test_name STREQUAL "tls-trust")
            target_compile_definitions(${test_target} PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        endif()
        target_link_libraries(${test_target} PRIVATE jellyfin-core)
        add_test(NAME ${test_name} COMMAND ${test_target})
        if(JELLYFIN_TEST_ENV)
            set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "${JELLYFIN_TEST_ENV}")
        endif()
    endforeach()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(JELLYFIN_NATIVE_WEBOS)
        set(JELLYFIN_FFMPEG_AUDIT_PLATFORM webos)
    elseif(APPLE)
        set(JELLYFIN_FFMPEG_AUDIT_PLATFORM macos)
    elseif(WIN32)
        set(JELLYFIN_FFMPEG_AUDIT_PLATFORM windows)
    else()
        set(JELLYFIN_FFMPEG_AUDIT_PLATFORM linux)
    endif()
    set(JELLYFIN_FFMPEG_CAPABILITY_HEADER
        "${CMAKE_CURRENT_BINARY_DIR}/generated/FfmpegCapabilities.h")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/ffmpeg-capabilities.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/manifests/ffmpeg-capabilities.json")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/ffmpeg-capabilities.py"
            cpp-header
            --platform "${JELLYFIN_FFMPEG_AUDIT_PLATFORM}"
            --output "${JELLYFIN_FFMPEG_CAPABILITY_HEADER}"
        RESULT_VARIABLE JELLYFIN_FFMPEG_HEADER_RESULT
        ERROR_VARIABLE JELLYFIN_FFMPEG_HEADER_ERROR
    )
    if(NOT JELLYFIN_FFMPEG_HEADER_RESULT EQUAL 0)
        message(FATAL_ERROR "FFmpeg capability header generation failed: ${JELLYFIN_FFMPEG_HEADER_ERROR}")
    endif()

    if(NOT WIN32)
        # Windows packages expose libmpv but do not ship FFmpeg development
        # libraries or pkg-config; keep its manifest generator test below,
        # but only build the linked compliance test where FFmpeg is available.
        pkg_check_modules(FFMPEG_AUDIT REQUIRED IMPORTED_TARGET libavutil libavcodec libavformat libavfilter)
        add_executable(ffmpeg-compliance-tests tests/media/FfmpegComplianceTest.cpp)
        target_include_directories(ffmpeg-compliance-tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(ffmpeg-compliance-tests PRIVATE
            TEST_MEDIA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/media/fixtures")
        target_link_libraries(ffmpeg-compliance-tests PRIVATE PkgConfig::FFMPEG_AUDIT)
        add_test(NAME ffmpeg-compliance COMMAND ffmpeg-compliance-tests)
    endif()
    add_test(NAME ffmpeg-capability-generator
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/FfmpegCapabilitiesTest.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/ffmpeg-capabilities.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/manifests/ffmpeg-capabilities.json"
    )

    add_test(NAME network-shaper
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/NetworkShaperTest.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/network-shaper.py"
    )

    find_program(QMLTESTRUNNER_EXECUTABLE NAMES qmltestrunner)
    if(QMLTESTRUNNER_EXECUTABLE)
        add_test(NAME route-policy
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_RoutePolicy.qml"
        )
        add_test(NAME key-router
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_KeyRouter.qml"
        )
        add_test(NAME player-overlay-input
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_PlayerOverlayInput.qml"
        )
        add_test(NAME option-picker-input
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_OptionPickerInput.qml"
        )
        add_test(NAME tls-trust-dialogs
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_TlsTrustDialogs.qml"
        )
        add_test(NAME nav-grid
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_NavGrid.qml"
        )
        add_test(NAME hold-navigation-controller
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_HoldNavigationController.qml"
        )
        add_test(NAME library-navigation
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_LibraryNavigation.qml"
        )
        add_test(NAME profile-navigation
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_ProfileNavigation.qml"
        )
        add_test(NAME settings-navigation
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_SettingsNavigation.qml"
        )
        if(WIN32)
            add_test(NAME subtitle-settings-panel
                COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                    -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_SubtitleSettingsPanel.qml"
            )
        endif()
        add_test(NAME atomic-view-reveal
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_AtomicViewReveal.qml"
        )
        add_test(NAME input-latency-warning
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_InputLatencyWarning.qml"
        )
        add_test(NAME player-seek-bar-hover
            COMMAND "${QMLTESTRUNNER_EXECUTABLE}"
                -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml/tst_PlayerSeekBarHover.qml"
        )
        get_filename_component(JELLYFIN_QML_PREFIX "${Qt6Qml_DIR}/../../.." ABSOLUTE)
        set(JELLYFIN_QML_IMPORT_DIR "${JELLYFIN_QML_PREFIX}/lib/qt-6/qml")
        set(JELLYFIN_QML_TEST_ENV
            "QT_QPA_PLATFORM=offscreen"
            "QT_QUICK_BACKEND=software"
            "QML2_IMPORT_PATH=${JELLYFIN_QML_IMPORT_DIR}"
            "QML_IMPORT_PATH=${JELLYFIN_QML_IMPORT_DIR}"
            ${JELLYFIN_TEST_ENV}
        )
        set_tests_properties(route-policy key-router player-overlay-input option-picker-input tls-trust-dialogs
            nav-grid hold-navigation-controller library-navigation profile-navigation settings-navigation
            atomic-view-reveal input-latency-warning player-seek-bar-hover
            PROPERTIES ENVIRONMENT "${JELLYFIN_QML_TEST_ENV}"
        )
    endif()
endif()

# Static Qt: import plugins that the linker can't auto-discover. BUILD_SHARED_LIBS
# controls this project's libraries, not whether the imported Qt build is static.
get_target_property(JELLYFIN_QT_CORE_TARGET_TYPE Qt6::Core TYPE)
if(JELLYFIN_QT_CORE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
    jellyfin_import_static_platform_plugins(jellyfin-native)

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


install(TARGETS jellyfin-native
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(UNIX AND NOT APPLE AND NOT JELLYFIN_NATIVE_WEBOS)
    install(FILES app/com.sachk.spool.desktop
        DESTINATION share/applications
    )
    install(FILES app/com.sachk.spool.metainfo.xml
        DESTINATION share/metainfo
        RENAME com.sachk.spool.appdata.xml
    )
    install(FILES app/icons/spool.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME com.sachk.spool.svg
    )
    foreach(icon_size 16 22 24 32 48 64 128 256 512)
        install(FILES "app/icons/png/spool/${icon_size}.png"
            DESTINATION "share/icons/hicolor/${icon_size}x${icon_size}/apps"
            RENAME com.sachk.spool.png
        )
    endforeach()
endif()
