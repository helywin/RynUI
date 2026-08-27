include_guard(GLOBAL)

function(rynui_add_shader_targets shadercross_command shadercross_dependency)
    set(shader_output_directory "${CMAKE_BINARY_DIR}/generated/shaders")

    set(shader_outputs)
    foreach(shader_name IN ITEMS quad glyph rounded_effect)
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

    foreach(shader_name IN ITEMS glyph rounded_effect)
        foreach(shader_stage IN ITEMS vertex fragment)
            if(shader_stage STREQUAL "vertex")
                set(entry_point VSMain)
            else()
                set(entry_point PSMain)
            endif()
            set(shader_reflection
                "${shader_output_directory}/${shader_name}.${shader_stage}.json")
            add_custom_command(
                OUTPUT "${shader_reflection}"
                COMMAND "${shadercross_command}"
                    "${shader_output_directory}/${shader_name}.${shader_stage}.spv"
                    -s SPIRV
                    -d JSON
                    -t "${shader_stage}"
                    -e "${entry_point}"
                    -o "${shader_reflection}"
                DEPENDS
                    "${shader_output_directory}/${shader_name}.${shader_stage}.spv"
                    ${shadercross_dependency}
                COMMENT "Reflecting ${shader_name} ${shader_stage} shader resources"
                VERBATIM
            )
            list(APPEND shader_outputs "${shader_reflection}")
        endforeach()
    endforeach()

    add_custom_target(rynui_shaders ALL DEPENDS ${shader_outputs})
    set(RYNUI_GENERATED_SHADER_DIRECTORY "${shader_output_directory}"
        CACHE INTERNAL "RynUI generated shader directory")
endfunction()

function(rynui_deploy_target_shaders target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot deploy shaders for unknown target: ${target}")
    endif()
    if(NOT ARGN)
        message(FATAL_ERROR "No shaders were requested for target: ${target}")
    endif()

    set(shader_sources)
    foreach(shader_file IN LISTS ARGN)
        list(APPEND shader_sources
            "${RYNUI_GENERATED_SHADER_DIRECTORY}/${shader_file}")
    endforeach()

    set(deployment_target "${target}_shader_assets")
    add_custom_target("${deployment_target}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${target}>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${shader_sources}
            "$<TARGET_FILE_DIR:${target}>/shaders"
        COMMENT "Deploying shaders for ${target}"
        VERBATIM
    )
    add_dependencies("${deployment_target}" rynui_shaders)
    add_dependencies("${target}" "${deployment_target}")
endfunction()
