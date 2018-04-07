set(COLORIST_LIB_SRCS
    src/colorist.c
)

include_directories(include)

add_library(colorist
     ${COLORIST_LIB_SRCS}
)
target_link_libraries(colorist ${COLORIST_EXT_LIBS})
# add_dependencies(colorist ${COLORIST_EXT_LIBS})
