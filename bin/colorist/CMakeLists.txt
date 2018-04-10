# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

set(COLORIST_BIN_SRCS
    convert.c
    generate.c
    identify.c
    main.c
    main.h
    read.c
    write.c
)

add_executable(colorist-bin
     ${COLORIST_BIN_SRCS}
)
target_link_libraries(colorist-bin colorist)
set_target_properties(colorist-bin PROPERTIES OUTPUT_NAME colorist)
