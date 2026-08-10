# Which firmware to build is chosen per-env via -DKBDRELAY_APP=<name>
# (see platformio.ini board_build.cmake_extra_args). Defaults to kbd.
if(NOT DEFINED KBDRELAY_APP)
    set(KBDRELAY_APP "kbd")
endif()

idf_component_register(
    SRCS "${KBDRELAY_APP}/main.c"
    INCLUDE_DIRS "."
    REQUIRES bridge_proto
)
