cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SHADER_DIRECTORY OR SHADER_DIRECTORY STREQUAL "")
    message(FATAL_ERROR "SHADER_DIRECTORY is required.")
endif()

foreach(shader_name IN ITEMS
        quad.vertex.dxil
        quad.fragment.dxil
        quad.vertex.spv
        quad.fragment.spv)
    set(shader_path "${SHADER_DIRECTORY}/${shader_name}")
    if(NOT EXISTS "${shader_path}")
        message(FATAL_ERROR "Generated shader is missing: ${shader_path}")
    endif()

    file(SIZE "${shader_path}" shader_size)
    if(shader_size LESS 16)
        message(FATAL_ERROR "Generated shader is unexpectedly small: ${shader_path}")
    endif()

    file(READ "${shader_path}" shader_magic OFFSET 0 LIMIT 4 HEX)
    if(shader_name MATCHES "[.]dxil$" AND NOT shader_magic STREQUAL "44584243")
        message(FATAL_ERROR
            "DXIL container has invalid magic ${shader_magic}: ${shader_path}")
    elseif(shader_name MATCHES "[.]spv$" AND NOT shader_magic STREQUAL "03022307")
        message(FATAL_ERROR
            "SPIR-V module has invalid magic ${shader_magic}: ${shader_path}")
    endif()
endforeach()
