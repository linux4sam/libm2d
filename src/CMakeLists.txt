add_library(m2d
    m2d.c
)

set(LIBDRM_MIN_VERSION 2.4.0)

if(GPU MATCHES "^microchip,sam.*-gfx2d$")
    target_sources(m2d PRIVATE gfx2d.c)

    set(NEED_LIBDRM ON)
endif()

string(COMPARE EQUAL ${GPU} "vivante,gc-etnaviv" IS_ETNAVIV)
if(IS_ETNAVIV)
    target_sources(m2d PRIVATE vivante_gc.c etnaviv.c)
    target_include_directories(m2d PRIVATE ${CMAKE_SOURCE_DIR}/src/vivante_gc})

    pkg_check_modules(LIBDRM_ETNAVIV REQUIRED libdrm_etnaviv>=${LIBDRM_MIN_VERSION})
    set(AX_PACKAGE_REQUIRES_PRIVATE "${AX_PACKAGE_REQUIRES_PRIVATE} libdrm_etnaviv >= ${LIBDRM_MIN_VERSION}")

    target_include_directories(m2d PRIVATE ${LIBDRM_ETNAVIV_INCLUDE_DIRS})
    target_compile_options(m2d PRIVATE ${LIBDRM_ETNAVIV_CFLAGS_OTHER})
    target_link_directories(m2d PRIVATE ${LIBDRM_ETNAVIV_LIBRARY_DIRS})
    target_link_libraries(m2d PRIVATE ${LIBDRM_ETNAVIV_LIBRARIES})
    target_link_options(m2d PRIVATE ${LIBDRM_ETNAVIV_LDFLAGS_OTHER})

    if(ENABLE_VIVANTE_GC_DUMP_CMD_BUF)
        target_compile_definitions(m2d PRIVATE VIVANTE_GC_DUMP_CMD_BUF=1)
        target_link_options(m2d PRIVATE -Wl,--wrap=etna_cmd_stream_flush)
    endif()

    set(NEED_LIBDRM ON)
endif()

string(COMPARE EQUAL ${GPU} "vivante,gc-nano2d" IS_NANO2D)
if(IS_NANO2D)
    target_sources(m2d PRIVATE vivante_gc.c nano2d.c)
    target_include_directories(m2d PRIVATE ${CMAKE_SOURCE_DIR}/src/vivante_gc})

    set(NANO2D_MIN_VERSION 2.0.41)
    pkg_check_modules(NANO2D REQUIRED nano2D>=${NANO2D_MIN_VERSION})
    set(AX_PACKAGE_REQUIRES_PRIVATE "${AX_PACKAGE_REQUIRES_PRIVATE} nano2D >= ${NANO2D_MIN_VERSION}")

    target_include_directories(m2d PRIVATE ${NANO2D_INCLUDE_DIRS})
    target_compile_options(m2d PRIVATE ${NANO2D_CFLAGS_OTHER})
    target_link_directories(m2d PRIVATE ${NANO2D_LIBRARY_DIRS})
    target_link_libraries(m2d PRIVATE ${NANO2D_LIBRARIES})
    target_link_options(m2d PRIVATE ${NANO2D_LDFLAGS_OTHER})

    if(ENABLE_VIVANTE_GC_DUMP_CMD_BUF)
        target_compile_definitions(m2d PRIVATE VIVANTE_GC_DUMP_CMD_BUF=1)
    endif()
endif()

set_target_properties(m2d PROPERTIES VERSION 2.1.0 SOVERSION 2)

target_sources(m2d PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include
    FILES
    ${CMAKE_BINARY_DIR}/include/m2d/version.h
    ${CMAKE_SOURCE_DIR}/include/m2d/m2d.h
)

add_custom_target(generate_gitversion_h
    COMMAND ${CMAKE_COMMAND} -DTOP_SOURCE_DIR=${CMAKE_SOURCE_DIR} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/gitversion.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_dependencies(m2d generate_gitversion_h)

target_compile_definitions(m2d PRIVATE $<$<CONFIG:Debug>:LIBM2D_ACTIVE_LEVEL=0>)

target_include_directories(m2d PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_BINARY_DIR}
)

if(NEED_LIBDRM)
    pkg_check_modules(LIBDRM REQUIRED libdrm>=${LIBDRM_MIN_VERSION})
    set(AX_PACKAGE_REQUIRES_PRIVATE "${AX_PACKAGE_REQUIRES_PRIVATE} libdrm >= ${LIBDRM_MIN_VERSION}")

    target_include_directories(m2d PRIVATE ${LIBDRM_INCLUDE_DIRS})
    target_compile_options(m2d PRIVATE ${LIBDRM_CFLAGS_OTHER})
    target_link_directories(m2d PRIVATE ${LIBDRM_LIBRARY_DIRS})
    target_link_libraries(m2d PRIVATE ${LIBDRM_LIBRARIES})
    target_link_options(m2d PRIVATE ${LIBDRM_LDFLAGS_OTHER})

    target_sources(m2d PRIVATE m2d_drm.c)
endif()

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix \${prefix})
set(libdir \${exec_prefix}/lib)
set(includedir \${prefix}/include)

configure_file(../libm2d.pc.in ../libm2d.pc @ONLY)
configure_file(../include/m2d/version.h.in ../include/m2d/version.h)

install(TARGETS m2d
    LIBRARY
    FILE_SET HEADERS
)
install(FILES ${CMAKE_BINARY_DIR}/libm2d.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
