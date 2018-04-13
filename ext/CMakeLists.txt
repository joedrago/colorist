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

include_directories(jpeg)
add_library(jpeg
    #jpeg/ansi2knr.c
    jpeg/cdjpeg.c
    #jpeg/cjpeg.c
    #jpeg/ckconfig.c
    #jpeg/djpeg.c
    # jpeg/example.c
    jpeg/jcapimin.c
    jpeg/jcapistd.c
    jpeg/jccoefct.c
    jpeg/jccolor.c
    jpeg/jcdctmgr.c
    jpeg/jchuff.c
    jpeg/jcinit.c
    jpeg/jcmainct.c
    jpeg/jcmarker.c
    jpeg/jcmaster.c
    jpeg/jcomapi.c
    jpeg/jcparam.c
    jpeg/jcphuff.c
    jpeg/jcprepct.c
    jpeg/jcsample.c
    jpeg/jctrans.c
    jpeg/jdapimin.c
    jpeg/jdapistd.c
    jpeg/jdatadst.c
    jpeg/jdatasrc.c
    jpeg/jdcoefct.c
    jpeg/jdcolor.c
    jpeg/jddctmgr.c
    jpeg/jdhuff.c
    jpeg/jdinput.c
    jpeg/jdmainct.c
    jpeg/jdmarker.c
    jpeg/jdmaster.c
    jpeg/jdmerge.c
    jpeg/jdphuff.c
    jpeg/jdpostct.c
    jpeg/jdsample.c
    jpeg/jdtrans.c
    jpeg/jerror.c
    jpeg/jfdctflt.c
    jpeg/jfdctfst.c
    jpeg/jfdctint.c
    jpeg/jidctflt.c
    jpeg/jidctfst.c
    jpeg/jidctint.c
    jpeg/jidctred.c
    #jpeg/jmemansi.c
    #jpeg/jmemdos.c
    #jpeg/jmemmac.c
    jpeg/jmemmgr.c
    #jpeg/jmemname.c
    jpeg/jmemnobs.c
    #jpeg/jpegtran.c
    jpeg/jquant1.c
    jpeg/jquant2.c
    jpeg/jutils.c
    jpeg/rdbmp.c
    jpeg/rdcolmap.c
    jpeg/rdgif.c
    #jpeg/rdjpgcom.c
    jpeg/rdppm.c
    jpeg/rdrle.c
    jpeg/rdswitch.c
    jpeg/rdtarga.c
    jpeg/transupp.c
    jpeg/wrbmp.c
    jpeg/wrgif.c
    jpeg/wrjpgcom.c
    jpeg/wrppm.c
    jpeg/wrrle.c
    jpeg/wrtarga.c
)

set(COLORIST_EXT_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/liblcms2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/libz"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/libpng"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/src/lib/openjp2"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/include"
    "${OPENJPEG_BINARY_DIR}/src/lib/openjp2"
    "${CMAKE_CURRENT_SOURCE_DIR}/jpeg"
)
set(COLORIST_EXT_INCLUDES ${COLORIST_EXT_INCLUDES} PARENT_SCOPE)

set(COLORIST_EXT_LIBS
    png
    lcms2
    openjp2
    jpeg
    z
)
set(COLORIST_EXT_LIBS ${COLORIST_EXT_LIBS} PARENT_SCOPE)
