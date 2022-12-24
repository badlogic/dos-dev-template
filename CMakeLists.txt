cmake_minimum_required(VERSION 3.7)
project(dos-app)

set(CMAKE_GENERATOR Ninja)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
add_compile_options(-Wall -std=gnu99)

file(GLOB sources CONFIGURE_DEPENDS "src/*.c" "src/*.cpp" "src/*.h" )

add_executable(main ${sources})

add_custom_target(assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_BINARY_DIR}/assets
)
add_dependencies(main assets)

add_executable(keyboard examples/keyboard.c)