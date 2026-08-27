cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS
        GENERATED_SHADER_DIRECTORY
        MINIMAL_SHADER_DIRECTORY
        TEXT_SHADER_DIRECTORY
        BUTTON_SHADER_DIRECTORY
        LAYOUT_SHADER_DIRECTORY)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

function(assert_shader_matches shader_file deployed_directory)
    set(generated_shader "${GENERATED_SHADER_DIRECTORY}/${shader_file}")
    set(deployed_shader "${deployed_directory}/${shader_file}")

    if(NOT EXISTS "${generated_shader}")
        message(FATAL_ERROR "Generated shader is missing: ${generated_shader}")
    endif()
    if(NOT EXISTS "${deployed_shader}")
        message(FATAL_ERROR "Deployed shader is missing: ${deployed_shader}")
    endif()

    file(SHA256 "${generated_shader}" generated_sha256)
    file(SHA256 "${deployed_shader}" deployed_sha256)
    if(NOT generated_sha256 STREQUAL deployed_sha256)
        message(FATAL_ERROR
            "Deployed shader is stale: ${deployed_shader}\n"
            "  generated: ${generated_sha256}\n"
            "  deployed:  ${deployed_sha256}")
    endif()
endfunction()

foreach(shader_file IN ITEMS
        quad.vertex.dxil
        quad.vertex.spv
        quad.fragment.dxil
        quad.fragment.spv)
    assert_shader_matches("${shader_file}" "${MINIMAL_SHADER_DIRECTORY}")
    assert_shader_matches("${shader_file}" "${TEXT_SHADER_DIRECTORY}")
    assert_shader_matches("${shader_file}" "${BUTTON_SHADER_DIRECTORY}")
    assert_shader_matches("${shader_file}" "${LAYOUT_SHADER_DIRECTORY}")
endforeach()

foreach(shader_file IN ITEMS
        glyph.vertex.dxil
        glyph.vertex.spv
        glyph.fragment.dxil
        glyph.fragment.spv)
    assert_shader_matches("${shader_file}" "${TEXT_SHADER_DIRECTORY}")
    assert_shader_matches("${shader_file}" "${BUTTON_SHADER_DIRECTORY}")
    assert_shader_matches("${shader_file}" "${LAYOUT_SHADER_DIRECTORY}")
endforeach()
