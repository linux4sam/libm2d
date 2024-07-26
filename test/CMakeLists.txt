add_executable(m2d_test test.c utils.c)
target_link_libraries(m2d_test PRIVATE m2d)

target_compile_definitions(m2d_test PRIVATE TESTDATA="${CMAKE_INSTALL_FULL_DATADIR}/m2d")

target_include_directories(m2d_test PRIVATE ${LIBPLANES_INCLUDE_DIRS})
target_compile_options(m2d_test PRIVATE ${LIBPLANES_CFLAGS_OTHER})
target_link_directories(m2d_test PRIVATE ${LIBPLANES_LIBRARY_DIRS})
target_link_libraries(m2d_test PRIVATE ${LIBPLANES_LIBRARIES})
target_link_options(m2d_test PRIVATE ${LIBPLANES_LDFLAGS_OTHER})

target_include_directories(m2d_test PRIVATE ${CAIRO_INCLUDE_DIRS})
target_compile_options(m2d_test PRIVATE ${CAIRO_CFLAGS_OTHER})
target_link_directories(m2d_test PRIVATE ${CAIRO_LIBRARY_DIRS})
target_link_libraries(m2d_test PRIVATE ${CAIRO_LIBRARIES})
target_link_options(m2d_test PRIVATE ${CAIRO_LDFLAGS_OTHER})

install(TARGETS m2d_test RUNTIME)
install(DIRECTORY resources/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/m2d
        FILES_MATCHING
        PATTERN *.png)
