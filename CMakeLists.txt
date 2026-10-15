cmake_minimum_required(VERSION 4.2.3)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(kaleidoscope)

file(GLOB SOURCES "src/*.cpp")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_DIR: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
    Support
    Core
    Analysis
    Passes
    ScalarOpts
    TransformUtils
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LLVM_LIBS})
