# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

set(COLORIST_LIB_INCLUDES
    include/colorist/colorist.h
    include/colorist/context.h
    include/colorist/image.h
    include/colorist/pixelmath.h
    include/colorist/profile.h
    include/colorist/task.h
    include/colorist/types.h
    include/colorist/version.h
)

set(COLORIST_LIB_SRCS
    src/context.c
    src/context_convert.c
    src/context_generate.c
    src/context_identify.c
    src/context_log.c
    src/context_memory.c
    src/context_rw.c
    src/context_version.c
    src/image.c
    src/image_debugdump.c
    src/image_jp2.c
    src/image_jpg.c
    src/image_png.c
    src/pixelmath_grade.c
    src/pixelmath_scale.c
    src/profile.c
    src/profile_debugdump.c
    src/task.c
    src/types.c
)

include_directories(include)

add_definitions(-DOPJ_STATIC)

add_library(colorist
     ${COLORIST_LIB_INCLUDES}
     ${COLORIST_LIB_SRCS}
)
target_link_libraries(colorist ${COLORIST_EXT_LIBS})
if(UNIX)
    target_link_libraries(colorist m)
endif()
