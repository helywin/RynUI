include_guard(GLOBAL)

function(rynui_add_quad_shader_target shadercross_command shadercross_dependency)
    set(shader_source "${PROJECT_SOURCE_DIR}/shaders/quad.hlsl")
    set(shader_output_directory "${CMAKE_BINARY_DIR}/generated/shaders")

    set(shader_outputs)
    foreach(shader_stage IN ITEMS vertex fragment)
        if(shader_stage STREQUAL "vertex")
            set(entry_point VSMain)
            set(stage_argument vertex)
        else()
            set(entry_point PSMain)
            set(stage_argument fragment)
        endif()

        foreach(shader_format IN ITEMS dxil spirv)
            if(shader_format STREQUAL "dxil")
                set(destination_argument DXIL)
                set(output_extension dxil)
            else()
                set(destination_argument SPIRV)
                set(output_extension spv)
            endif()

            set(shader_output
                "${shader_output_directory}/quad.${shader_stage}.${output_extension}")
            add_custom_command(
                OUTPUT "${shader_output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${shader_output_directory}"
                COMMAND "${shadercross_command}"
                    "${shader_source}"
                    -s HLSL
                    -d "${destination_argument}"
                    -t "${stage_argument}"
                    -e "${entry_point}"
                    -o "${shader_output}"
                DEPENDS "${shader_source}" ${shadercross_dependency}
                COMMENT
                    "Compiling quad ${shader_stage} shader to ${destination_argument}"
                VERBATIM
            )
            list(APPEND shader_outputs "${shader_output}")
        endforeach()
    endforeach()

    add_custom_target(rynui_shaders ALL DEPENDS ${shader_outputs})
    set(RYNUI_GENERATED_SHADER_DIRECTORY "${shader_output_directory}"
        CACHE INTERNAL "RynUI generated shader directory")
endfunction()
