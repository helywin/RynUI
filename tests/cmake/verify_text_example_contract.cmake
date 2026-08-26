file(READ "${EXAMPLE_SOURCE}" example_source)

foreach(required IN ITEMS
        "ryn::String content = u8\""
        "Latin body text"
        "中文回退字体"
        "pixel_size,"
        "font_rasterizations="
        "replacement_count="
        "fallback_runs="
        "shape_count="
        "atlas_pages="
        "atlas_uploads="
        "instance_rebuilds="
        "buffer_uploads="
        "glyph_draws="
        "submits="
        "idle_waits="
        "exit_code=0")
    string(FIND "${example_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Text example is missing contract token: ${required}")
    endif()
endforeach()

string(FIND "${example_source}" "{1.0F, 1.0F, 1.0F, 0.65F}" semantic_color)
if(semantic_color EQUAL -1)
    message(FATAL_ERROR "Text example does not use Ant Design secondary semantic color")
endif()
