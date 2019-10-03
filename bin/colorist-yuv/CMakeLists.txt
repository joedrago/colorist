# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

set(COLORIST_YUV_SRCS
    main.c
)

add_executable(colorist-yuv
     ${COLORIST_YUV_SRCS}
)
target_link_libraries(colorist-yuv colorist)
