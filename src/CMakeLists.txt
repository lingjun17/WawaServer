cmake_minimum_required(VERSION 3.5)
project(wawa)

set(CMAKE_CXX_STANDARD 11)
#aux_source_directory(${PROJECT_SOURCE_DIR} SRC_LIST)
#set(SOURCE_FILES ${SRC_LIST} io.cpp)
add_executable(wawa server.cpp protocol.cpp handler.cpp)
add_executable(client client.cpp protocol.cpp)
add_executable(remote remote.cpp protocol.cpp)


target_link_libraries(wawa -lev)