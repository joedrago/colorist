# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

MACRO(generate_headers)
    FOREACH (_current_FILE ${ARGN})
      GET_FILENAME_COMPONENT(_in ${_current_FILE} ABSOLUTE)
      GET_FILENAME_COMPONENT(_basename ${_current_FILE} NAME_WE)

      set(_out ${CMAKE_CURRENT_BINARY_DIR}/${_basename}Binary.h)

      ADD_CUSTOM_COMMAND(
         OUTPUT ${_out}
         COMMAND genHeader
         ARGS
         -i ${_in}
         -o ${_out}
         -p ${_basename}
         DEPENDS ${_in}
         DEPENDS genHeader
      )
   ENDFOREACH (_current_FILE)
ENDMACRO(generate_headers)

include(${CMAKE_CURRENT_SOURCE_DIR}/bin2h.cmake)

set(COLORIST_LIB_INCLUDES
    include/colorist/colorist.h
    include/colorist/context.h
    include/colorist/image.h
    include/colorist/pixelmath.h
    include/colorist/profile.h
    include/colorist/raw.h
    include/colorist/task.h
    include/colorist/transform.h
    include/colorist/types.h
    include/colorist/version.h
    include/colorist/embedded.h
    ${CMAKE_CURRENT_BINARY_DIR}/reportTemplateBinary.h
    ${CMAKE_CURRENT_BINARY_DIR}/pqCurveBinary.h
    ${CMAKE_CURRENT_BINARY_DIR}/hlgCurveBinary.h
)

generate_headers(templates/reportTemplate.html)
generate_headers(raw/pqCurve.bin)
generate_headers(raw/hlgCurve.bin)

set(COLORIST_LIB_SRCS
    src/context.c
    src/context_convert.c
    src/context_formats.c
    src/context_generate.c
    src/context_identify.c
    src/context_log.c
    src/context_memory.c
    src/context_modify.c
    src/context_report.c
    src/context_rw.c
    src/context_version.c
    src/embedded.c
    src/format_avif.c
    src/format_bmp.c
    src/format_jp2.c
    src/format_jpg.c
    src/format_png.c
    src/format_tiff.c
    src/format_webp.c
    src/image.c
    src/image_debugdump.c
    src/image_diff.c
    src/image_highlight.c
    src/image_stats.c
    src/image_string.c
    src/pixelmath_grade.c
    src/pixelmath_resize.c
    src/pixelmath_scale.c
    src/profile.c
    src/profile_curves.c
    src/profile_debugdump.c
    src/raw.c
    src/task.c
    src/transform.c
    src/types.c
)

include_directories(
    include
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(-DOPJ_STATIC)

add_library(colorist
     ${COLORIST_LIB_INCLUDES}
     ${COLORIST_LIB_SRCS}
)
target_link_libraries(colorist ${COLORIST_EXT_LIBS})
if(UNIX)
    target_link_libraries(colorist m)
endif()
