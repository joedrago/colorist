# ---------------------------------------------------------------------------
#                         Copyright Joe Drago 2018.
#         Distributed under the Boost Software License, Version 1.0.
#            (See accompanying file LICENSE_1_0.txt or copy at
#                  http://www.boost.org/LICENSE_1_0.txt)
# ---------------------------------------------------------------------------

# Everything in this file is awful. Replace it someday with proper per-platform find_package()/find_library() stuff.#

set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

# Currently not building zlib or libpng because openjp2 does it already.
# I should stop using theirs and build my own, and somehow convince
# openjpeg to use what I use.
#
#  add_subdirectory(zlib)
#  set(PNG_BUILD_ZLIB "${CMAKE_CURRENT_SOURCE_DIR}/zlib" CACHE STRING "")
#  set(PNG_SHARED OFF CACHE BOOL "")
#  set(PNG_TESTS OFF CACHE BOOL "")
#  add_subdirectory(libpng)

set(EXECUTABLE_OUTPUT_PATH "" CACHE PATH "Unset OpenJPEG shenanigans")
set(LIBRARY_OUTPUT_PATH "" CACHE PATH "Unset OpenJPEG shenanigans")
set(BUILD_THIRDPARTY ON CACHE BOOL "")
add_subdirectory(openjpeg)

set(COLORIST_EXT_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/liblcms2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/libz"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/libpng"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/src/lib/openjp2"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/include"
    "${OPENJPEG_BINARY_DIR}/src/lib/openjp2"
)
set(COLORIST_EXT_INCLUDES ${COLORIST_EXT_INCLUDES} PARENT_SCOPE)

set(COLORIST_EXT_LIBS
    z
    png
    lcms2
    openjp2
)
set(COLORIST_EXT_LIBS ${COLORIST_EXT_LIBS} PARENT_SCOPE)
