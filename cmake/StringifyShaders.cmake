# Adapted from EqStringfyShaders.cmake from Equalizer
# in 2012 by Martin Lambers <marlam@marlam.de>
#
# Original copyright:
##
# Copyright (c) 2010 Daniel Pfeifer <daniel@pfeifer-mail.de>
##

macro(STRINGIFY_SHADERS)
  foreach(FILE ${ARGN})
    set(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
    set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${FILE})
    set(OUTPUT_FILES ${OUTPUT}.h)
    add_custom_command(OUTPUT ${OUTPUT_FILES}
      COMMAND ${CMAKE_COMMAND} -DSTRINGIFY_SHADERS_PROCESSING_MODE=ON
        -DINPUT=${INPUT} -DOUTPUT=${OUTPUT}
        -P ${CMAKE_SOURCE_DIR}/cmake/StringifyShaders.cmake
      DEPENDS ${INPUT}
      COMMENT "Stringifying shader source code"
      VERBATIM
    )
  endforeach()
endmacro()

if(NOT STRINGIFY_SHADERS_PROCESSING_MODE)
  return()
endif()

#

get_filename_component(FILENAME ${INPUT} NAME)
string(REGEX REPLACE "[.-]" "_" NAME ${FILENAME})
string(TOUPPER ${NAME} NAME)

file(STRINGS ${INPUT} LINES)

file(WRITE ${OUTPUT}.h
  "/* Generated file, do not edit! */\n"
  "#ifndef ${NAME}_STR\n"
  "#define ${NAME}_STR \\\n"
)

foreach(LINE ${LINES})
  string(REPLACE "\"" "\\\"" LINE "${LINE}")
  file(APPEND ${OUTPUT}.h "\"${LINE}\\n\" \\\n")
endforeach()

file(APPEND ${OUTPUT}.h "\"\"\n")
file(APPEND ${OUTPUT}.h "#endif\n")
