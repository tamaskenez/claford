cmake_minimum_required(VERSION 3.20)
project(imgui)

if(NOT IMGUI_DIR)
    message(FATAL_ERROR "IMGUI_DIR must be set to the imgui root directory")
endif()

set(CMAKE_DEBUG_POSTFIX _d)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED)

find_package(freetype REQUIRED CONFIG)

set(ROOT ${IMGUI_DIR})

add_library(imgui STATIC
    ${ROOT}/imgui.cpp
    ${ROOT}/imgui.h
    ${ROOT}/imgui_demo.cpp
    ${ROOT}/imgui_draw.cpp
    ${ROOT}/imgui_internal.h
    ${ROOT}/imgui_tables.cpp
    ${ROOT}/imgui_widgets.cpp
    ${ROOT}/imstb_rectpack.h
    ${ROOT}/imstb_textedit.h
    ${ROOT}/imstb_truetype.h
    ${ROOT}/misc/freetype/imgui_freetype.cpp
    ${ROOT}/misc/freetype/imgui_freetype.h
)
target_include_directories(imgui PUBLIC
    $<BUILD_INTERFACE:${ROOT}>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(imgui PUBLIC
    IMGUI_ENABLE_FREETYPE
)

target_link_libraries(imgui PRIVATE freetype)
    
install(TARGETS imgui
    EXPORT imgui-targets DESTINATION lib)
install(EXPORT imgui-targets
    FILE imgui-config.cmake
    NAMESPACE imgui::
    DESTINATION lib/cmake/imgui)
install(FILES ${ROOT}/imgui.h ${ROOT}/imconfig.h
    DESTINATION include)
