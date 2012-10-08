# Copyright (C) 2012
# Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
cmake_policy(SET CMP0015 NEW)

project(GLS C)

# Build options
option(GLS_BUILD_STATIC_LIB "Build static version of libgls" ON)
option(GLS_BUILD_SHARED_LIB "Build shared version of libgls" ON)
option(GLS_BUILD_TEST "Build GLS test application (requires GLUT)" ON)
option(GLS_BUILD_DOCUMENTATION "Build API reference documentation (requires Doxygen)" ON)

# Libgls version
set(GLS_VERSION_MAJOR "1")
set(GLS_VERSION_MINOR "0")
set(GLS_VERSION_PATCH "0")
set(GLS_VERSION ${GLS_VERSION_MAJOR}.${GLS_VERSION_MINOR}.${GLS_VERSION_PATCH})
set(GLS_LIB_VERSION "0.0.0")
set(GLS_LIB_SOVERSION "0")

# Main target: libgls
find_package(GLEWMX REQUIRED)
find_package(X11)
if(X11_FOUND)
  add_definitions(-DGLS_USE_GLX=1)
else()
  add_definitions(-DGLS_USE_GLX=0)
endif()
configure_file("${GLS_SOURCE_DIR}/gls/gls_version.h.in" "${GLS_BINARY_DIR}/gls/gls_version.h" @ONLY)
include(StringifyShaders)
stringify_shaders(gls/gls.glsl)
include_directories("${GLEWMX_INCLUDE_DIRS}" "${GLS_BINARY_DIR}" "${GLS_BINARY_DIR}/gls")
link_directories("${GLEWMX_LIBRARY_DIRS}")
if(GLS_BUILD_SHARED_LIB)
  add_library(libgls_shared SHARED gls/gls.c gls/gls.glsl.h gls/gls.h gls/gls_version.h)
  target_link_libraries(libgls_shared ${GLEWMX_LIBRARIES})
  if(WIN32)
    target_link_libraries(libgls_shared opengl32)
  endif(WIN32)
  set_target_properties(libgls_shared PROPERTIES OUTPUT_NAME gls)
  set_target_properties(libgls_shared PROPERTIES VERSION ${GLS_LIB_VERSION})
  set_target_properties(libgls_shared PROPERTIES SOVERSION ${GLS_LIB_SOVERSION})  
  install(TARGETS libgls_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
  )
endif()
if(GLS_BUILD_STATIC_LIB)
  add_library(libgls_static STATIC gls/gls.c)
  set_target_properties(libgls_static PROPERTIES OUTPUT_NAME gls)
  install(TARGETS libgls_static
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
  )
endif()
install(FILES gls/gls.h "${GLS_BINARY_DIR}/gls/gls_version.h" DESTINATION include)
# pkg-config file: gls.pc
set(prefix ${CMAKE_INSTALL_PREFIX})
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
configure_file("${GLS_SOURCE_DIR}/gls.pc.in" "${GLS_BINARY_DIR}/gls.pc" @ONLY)
install(FILES "${GLS_BINARY_DIR}/gls.pc" DESTINATION lib/pkgconfig)

# Optional target: gls-test
if(GLS_BUILD_TEST)
  find_package(GLUT REQUIRED)
  add_executable(test_program test/gls-test.c)
  set_target_properties(test_program PROPERTIES OUTPUT_NAME gls-test)
  include_directories("${GLUT_INCLUDE_DIR}" "${GLS_SOURCE_DIR}" "${GLS_BINARY_DIR}")
  if(GLS_BUILD_SHARED_LIB)
    target_link_libraries(test_program libgls_shared ${GLUT_LIBRARIES})
  else()
    target_link_libraries(test_program libgls_static ${GLUT_LIBRARIES} ${GLEWMX_LIBRARIES})
  endif()
  install(TARGETS test_program RUNTIME DESTINATION bin)
endif()

# Optional target: reference documentation
if(GLS_BUILD_DOCUMENTATION)
  find_package(Doxygen REQUIRED)
  configure_file("${GLS_SOURCE_DIR}/doc/doxyfile.in" "${GLS_BINARY_DIR}/doc/doxyfile" @ONLY)
  file(MAKE_DIRECTORY "${GLS_BINARY_DIR}/doc/html")
  file(COPY "${GLS_SOURCE_DIR}/doc/gamma-pattern-tb.png" DESTINATION "${GLS_BINARY_DIR}/doc/html")
  file(COPY "${GLS_SOURCE_DIR}/doc/crosstalk-pattern-tb.png" DESTINATION "${GLS_BINARY_DIR}/doc/html")
  add_custom_command(OUTPUT "${GLS_BINARY_DIR}/doc/html/index.html"
    COMMAND ${DOXYGEN_EXECUTABLE} "${GLS_BINARY_DIR}/doc/doxyfile"
    WORKING_DIRECTORY "${GLS_BINARY_DIR}/doc"
    DEPENDS "${GLS_SOURCE_DIR}/doc/doxyfile.in" "${GLS_SOURCE_DIR}/gls/gls.h"
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )
  add_custom_target(doc ALL DEPENDS "${GLS_BINARY_DIR}/doc/html/index.html")
  install(DIRECTORY "${GLS_BINARY_DIR}/doc/html" DESTINATION share/doc)
  install(FILES "${GLS_SOURCE_DIR}/doc/FindGLS.cmake" DESTINATION share/doc)
endif()

# Extra target: 'make dist' for making
set(ARCHIVE_NAME libgls-${GLS_VERSION})
add_custom_target(dist
  COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
    | xz > ${GLS_BINARY_DIR}/${ARCHIVE_NAME}.tar.xz
  WORKING_DIRECTORY ${GLS_SOURCE_DIR})
