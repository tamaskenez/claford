file(GLOB_RECURSE sources *.cpp *.h)
add_executable(claford ${sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${sources})

target_link_libraries(claford PRIVATE
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
