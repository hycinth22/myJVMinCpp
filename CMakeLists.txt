cmake_minimum_required(VERSION 3.10)
project(myJVMinCpp)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(fmt REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/lib)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(myJVMinCpp
    src/main.cpp
    src/classFileParser.cpp
    src/ClassLoader.cpp
    src/constantPool.cpp
    src/interpreter.cpp
    src/NativeMethods.cpp
)

if(TARGET fmt::fmt)
    target_link_libraries(myJVMinCpp PRIVATE fmt::fmt)
else()
    target_link_libraries(myJVMinCpp PRIVATE fmt)
endif()