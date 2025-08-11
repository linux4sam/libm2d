cmake_minimum_required(VERSION 3.22)

project(
    libm2d
    VERSION 2.0.0
    DESCRIPTION "A linux middleware graphic library for Microchip MPUs"
    LANGUAGES C
)
#set(PROJECT_VERSION_RC 1)
if(DEFINED PROJECT_VERSION_RC)
    string(APPEND PROJECT_VERSION "-rc${PROJECT_VERSION_RC}")
endif()

include(GNUInstallDirs)

set(CMAKE_VERBOSE_MAKEFILE True)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb")

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBDRM REQUIRED libdrm>=2.4.0)
set(AX_PACKAGE_REQUIRES_PRIVATE "libdrm >= 2.4.0")

set(PACKAGE_VERSION ${PROJECT_VERSION})

add_subdirectory(src)

option(ENABLE_TESTS "build tests [default=OFF]" OFF)
if(ENABLE_TESTS)
    pkg_check_modules(CAIRO REQUIRED cairo>=1.14.6)
    pkg_check_modules(LIBPLANES REQUIRED libplanes>=1.1.0)
    add_subdirectory(test)
endif()
