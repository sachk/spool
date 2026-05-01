set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(DEFINED ENV{WEBOS_SDK_ROOT} AND NOT "$ENV{WEBOS_SDK_ROOT}" STREQUAL "")
    set(WEBOS_SDK_ROOT "$ENV{WEBOS_SDK_ROOT}")
else()
    get_filename_component(WEBOS_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
    get_filename_component(WEBOS_REPO_ROOT "${WEBOS_TOOLCHAIN_DIR}/../.." ABSOLUTE)
    set(WEBOS_SDK_ROOT "${WEBOS_REPO_ROOT}/build/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot")
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
set(PKG_CONFIG_EXECUTABLE "${_toolchain_dir}/pkg-config-webos.sh" CACHE FILEPATH "")
