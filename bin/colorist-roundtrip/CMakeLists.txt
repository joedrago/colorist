# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

if(NOT COLORIST_EMSCRIPTEN)

set(COLORIST_ROUNDTRIP_SRCS
    main.c
)

add_executable(colorist-roundtrip
     ${COLORIST_ROUNDTRIP_SRCS}
)
target_link_libraries(colorist-roundtrip colorist)

endif()
