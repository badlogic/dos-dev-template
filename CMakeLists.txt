cmake_minimum_required(VERSION 3.7)
project(dos-app)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

file(GLOB sources CONFIGURE_DEPENDS "src/*.c" "src/*.cpp" "src/*.h" )

add_executable(main ${sources})