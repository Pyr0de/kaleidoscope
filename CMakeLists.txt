cmake_minimum_required(VERSION 4.2.3)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(kaleidoscope)

file(GLOB SOURCES "src/*.cpp")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_DIR: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(${PROJECT_NAME} ${SOURCES})

if(TARGET LLVM)
    target_link_libraries(${PROJECT_NAME} PRIVATE LLVM)     # libLLVM-22.so
else()
    llvm_map_components_to_libnames(LLVM_LIBS core orcjit native support)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LLVM_LIBS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LLVM_LIBS})

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp"
)

target_sources(${PROJECT_NAME} PRIVATE ${LIB_SOURCES})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(${PROJECT_NAME} PRIVATE
        "-Wl,--export-dynamic"
    )
endif()
