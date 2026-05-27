cmake_minimum_required(VERSION 3.22)

project(JellyfinNativeWebOS LANGUAGES CXX)

set(JELLYFIN_NATIVE_WEBOS_DEFAULT OFF)
if(CMAKE_CROSSCOMPILING)
    set(JELLYFIN_NATIVE_WEBOS_DEFAULT ON)
endif()
option(JELLYFIN_NATIVE_WEBOS "Build the webOS Starfish/native-surface client" ${JELLYFIN_NATIVE_WEBOS_DEFAULT})
option(JELLYFIN_NATIVE_DEV_BUILD "Favor faster local development builds over smallest release artifacts" OFF)
option(JELLYFIN_DIAGNOSTICS "Build debug lifecycle/process/watchdog diagnostics" OFF)
option(JELLYFIN_DIAGNOSTICS_STACKDUMP "Allow diagnostics to request gdb stack dumps" OFF)
option(JELLYFIN_DIAGNOSTICS_ABORT_ON_HANG "Abort after watchdog hang diagnostics are written" OFF)

set(MPV_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv" CACHE PATH "mpv source tree")
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcoro/cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcoro/cmake/QCoroConfig.cmake")

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
        VirtualKeyboard
        VirtualKeyboardQml
        VirtualKeyboardSettings
        WaylandClient
    )
else()
    set(JELLYFIN_QT_MIN_VERSION 6.11)
endif()
find_package(Qt6 ${JELLYFIN_QT_MIN_VERSION} REQUIRED COMPONENTS ${JELLYFIN_QT_COMPONENTS})

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
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/NativeAppWindow.h
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
    src/player/PlayerController.cpp
    src/player/PlayerController.h
    src/player/MpvVideoItem.cpp
    src/player/MpvVideoItem.h
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

qt_add_qml_module(jellyfin-native
    URI JellyfinWebOS
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/HomePage.qml
        qml/pages/ItemDetailsPage.qml
        qml/pages/LibrariesPage.qml
        qml/pages/LibraryGridPage.qml
        qml/pages/LoginPage.qml
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
        qml/primitives/EmptyPlaceholder.qml
        qml/primitives/FocusFrame.qml
        qml/primitives/IconButton.qml
        qml/primitives/ImageCard.qml
        qml/primitives/LandscapeCard.qml
        qml/primitives/MetadataChip.qml
        qml/primitives/MonoText.qml
        qml/primitives/MaterialIcon.qml
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
        qml/shell/RouteStack.qml
        qml/shell/ShortcutOverlay.qml
        qml/shell/SideRail.qml
        qml/shell/ToastLayer.qml
        qml/theme/Metrics.qml
        qml/theme/Theme.qml
        qml/theme/Typography.qml
    RESOURCES
        qml/fonts/MaterialIcons-Regular.ttf
)

if(NOT BUILD_SHARED_LIBS)
    set_target_properties(jellyfin-native PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN ON)
endif()

target_include_directories(jellyfin-native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcoro/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/player"
    "${MPV_SOURCE_DIR}"
    "${MPV_SOURCE_DIR}/include"
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

# Static Qt: import plugins that the linker can't auto-discover
if(NOT BUILD_SHARED_LIBS)
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
    endif()

    set(JELLYFIN_STATIC_QML_PLUGINS
        Qt6::qmlplugin
        Qt6::modelsplugin
        Qt6::workerscriptplugin
        Qt6::qtquick2plugin
        Qt6::quickwindow
        Qt6::qquicklayoutsplugin
        Qt6::qtquicktemplates2plugin
        Qt6::qtquickcontrols2implplugin
        Qt6::qtquickcontrols2basicstyleimplplugin
        Qt6::qtquickcontrols2basicstyleplugin
        Qt6::qmlfolderlistmodelplugin
        Qt6::qmlwebsocketsplugin
        Qt6::VirtualKeyboardplugin
        Qt6::qtvkbplugin
        Qt6::qtvkblayoutsplugin
        Qt6::qtvkbsettingsplugin
        Qt6::qtvkbcomponentsplugin
        Qt6::qtvkbstylesplugin
        Qt6::qtvkbbuiltinstylesplugin
        Qt6::qtvkbpluginsplugin
        Qt6::qtvkbhangulplugin
        Qt6::qtvkbopenwnnplugin
        Qt6::qtvkbpinyinplugin
        Qt6::qtvkbtcimeplugin
        Qt6::qtvkbthaiplugin
    )
    foreach(qml_plugin IN LISTS JELLYFIN_STATIC_QML_PLUGINS)
        if(TARGET ${qml_plugin})
            target_link_libraries(jellyfin-native PRIVATE ${qml_plugin})
        endif()
    endforeach()

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

if(APPLE)
    set_target_properties(jellyfin-native PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Jellyfin Native"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.sachk.tern"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.2.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.2.1"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

install(TARGETS jellyfin-native
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
