set(COLORIST_BIN_SRCS
    main.c
)

add_executable(colorist-bin
     ${COLORIST_BIN_SRCS}
)
target_link_libraries(colorist-bin colorist)
set_target_properties(colorist-bin PROPERTIES OUTPUT_NAME colorist)
