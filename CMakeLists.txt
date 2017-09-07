cmake_minimum_required(VERSION 3.5)
project(wawa)

message("--START BUILD... ")
set(CMAKE_CXX_STANDARD 11)

add_definitions("-g")

include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(
        ${PROJECT_SOURCE_DIR}/lib
)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

add_subdirectory(${PROJECT_SOURCE_DIR}/src)
