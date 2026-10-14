cmake_minimum_required(VERSION 4.2.3)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(kaleidoscope)

file(GLOB SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)
