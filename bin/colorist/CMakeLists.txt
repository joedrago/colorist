# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

set(COLORIST_BIN_SRCS
    main.c
)

add_executable(colorist-bin
     ${COLORIST_BIN_SRCS}
)
target_link_libraries(colorist-bin colorist)
set_target_properties(colorist-bin PROPERTIES OUTPUT_NAME colorist)

if(COLORIST_EMSCRIPTEN)
    set_target_properties(colorist-bin PROPERTIES LINK_FLAGS "-s EXTRA_EXPORTED_RUNTIME_METHODS=\"['ccall', 'cwrap', 'stringToUTF8', 'setValue', 'FS']\" -s FORCE_FILESYSTEM=1 -s ALLOW_MEMORY_GROWTH=1")
    set_target_properties(colorist-bin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/emscripten"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/emscripten"
    )
endif()
