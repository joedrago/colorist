# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

if(NOT COLORIST_EMSCRIPTEN)

set(COLORIST_TEST_SRCS
    main.c
    main.h

    test_coverage.c
    test_strings.c
)

add_executable(colorist-test
     ${COLORIST_TEST_SRCS}
)
target_link_libraries(colorist-test colorist unity)

endif()
