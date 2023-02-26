cmake_minimum_required(VERSION 3.20)
project(claford)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_REQUIRED 1)

find_package(fmt REQUIRED)
find_package(glog REQUIRED)
find_package(libfswatch REQUIRED)
find_package(nowide REQUIRED)
find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)

set(WARNINGS_AS_ERRORS ON CACHE BOOL "Warnings as errors" FORCE)
include(cmake/cpp_warnings.cmake)
add_compile_options(${PROJECT_WARNING_FLAGS})

set(imgui_glfw_binding_sources
    ${imgui_INCLUDE_DIRS}/../res/bindings/imgui_impl_glfw.h
    ${imgui_INCLUDE_DIRS}/../res/bindings/imgui_impl_glfw.cpp
    ${imgui_INCLUDE_DIRS}/../res/bindings/imgui_impl_opengl3.h
    ${imgui_INCLUDE_DIRS}/../res/bindings/imgui_impl_opengl3.cpp
)
add_library(imgui_glfw_binding STATIC ${imgui_glfw_binding_sources})
source_group(TREE ${imgui_INCLUDE_DIRS}/../res/bindings FILES ${imgui_glfw_binding_sources})
target_link_libraries(imgui_glfw_binding
    PRIVATE glfw
    PUBLIC imgui::imgui)
target_include_directories(imgui_glfw_binding PUBLIC
    ${imgui_INCLUDE_DIRS}/../res/bindings)

add_subdirectory(src)


add_executable(glfw_main glfw_main.cpp)
target_link_libraries(glfw_main PRIVATE
    glfw
    imgui_glfw_binding
)
