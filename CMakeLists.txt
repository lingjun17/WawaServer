cmake_minimum_required(VERSION 3.5)
project(wawa)

message("--START BUILD... ")
set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra -Wno-write-strings -Wno-missing-field-initializers -g")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Wall -Wextra -Wno-write-strings -Wno-missing-field-initializers")
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

SET (LIBS pthread)

#find VideoCore libraries
#FIND_PACKAGE(VideoCore REQUIRED)
#IF (VIDEOCORE_FOUND)
#    INCLUDE_DIRECTORIES(${VideoCore_INCLUDE_DIRS})
#    SET (LIBS ${LIBS} ${VideoCore_LIBRARIES})
#ENDIF()

add_definitions("-g")

include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(
        ${PROJECT_SOURCE_DIR}/lib
        /usr/local/lib
)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

add_subdirectory(${PROJECT_SOURCE_DIR}/src)
