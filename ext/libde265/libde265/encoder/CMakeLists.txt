set (encoder_sources
  encoder-core.cc encoder-core.h
  encoder-types.h encoder-types.cc
  encoder-params.h encoder-params.cc
  encoder-context.h encoder-context.cc
  encoder-syntax.h encoder-syntax.cc
  encpicbuf.h encpicbuf.cc
  sop.h sop.cc
)

add_subdirectory (algo)
add_library(de265encoder OBJECT ${encoder_sources})
set(ENCODER_OBJECTS $<TARGET_OBJECTS:de265encoder> ${ALGO_OBJECTS} PARENT_SCOPE)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  SET_TARGET_PROPERTIES(de265encoder PROPERTIES COMPILE_FLAGS "-fPIC")
endif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
