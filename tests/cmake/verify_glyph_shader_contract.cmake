cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS SHADER_SOURCE SHADER_DIRECTORY)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(READ "${SHADER_SOURCE}" glyph_source)
foreach(binding IN ITEMS
        "SamplerState AtlasSampler : register(s0, space2)"
        "Texture2D<float> AtlasTexture : register(t0, space2)"
        "input.color.a * input.opacity * coverage")
    string(FIND "${glyph_source}" "${binding}" binding_offset)
    if(binding_offset EQUAL -1)
        message(FATAL_ERROR "Glyph shader is missing contract: ${binding}")
    endif()
endforeach()

file(READ "${SHADER_DIRECTORY}/glyph.vertex.json" vertex_reflection)
string(JSON vertex_input_count ERROR_VARIABLE vertex_error
    LENGTH "${vertex_reflection}" inputs)
if(vertex_error OR NOT vertex_input_count EQUAL 5)
    message(FATAL_ERROR
        "Glyph vertex reflection must contain five instance attributes: ${vertex_error}")
endif()
foreach(attribute_index RANGE 0 4)
    string(JSON attribute_location ERROR_VARIABLE location_error
        GET "${vertex_reflection}" inputs ${attribute_index} location)
    string(JSON attribute_type ERROR_VARIABLE type_error
        GET "${vertex_reflection}" inputs ${attribute_index} type)
    if(location_error OR type_error
            OR NOT attribute_location EQUAL attribute_index
            OR NOT attribute_type STREQUAL "float4")
        message(FATAL_ERROR
            "Glyph vertex attribute ${attribute_index} reflection is invalid")
    endif()
endforeach()

file(READ "${SHADER_DIRECTORY}/glyph.fragment.json" fragment_reflection)
string(JSON sampler_count ERROR_VARIABLE sampler_error
    GET "${fragment_reflection}" samplers)
string(JSON storage_texture_count ERROR_VARIABLE storage_texture_error
    GET "${fragment_reflection}" storage_textures)
string(JSON storage_buffer_count ERROR_VARIABLE storage_buffer_error
    GET "${fragment_reflection}" storage_buffers)
string(JSON uniform_buffer_count ERROR_VARIABLE uniform_buffer_error
    GET "${fragment_reflection}" uniform_buffers)
if(sampler_error OR storage_texture_error OR storage_buffer_error OR uniform_buffer_error
        OR NOT sampler_count EQUAL 1
        OR NOT storage_texture_count EQUAL 0
        OR NOT storage_buffer_count EQUAL 0
        OR NOT uniform_buffer_count EQUAL 0)
    message(FATAL_ERROR
        "Glyph fragment reflection does not expose exactly one sampled atlas")
endif()
