include_guard(GLOBAL)

function(rynui_add_shader_targets shadercross_command shadercross_dependency)
    set(shader_output_directory "${CMAKE_BINARY_DIR}/generated/shaders")

    set(shader_outputs)
    foreach(shader_name IN ITEMS quad glyph)
        set(shader_source "${PROJECT_SOURCE_DIR}/shaders/${shader_name}.hlsl")
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
                    "${shader_output_directory}/${shader_name}.${shader_stage}.${output_extension}")
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
                        "Compiling ${shader_name} ${shader_stage} shader to ${destination_argument}"
                    VERBATIM
                )
                list(APPEND shader_outputs "${shader_output}")
            endforeach()
        endforeach()
    endforeach()

    foreach(shader_stage IN ITEMS vertex fragment)
        if(shader_stage STREQUAL "vertex")
            set(entry_point VSMain)
        else()
            set(entry_point PSMain)
        endif()
        set(glyph_reflection
            "${shader_output_directory}/glyph.${shader_stage}.json")
        add_custom_command(
            OUTPUT "${glyph_reflection}"
            COMMAND "${shadercross_command}"
                "${shader_output_directory}/glyph.${shader_stage}.spv"
                -s SPIRV
                -d JSON
                -t "${shader_stage}"
                -e "${entry_point}"
                -o "${glyph_reflection}"
            DEPENDS
                "${shader_output_directory}/glyph.${shader_stage}.spv"
                ${shadercross_dependency}
            COMMENT "Reflecting glyph ${shader_stage} shader resources"
            VERBATIM
        )
        list(APPEND shader_outputs "${glyph_reflection}")
    endforeach()

    add_custom_target(rynui_shaders ALL DEPENDS ${shader_outputs})
    set(RYNUI_GENERATED_SHADER_DIRECTORY "${shader_output_directory}"
        CACHE INTERNAL "RynUI generated shader directory")
endfunction()
