cmake_minimum_required(VERSION 3.23)
project(claford)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_REQUIRED 1)

find_package(Boost REQUIRED)
find_package(concurrentqueue REQUIRED)
find_package(fmt REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glog REQUIRED)
find_package(imgui REQUIRED)
find_package(libfswatch REQUIRED)
find_package(nowide REQUIRED)
find_package(tl-expected REQUIRED)
find_package(readerwriterqueue REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(WARNINGS_AS_ERRORS ON CACHE BOOL "Warnings as errors" FORCE)
include(cmake/cpp_warnings.cmake)
add_compile_options(${PROJECT_WARNING_FLAGS})

set(imgui_source ${PROJECT_SOURCE_DIR}/deps/imgui/s)
set(imgui_glfw_binding_sources
    ${imgui_source}/backends/imgui_impl_glfw.h
    ${imgui_source}/backends/imgui_impl_glfw.cpp
    ${imgui_source}/backends/imgui_impl_opengl3.h
    ${imgui_source}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui_glfw_binding STATIC ${imgui_glfw_binding_sources})
source_group(TREE ${imgui_source}/backends FILES ${imgui_glfw_binding_sources})
target_link_libraries(imgui_glfw_binding
    PRIVATE glfw
    PUBLIC imgui::imgui)
target_include_directories(imgui_glfw_binding PUBLIC
    ${imgui_source}/backends)

add_subdirectory(src)


