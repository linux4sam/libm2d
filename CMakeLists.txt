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

add_compile_options(-Wall -Wextra -Wpedantic -Wsign-compare -Wmissing-declarations)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb")

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBDRM REQUIRED libdrm>=2.4.0)
set(AX_PACKAGE_REQUIRES_PRIVATE "libdrm >= 2.4.0")

set(PACKAGE_VERSION ${PROJECT_VERSION})

set(SUPPORTED_GPUS "microchip,sam9x60-gfx2d" "microchip,sam9x7-gfx2d")
if(NOT DEFINED GPU)
    list(GET SUPPORTED_GPUS 0 GPU)
else()
    string(TOLOWER "${GPU}" GPU)
endif()
if(GPU IN_LIST SUPPORTED_GPUS)
    message(CHECK_START "Selecting GPU: \"${GPU}\"")
else()
    message(FATAL_ERROR "unsupported GPU: \"${GPU}\" (supported GPUs: ${SUPPORTED_GPUS})")
endif()

add_subdirectory(src)

option(ENABLE_TESTS "build tests [default=OFF]" OFF)
if(ENABLE_TESTS)
    pkg_check_modules(CAIRO REQUIRED cairo>=1.14.6)
    pkg_check_modules(LIBPLANES REQUIRED libplanes>=1.1.0)
    add_subdirectory(test)
endif()
