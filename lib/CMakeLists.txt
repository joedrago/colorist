# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

set(COLORIST_LIB_INCLUDES
    include/colorist/colorist.h
    include/colorist/image.h
    include/colorist/profile.h
    include/colorist/types.h
)

set(COLORIST_LIB_SRCS
    src/colorist.c
    src/image.c
    src/image_debugdump.c
    src/image_png.c
    src/profile.c
    src/profile_debugdump.c
    src/types.c
)

include_directories(include)

add_library(colorist
     ${COLORIST_LIB_INCLUDES}
     ${COLORIST_LIB_SRCS}
)
target_link_libraries(colorist ${COLORIST_EXT_LIBS})
if(UNIX)
    target_link_libraries(colorist m)
endif()
