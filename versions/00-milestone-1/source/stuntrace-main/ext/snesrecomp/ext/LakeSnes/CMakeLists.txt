cmake_minimum_required(VERSION 3.16)
project(lakesnes_standalone C)
set(CMAKE_C_STANDARD 17)

find_package(SDL2 CONFIG REQUIRED)

file(GLOB SNES_SRC snes/*.c)
add_executable(lakesnes_test ${SNES_SRC} zip/zip.c tracing.c main.c)
target_include_directories(lakesnes_test PRIVATE snes zip .)
target_compile_definitions(lakesnes_test PRIVATE strcasecmp=_stricmp)
target_link_libraries(lakesnes_test PRIVATE SDL2::SDL2 SDL2::SDL2main)

if(WIN32)
    add_custom_command(TARGET lakesnes_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:lakesnes_test>
    )
endif()
