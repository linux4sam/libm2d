add_library(m2d SHARED
    m2d.c
)

if(GPU MATCHES "^microchip,sam.*-gfx2d$")
    target_sources(m2d PRIVATE gfx2d.c)
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

target_include_directories(m2d PRIVATE ${LIBDRM_INCLUDE_DIRS})
target_compile_options(m2d PRIVATE ${LIBDRM_CFLAGS_OTHER})
target_link_directories(m2d PRIVATE ${LIBDRM_LIBRARY_DIRS})
target_link_libraries(m2d PRIVATE ${LIBDRM_LIBRARIES})
target_link_options(m2d PRIVATE ${LIBDRM_LDFLAGS_OTHER})

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
