cmake_minimum_required(VERSION 3.10)
project(apg)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_definitions(-std=c99) # Enforce C99 for gcc
endif()

add_subdirectory(ext)

include_directories(
    include
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/aom"
    "${AOM_BINARY_DIR}"
)
add_library(apg STATIC
    include/apg.h
    src/apg.c
)
target_link_libraries(apg aom)

option(APG_BUILD_EXAMPLES "Build APG Examples." OFF)

if(APG_BUILD_EXAMPLES)
    add_executable(apg_example1 examples/apg_example1.c)
    target_link_libraries(apg_example1 apg)
endif()

macro(apg_set_folder_safe target folder)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER ${folder})
    endif()
endmacro()

apg_set_folder_safe(aom "ext/apg")
apg_set_folder_safe(aom_av1 "ext/apg")
apg_set_folder_safe(aom_av1_common "ext/apg")
apg_set_folder_safe(aom_av1_common_avx2_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_common_sse2_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_common_sse4_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_common_ssse3_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_decoder "ext/apg")
apg_set_folder_safe(aom_av1_encoder "ext/apg")
apg_set_folder_safe(aom_av1_encoder_avx2_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_encoder_sse2 "ext/apg")
apg_set_folder_safe(aom_av1_encoder_sse2_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_encoder_sse3_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_encoder_sse42_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_encoder_sse4_intrinsics "ext/apg")
apg_set_folder_safe(aom_av1_encoder_ssse3 "ext/apg")
apg_set_folder_safe(aom_av1_encoder_ssse3_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp "ext/apg")
apg_set_folder_safe(aom_dsp_common "ext/apg")
apg_set_folder_safe(aom_dsp_common_avx2_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp_common_sse2 "ext/apg")
apg_set_folder_safe(aom_dsp_common_sse2_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp_common_sse4_1_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp_common_ssse3 "ext/apg")
apg_set_folder_safe(aom_dsp_common_ssse3_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp_decoder "ext/apg")
apg_set_folder_safe(aom_dsp_encoder "ext/apg")
apg_set_folder_safe(aom_dsp_encoder_avx "ext/apg")
apg_set_folder_safe(aom_dsp_encoder_avx2_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp_encoder_sse2 "ext/apg")
apg_set_folder_safe(aom_dsp_encoder_sse2_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp_encoder_sse4_1_intrinsics "ext/apg")
apg_set_folder_safe(aom_dsp_encoder_ssse3 "ext/apg")
apg_set_folder_safe(aom_dsp_encoder_ssse3_intrinsics "ext/apg")
apg_set_folder_safe(aom_mem "ext/apg")
apg_set_folder_safe(aom_ports "ext/apg")
apg_set_folder_safe(aom_rtcd "ext/apg")
apg_set_folder_safe(aom_scale "ext/apg")
apg_set_folder_safe(aom_util "ext/apg")
apg_set_folder_safe(aom_version "ext/apg")
apg_set_folder_safe(aom_version_check "ext/apg")
apg_set_folder_safe(apg "ext/apg")
apg_set_folder_safe(dist "ext/apg")
