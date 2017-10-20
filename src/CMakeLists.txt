cmake_minimum_required(VERSION 3.5)
project(wawa)

set(CMAKE_CXX_STANDARD 11)
#aux_source_directory(${PROJECT_SOURCE_DIR} SRC_LIST)
add_executable(wawa server.cpp protocol.cpp handler.cpp)
#add_executable(wawa server.cpp protocol.cpp handler.cpp)
add_executable(remote remote.cpp protocol.cpp)
#add_executable(raspi raspi.cpp)
#add_executable(client client.cpp protocol.cpp)

target_link_libraries(wawa -lev)

#target_link_libraries(raspi ${LIBS} libcamkit.so)
