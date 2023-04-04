include(EmbedFile)

file(GLOB_RECURSE sources *.cpp *.h)

add_executable(claford ${sources})

embed_file("${imgui_source}/misc/fonts/Karla-Regular.ttf"
    TARGET claford
)
embed_file("${PROJECT_SOURCE_DIR}/assets/Inter-Regular.ttf"
    TARGET claford
)

target_include_directories(claford PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${sources})

target_link_libraries(claford PRIVATE
    concurrentqueue::concurrentqueue
    fmt::fmt
    glfw
    glog::glog
    imgui_glfw_binding
    libfswatch::libfswatch
    nowide::nowide
)

install(TARGETS claford
    RUNTIME DESTINATION bin
)
