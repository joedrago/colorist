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

include_directories(cJSON)
add_library(cjson
    cJSON/cJSON.c
    cJSON/cJSON.h
)

include_directories(stb)
add_library(stb
    stb/stb_image_resize.c
    stb/stb_image_resize.h
)

add_subdirectory(genHeader)

set(EXECUTABLE_OUTPUT_PATH "" CACHE PATH "Unset OpenJPEG shenanigans")
set(LIBRARY_OUTPUT_PATH "" CACHE PATH "Unset OpenJPEG shenanigans")
set(BUILD_THIRDPARTY ON CACHE BOOL "")
add_subdirectory(openjpeg)

set(WEBP_BUILD_CWEBP OFF CACHE BOOL "")
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "")
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "")
set(WEBP_BUILD_IMG2WEBP ON CACHE BOOL "")
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "")
add_subdirectory(libwebp)

include_directories(jpeg)
add_library(jpeg
    jpeg/cdjpeg.c
    jpeg/jaricom.c
    jpeg/jcapimin.c
    jpeg/jcapistd.c
    jpeg/jcarith.c
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
    jpeg/jcprepct.c
    jpeg/jcsample.c
    jpeg/jctrans.c
    jpeg/jdapimin.c
    jpeg/jdapistd.c
    jpeg/jdarith.c
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
    jpeg/jmemmgr.c
    jpeg/jmemnobs.c
    jpeg/jquant1.c
    jpeg/jquant2.c
    jpeg/jutils.c
    jpeg/rdbmp.c
    jpeg/rdcolmap.c
    jpeg/rdgif.c
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
    "${CMAKE_CURRENT_SOURCE_DIR}/cJSON"
    "${CMAKE_CURRENT_SOURCE_DIR}/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/liblcms2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/libz"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/libpng"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/libtiff"
    "${libtiff_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/src/lib/openjp2"
    "${CMAKE_CURRENT_SOURCE_DIR}/openjpeg/thirdparty/include"
    "${OPENJPEG_BINARY_DIR}/src/lib/openjp2"
    "${CMAKE_CURRENT_SOURCE_DIR}/jpeg"
    "${CMAKE_CURRENT_SOURCE_DIR}/libwebp/imageio"
    "${CMAKE_CURRENT_SOURCE_DIR}/libwebp/src/webp"
)
set(COLORIST_EXT_INCLUDES ${COLORIST_EXT_INCLUDES} PARENT_SCOPE)

set(COLORIST_EXT_LIBS
    cjson
    png
    lcms2
    openjp2
    jpeg
    stb
    tiff
    z
    webp
    webpmux
    webpdemux
)
set(COLORIST_EXT_LIBS ${COLORIST_EXT_LIBS} PARENT_SCOPE)

# For VS users
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

macro(set_folder_safe target folder)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER ${folder})
    endif()
endmacro()

set_folder_safe(cjson "ext")
set_folder_safe(genHeader "ext")
set_folder_safe(jpeg "ext")
set_folder_safe(lcms2 "ext")
set_folder_safe(openjp2 "ext/openjpeg")
set_folder_safe(opj_compress "ext/openjpeg")
set_folder_safe(opj_decompress "ext/openjpeg")
set_folder_safe(opj_dump "ext/openjpeg")
set_folder_safe(png "ext")
set_folder_safe(stb "ext")
set_folder_safe(tiff "ext/openjpeg")
set_folder_safe(webp "ext/webp")
set_folder_safe(webpdecode "ext/webp")
set_folder_safe(webpdecoder "ext/webp")
set_folder_safe(webpdemux "ext/webp")
set_folder_safe(webpdsp "ext/webp")
set_folder_safe(webpdspdecode "ext/webp")
set_folder_safe(webpencode "ext/webp")
set_folder_safe(webpmux "ext/webp")
set_folder_safe(webputils "ext/webp")
set_folder_safe(webputilsdecode "ext/webp")
set_folder_safe(z "ext")

# TODO: figure out how to make these go away without losing libwebpmux
set_folder_safe(exampleutil "ext/webp")
set_folder_safe(imagedec "ext/webp")
set_folder_safe(imageenc "ext/webp")
set_folder_safe(imageioutil "ext/webp")
set_folder_safe(img2webp "ext/webp")
