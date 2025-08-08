cmake_minimum_required(VERSION 3.10)

project("PlatinumSrc")

set(PSRC_ROMFS "" CACHE STRING "Name of the PAF archive in 'src/main/assets'")
set(PSRC_DEFAULTGAME "" CACHE STRING "Name of the game to run instead of 'default'")

add_custom_target(
    psrc ALL
    DEPENDS "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpsrc.so" sentry
    COMMAND :
)

add_custom_target(
    sentry
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/sentry1"
)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SDL")
find_library(SDL2 SDL2)

add_custom_command(
    OUTPUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpsrc.so" "${CMAKE_CURRENT_BINARY_DIR}/sentry1"
    DEPENDS SDL2
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/psrc"
    COMMAND make ROMFS_EXT='${PSRC_ROMFS}' DEFAULTGAME='${PSRC_DEFAULTGAME}' CROSS=android OUTDIR='${CMAKE_LIBRARY_OUTPUT_DIRECTORY}' CFLAGS='-I${CMAKE_CURRENT_SOURCE_DIR}/SDL/include' LDFLAGS='-L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}' CC='${CMAKE_C_COMPILER} --target=${CMAKE_C_COMPILER_TARGET}' PLATFORM='Android_${CMAKE_SYSTEM_PROCESSOR}' -j$$(nproc)
)
