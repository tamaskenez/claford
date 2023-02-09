cmake_minimum_required(VERSION 3.20)
project(claford)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_REQUIRED 1)

find_package(fmt REQUIRED)
find_package(glog REQUIRED)
find_package(libfswatch REQUIRED)
find_package(nowide REQUIRED)

set(WARNINGS_AS_ERRORS ON CACHE BOOL "Warnings as errors" FORCE)
include(cmake/cpp_warnings.cmake)
add_compile_options(${PROJECT_WARNING_FLAGS})

add_subdirectory(src)
