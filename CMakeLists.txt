cmake_minimum_required(VERSION 3.20)
project(claford)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_REQUIRED 1)

find_package(fmt REQUIRED)
find_package(glog REQUIRED)
find_package(libfswatch REQUIRED)

add_subdirectory(src)
