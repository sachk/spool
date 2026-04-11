Qt OpenAPI

The Qt OpenAPI module provides functionality for generating Qt HTTP clients using Qt Network RESTful APIs. The main module features are the Qt OpenAPI generator and pre-generated OpenApiCommon library.

The Qt OpenAPI generator is a plugin to the OpenAPI generator. It allows you to autogenerate Qt HTTP clients in C++ using Qt Network APIs such as QRestAccessManager.

Note: Qt OpenAPI in 6.11 is in Technical Preview, excluding its API from Qt's compatibility promises.
Using the module

Installation of the following packages is necessary to use the Qt OpenAPI module:

    OpenAPI generator (requires OpenAPI 7.12.0 or above).
    Maven plugin (requires Maven 3.0 or above).
    Java JDK (requires JDK 17 or above).

Once you have the required installations, the Qt OpenAPI generator is ready for use within your projects. To generate client code from an OpenAPI specification using the Qt OpenAPI Generator, call the qt_add_openapi_client function inside your project's CMakeLists.txt file. See Building with CMake for details.
Building with CMake
Using the qt_add_openapi_client function

Use the find_package() command to locate the needed module components in the Qt6 package. Then call the qt_add_openapi_client function to generate a required Qt HTTP client library. See the full CMake code example below:

cmake_minimum_required(VERSION 3.22)

project(openapiApplication LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core OpenApiCommon OpenApiTools)

qt_add_executable(openapiApplication
main.cpp
)
qt_add_library(generatedLibrary)
qt_add_openapi_client(generatedLibrary
SPEC_FILE
${CMAKE_CURRENT_SOURCE_DIR}/spec.yaml
)
target_link_libraries(openapiApplication Qt6::Core generatedLibrary)

include(GNUInstallDirs)
install(TARGETS openapiApplication
LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
