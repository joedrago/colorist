cmake_minimum_required(VERSION 3.2)
project(colorist)

add_subdirectory(ext)
include_directories(${COLORIST_EXT_INCLUDES})

add_subdirectory(lib)
include_directories(lib/include)

add_subdirectory(bin)
