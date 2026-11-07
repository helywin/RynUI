cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS SHADER_SOURCE SHADER_LOCK SHADER_DIRECTORY PYTHON_EXECUTABLE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

get_filename_component(shader_root "${SHADER_SOURCE}" DIRECTORY)
get_filename_component(project_root "${shader_root}" DIRECTORY)
execute_process(COMMAND "${PYTHON_EXECUTABLE}" -B "${project_root}/tools/effect_shader_closure.py"
    --root "${project_root}" --lock "${SHADER_LOCK}"
    RESULT_VARIABLE lock_result OUTPUT_VARIABLE lock_output ERROR_VARIABLE lock_error)
if(NOT lock_result EQUAL 0)
    message(FATAL_ERROR "Effect shader closure validation failed: ${lock_error}")
endif()
file(READ "${SHADER_SOURCE}" shader_source)
file(READ "${shader_root}/rounded_effect_coverage.hlsli" coverage_source)
string(APPEND shader_source "\n${coverage_source}")

foreach(contract IN ITEMS
        "nointerpolation float4 shapeRect"
        "RoundedRectDistance"
        "ErfApprox"
        "GaussianEdge"
        "CircularArcDistance"
        "RoundedPathNearest"
        "DashedOutlineCoverage"
        "TooltipDistance"
        "samplePosition < input.clipBounds.xy"
        "samplePosition >= input.clipBounds.zw"
        "discard;"
        "float surfaceDistance"
        "float surfaceCoverage"
        "float antialiasWidth"
        "float outlineOffset"
        "input.color.a * input.materialParams.x * coverage")
    string(FIND "${shader_source}" "${contract}" contract_offset)
    if(contract_offset EQUAL -1)
        message(FATAL_ERROR
            "Rounded-effect shader is missing contract: ${contract}")
    endif()
endforeach()

foreach(shader_stage IN ITEMS vertex fragment)
    set(reflection_path
        "${SHADER_DIRECTORY}/rounded_effect.${shader_stage}.json")
    file(READ "${reflection_path}" reflection)
    foreach(resource IN ITEMS samplers storage_textures storage_buffers uniform_buffers)
        string(JSON resource_count ERROR_VARIABLE resource_error
            GET "${reflection}" ${resource})
        if(resource_error OR NOT resource_count EQUAL 0)
            message(FATAL_ERROR
                "Rounded-effect ${shader_stage} reflection unexpectedly uses ${resource}")
        endif()
    endforeach()
endforeach()

file(READ "${SHADER_DIRECTORY}/rounded_effect.vertex.json" vertex_reflection)
string(JSON vertex_input_count ERROR_VARIABLE vertex_error
    LENGTH "${vertex_reflection}" inputs)
if(vertex_error OR NOT vertex_input_count EQUAL 7)
    message(FATAL_ERROR
        "Rounded-effect vertex reflection must contain seven float4 attributes")
endif()
foreach(attribute_index RANGE 0 6)
    string(JSON attribute_location ERROR_VARIABLE location_error
        GET "${vertex_reflection}" inputs ${attribute_index} location)
    string(JSON attribute_type ERROR_VARIABLE type_error
        GET "${vertex_reflection}" inputs ${attribute_index} type)
    if(location_error OR type_error
            OR NOT attribute_location EQUAL attribute_index
            OR NOT attribute_type STREQUAL "float4")
        message(FATAL_ERROR
            "Rounded-effect vertex attribute ${attribute_index} reflection is invalid")
    endif()
endforeach()

foreach(artifact IN ITEMS
        rounded_effect.vertex.dxil
        rounded_effect.fragment.dxil
        rounded_effect.vertex.spv
        rounded_effect.fragment.spv
        rounded_effect.vertex.json
        rounded_effect.fragment.json)
    set(artifact_path "${SHADER_DIRECTORY}/${artifact}")
    if(NOT EXISTS "${artifact_path}")
        message(FATAL_ERROR "Rounded-effect shader artifact is missing: ${artifact}")
    endif()
    file(TIMESTAMP "${artifact_path}" artifact_time "%s")
    foreach(source IN ITEMS "${SHADER_SOURCE}" "${shader_root}/rounded_effect_coverage.hlsli" "${SHADER_LOCK}")
        file(TIMESTAMP "${source}" source_time "%s")
        if(artifact_time LESS source_time)
            message(FATAL_ERROR "Rounded-effect shader artifact is stale: ${artifact} vs ${source}")
        endif()
    endforeach()
endforeach()
