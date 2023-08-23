cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(fmerge)

# Define folders
set(EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin)

# Compiler settings
set(CMAKE_CXX_STANDARD 17)
set(warnings "-Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warnings}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_LEGACY_CYGWIN_WIN32 0)

# Binaries
file(GLOB_RECURSE SOURCES ${SRC_DIR}/*.cpp)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(${CMAKE_PROJECT_NAME} uuid)