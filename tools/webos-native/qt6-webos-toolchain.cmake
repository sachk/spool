set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(DEFINED ENV{WEBOS_SDK_ROOT} AND NOT "$ENV{WEBOS_SDK_ROOT}" STREQUAL "")
    set(WEBOS_SDK_ROOT "$ENV{WEBOS_SDK_ROOT}")
else()
    get_filename_component(WEBOS_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
    get_filename_component(WEBOS_REPO_ROOT "${WEBOS_TOOLCHAIN_DIR}/../.." ABSOLUTE)
    get_filename_component(WEBOS_WORKSPACE_ROOT "${WEBOS_REPO_ROOT}/.." ABSOLUTE)
    set(_webos_repo_sdk_root "${WEBOS_REPO_ROOT}/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot")
    set(_webos_workspace_sdk_root "${WEBOS_WORKSPACE_ROOT}/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot")
    if(EXISTS "${_webos_workspace_sdk_root}")
        set(WEBOS_SDK_ROOT "${_webos_workspace_sdk_root}")
    else()
        set(WEBOS_SDK_ROOT "${_webos_repo_sdk_root}")
    endif()
endif()

set(WEBOS_TOOLCHAIN_BINDIR "${WEBOS_SDK_ROOT}/bin")
set(CMAKE_SYSROOT "${WEBOS_SDK_ROOT}/arm-webos-linux-gnueabi/sysroot")

set(CMAKE_C_COMPILER "${WEBOS_TOOLCHAIN_BINDIR}/arm-webos-linux-gnueabi-gcc")
set(CMAKE_CXX_COMPILER "${WEBOS_TOOLCHAIN_BINDIR}/arm-webos-linux-gnueabi-g++")
set(CMAKE_ASM_COMPILER "${WEBOS_TOOLCHAIN_BINDIR}/arm-webos-linux-gnueabi-gcc")
set(CMAKE_AR "${WEBOS_TOOLCHAIN_BINDIR}/arm-webos-linux-gnueabi-ar")
set(CMAKE_RANLIB "${WEBOS_TOOLCHAIN_BINDIR}/arm-webos-linux-gnueabi-ranlib")
set(CMAKE_STRIP "${WEBOS_TOOLCHAIN_BINDIR}/arm-webos-linux-gnueabi-strip")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")

get_filename_component(_toolchain_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
set(PKG_CONFIG_EXECUTABLE "${_toolchain_dir}/pkg-config-webos.sh" CACHE FILEPATH "" FORCE)

# Qt's static CMake packages re-run third-party dependency discovery in
# downstream projects. Keep those lookups pinned to the webOS SDK sysroot so
# consumers do not need to repeat the same cache hints at every configure site.
function(_webos_cache_sysroot_library cache_var library_name)
    if(DEFINED ${cache_var} AND EXISTS "${${cache_var}}")
        return()
    endif()

    unset(_webos_resolved_library)
    unset(_webos_resolved_library CACHE)
    foreach(_webos_candidate
            "${CMAKE_SYSROOT}/usr/lib/lib${library_name}.so"
            "${CMAKE_SYSROOT}/lib/lib${library_name}.so")
        if(EXISTS "${_webos_candidate}" OR IS_SYMLINK "${_webos_candidate}")
            set(_webos_resolved_library "${_webos_candidate}")
            break()
        endif()
    endforeach()

    find_library(_webos_resolved_library
        NAMES ${library_name}
        PATHS
            "${CMAKE_SYSROOT}/usr/lib"
            "${CMAKE_SYSROOT}/lib"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
    if(NOT _webos_resolved_library)
        file(GLOB _webos_library_matches
            "${CMAKE_SYSROOT}/usr/lib/lib${library_name}.so.*"
            "${CMAKE_SYSROOT}/lib/lib${library_name}.so.*"
        )
        if(_webos_library_matches)
            list(SORT _webos_library_matches)
            list(GET _webos_library_matches 0 _webos_resolved_library)
        endif()
    endif()
    if(_webos_resolved_library)
        set(${cache_var} "${_webos_resolved_library}" CACHE FILEPATH "" FORCE)
    endif()
endfunction()

set(GLESv2_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
set(EGL_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
set(XKB_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
_webos_cache_sysroot_library(GLESv2_LIBRARY GLESv2)
_webos_cache_sysroot_library(EGL_LIBRARY EGL)
_webos_cache_sysroot_library(XKB_LIBRARY xkbcommon)

set(FREETYPE_INCLUDE_DIR_freetype2 "${CMAKE_SYSROOT}/usr/include/freetype2" CACHE PATH "")
set(FREETYPE_INCLUDE_DIR_ft2build "${CMAKE_SYSROOT}/usr/include/freetype2" CACHE PATH "")
set(FREETYPE_LIBRARY_RELEASE "${CMAKE_SYSROOT}/usr/lib/libfreetype.so" CACHE FILEPATH "")
set(Fontconfig_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
set(Fontconfig_LIBRARY "${CMAKE_SYSROOT}/usr/lib/libfontconfig.so" CACHE FILEPATH "")

set(Wayland_Client_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
set(Wayland_Server_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
set(Wayland_Cursor_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
set(Wayland_Egl_INCLUDE_DIR "${CMAKE_SYSROOT}/usr/include" CACHE PATH "")
_webos_cache_sysroot_library(Wayland_Client_LIBRARY wayland-client)
_webos_cache_sysroot_library(Wayland_Server_LIBRARY wayland-server)
_webos_cache_sysroot_library(Wayland_Cursor_LIBRARY wayland-cursor)
_webos_cache_sysroot_library(Wayland_Egl_LIBRARY wayland-egl)
