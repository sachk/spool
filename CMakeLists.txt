cmake_minimum_required(VERSION 3.22)

project(JellyfinNativeWebOS LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcoro/cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcoro/cmake/QCoroConfig.cmake")

find_package(PkgConfig REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)

find_package(Qt6 6.11 REQUIRED COMPONENTS
    Core
    Gui
    GuiPrivate
    Network
    Qml
    Quick
    QuickControls2
    Sql
    OpenGL
    WaylandClient
    OpenApiCommon
)
find_package(Qt6OpenApiTools REQUIRED)

qt_standard_project_setup(REQUIRES 6.11)

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
set(MPV_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../mpv/build/webos-libmpv/libmpv.so")
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

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/openapi")
set(OPENAPI_SOURCE_SPEC "${CMAKE_CURRENT_SOURCE_DIR}/openapi.json")
set(OPENAPI_REDUCED_SPEC "${CMAKE_CURRENT_BINARY_DIR}/openapi/openapi-reduced.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OPENAPI_SOURCE_SPEC}")
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/reduce_openapi.py"
        "${OPENAPI_SOURCE_SPEC}"
        "${OPENAPI_REDUCED_SPEC}"
        "/System/Info/Public"
        "/QuickConnect/Enabled"
        "/QuickConnect/Initiate"
        "/QuickConnect/Connect"
    RESULT_VARIABLE OPENAPI_REDUCE_RESULT
    OUTPUT_VARIABLE OPENAPI_REDUCE_STDOUT
    ERROR_VARIABLE OPENAPI_REDUCE_STDERR
)
if(NOT OPENAPI_REDUCE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to reduce OpenAPI spec: ${OPENAPI_REDUCE_STDERR}")
endif()

qt_add_library(jellyfin_openapi_client STATIC)
qt_add_openapi_client(jellyfin_openapi_client
    SPEC_FILE "${OPENAPI_REDUCED_SPEC}"
    GENERATE_OPTIONS
        -g
        cpp-qt6-client
    ADDITIONAL_PROPERTIES
        cppCommonNamespace=QtOpenApiCommon
        prefix=QOAI
        cppNamespace=JellyfinOpenApi
)

qt_add_executable(jellyfin-native
    src/api/JellyfinApiFacade.cpp
    src/api/JellyfinApiFacade.h
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/NativeAppWindow.cpp
    src/app/NativeAppWindow.h
    src/app/QmlNetworkAccessManagerFactory.cpp
    src/app/QmlNetworkAccessManagerFactory.h
    src/cache/DatabaseManager.cpp
    src/cache/DatabaseManager.h
    src/common/JellyfinTypes.cpp
    src/common/JellyfinTypes.h
    src/discovery/DiscoveryController.cpp
    src/discovery/DiscoveryController.h
    src/main.cpp
    src/models/DiscoveredServerModel.cpp
    src/models/DiscoveredServerModel.h
    src/models/LibraryListModel.cpp
    src/models/LibraryListModel.h
    src/models/MovieGridModel.cpp
    src/models/MovieGridModel.h
    src/player/PlayerController.cpp
    src/player/PlayerController.h
)

qt_add_qml_module(jellyfin-native
    URI JellyfinWebOS
    VERSION 1.0
    QML_FILES
        qml/LibrariesPage.qml
        qml/LibraryPage.qml
        qml/LoginPage.qml
        qml/Main.qml
        qml/PlayerOverlay.qml
        qml/PosterCard.qml
        qml/SettingsPanel.qml
)

target_include_directories(jellyfin-native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcoro/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../mpv"
    "${CMAKE_CURRENT_SOURCE_DIR}/../mpv/include"
)

target_link_libraries(jellyfin-native PRIVATE
    jellyfin_openapi_client
    QCoro::Core
    QCoro::Network
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::OpenGL
    Qt6::WaylandClient
    Qt6::OpenApiCommon
    PkgConfig::GLIB
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
    "-Wl,-rpath-link,${CMAKE_CURRENT_SOURCE_DIR}/../mpv/build/webos-libmpv"
    "-Wl,-rpath-link,${WEBOS_NATIVE_LIB_DIR}"
    "-Wl,-rpath-link,${WEBOS_SYSROOT_LIB_DIR}"
    "-Wl,-rpath-link,${WEBOS_SYSROOT_BASE_LIB_DIR}"
)

# Static Qt: import plugins that the linker can't auto-discover
if(NOT BUILD_SHARED_LIBS)
    find_library(XKBCOMMON_LIB xkbcommon REQUIRED)
    target_link_libraries(jellyfin-native PRIVATE ${XKBCOMMON_LIB})

    qt_import_plugins(jellyfin-native
        INCLUDE
            Qt6::QWaylandIntegrationPlugin
            Qt6::QWaylandEglClientBufferPlugin
            Qt6::QWaylandWlShellIntegrationPlugin
            Qt6::QSQLiteDriverPlugin
        EXCLUDE_BY_TYPE imageformats
    )
    # Re-include specific image plugins after the blanket exclude
    qt_import_plugins(jellyfin-native
        INCLUDE_BY_TYPE imageformats 
            Qt6::QJpegPlugin 
            Qt6::QWebpPlugin
    )
    # Add linker GC to strip unused code from static libs
    target_link_options(jellyfin-native PRIVATE
        "-Wl,--gc-sections"
    )
    target_compile_options(jellyfin-native PRIVATE
        "-ffunction-sections"
        "-fdata-sections"
    )
endif()

set_target_properties(jellyfin-native PROPERTIES
    OUTPUT_NAME "jellyfin-native"
    INSTALL_RPATH "$ORIGIN/../lib"
)

install(TARGETS jellyfin-native
    RUNTIME DESTINATION bin
)
