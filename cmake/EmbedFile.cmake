include_guard(GLOBAL)

include(CMakeParseArguments)

function(embed_file input_path)

    cmake_parse_arguments(PARSE_ARGV 1 EMBED_FILE
        ""
        "TARGET"
        ""
    )

    if(NOT EMBED_FILE_TARGET)
        message(FATAL_ERROR "Missing TARGET")
    endif()

    message(STATUS "input_path: ${input_path}")
    file(READ ${input_path} filedata HEX)
    string(LENGTH "${filedata}" hex_length)
    string(REGEX REPLACE "([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f])" "\\1\n" filedata ${filedata})
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
    math(EXPR size "${hex_length}/2")
    cmake_path(GET input_path FILENAME path_filename)
    string(MAKE_C_IDENTIFIER ${path_filename} var_name)
    set(path_h "${CMAKE_CURRENT_BINARY_DIR}/${path_filename}.h")
    set(path_cpp "${CMAKE_CURRENT_BINARY_DIR}/${path_filename}.cpp")
    file(WRITE "${path_h}"
        "#pragma once\n#include <span>\n#include <cstddef>\nextern const std::span<const unsigned char, ${size}> ${var_name};\n")
    file(WRITE "${path_cpp}"
        "#include \"${path_h}\"\nconst unsigned char ${var_name}_array[${size}] = {${filedata}};\n")
    file(APPEND "${path_cpp}"
        "const std::span<const unsigned char, ${size}> ${var_name}(${var_name}_array, ${size});\n")
    target_sources(${EMBED_FILE_TARGET} PRIVATE ${path_h} ${path_cpp})
    source_group(TREE ${CMAKE_CURRENT_BINARY_DIR} FILES ${path_h} ${path_cpp})

endfunction()