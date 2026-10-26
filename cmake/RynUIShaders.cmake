include_guard(GLOBAL)

function(rynui_add_shader_targets shadercross_command shadercross_dependency)
    set(shader_output_directory "${CMAKE_BINARY_DIR}/generated/shaders")

    set(shader_outputs)
    set(shader_filenames)
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
                list(APPEND shader_filenames "${shader_name}.${shader_stage}.${output_extension}")
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
    set(RYNUI_BUILTIN_SHADER_FILENAMES "${shader_filenames}" CACHE INTERNAL "RynUI built-in shader files")
endfunction()

function(rynui_embed_target_shaders target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot embed shaders for unknown target: ${target}")
    endif()
    get_target_property(target_type "${target}" TYPE)
    if(NOT target_type STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "Built-in shader resources must be linked into the final executable")
    endif()
    get_target_property(already_embedded "${target}" RYNUI_EMBEDDED_SHADERS)
    if(already_embedded)
        message(FATAL_ERROR "Shaders are already embedded for ${target}")
    endif()
    set_target_properties("${target}" PROPERTIES RYNUI_EMBEDDED_SHADERS TRUE)
    set(shader_inputs)
    foreach(filename IN LISTS RYNUI_BUILTIN_SHADER_FILENAMES)
        list(APPEND shader_inputs "${RYNUI_GENERATED_SHADER_DIRECTORY}/${filename}")
    endforeach()
    set(resource_directory "${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders/${target}")
    if(WIN32)
        set(resource_file "${resource_directory}/shaders.rc")
        configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/renderer/sdl/shaders.rc.in"
            "${resource_file}" @ONLY)
        set_source_files_properties("${resource_file}" PROPERTIES OBJECT_DEPENDS "${shader_inputs}")
        target_sources("${target}" PRIVATE "${resource_file}")
    else()
        set(resource_file "${resource_directory}/shaders.cpp")
        set(resource_stamp "${resource_directory}/shaders.stamp")
        set(generator "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/EmbedBuiltinShaders.cmake")
        add_custom_command(
            OUTPUT "${resource_stamp}"
            BYPRODUCTS "${resource_file}"
            COMMAND "${CMAKE_COMMAND}" "-DSHADER_DIRECTORY=${RYNUI_GENERATED_SHADER_DIRECTORY}"
                "-DSHADER_FILES=${RYNUI_BUILTIN_SHADER_FILENAMES}" "-DOUTPUT=${resource_file}" -P "${generator}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${resource_stamp}"
            DEPENDS ${shader_inputs} "${generator}"
            VERBATIM
        )
        set_source_files_properties("${resource_stamp}" PROPERTIES HEADER_FILE_ONLY TRUE)
        target_sources("${target}" PRIVATE "${resource_file}" "${resource_stamp}")
    endif()
    add_dependencies("${target}" rynui_shaders)
endfunction()
