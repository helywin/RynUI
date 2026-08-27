file(READ "${EXAMPLE_SOURCE}" example_source)

foreach(required IN ITEMS
        "application.mount(ryn::Content"
        "ryn::Text("
        "ryn::TextTone::Primary"
        "ryn::TextTone::Secondary"
        "ryn::TextTone::Disabled"
        "Latin"
        "中文"
        "content.set("
        "tone.set("
        "width.set("
        "margin.set("
        "resize_window("
        "mount_runs="
        "prop_updates="
        "shape_count="
        "measure_count="
        "layout_count="
        "atlas_uploads="
        "instance_rebuilds="
        "glyph_draws="
        "submits="
        "idle_waits="
        "exit_code=0")
    string(FIND "${example_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Text example is missing contract token: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "TextRenderController"
        "TextState "
        "GlyphAtlas "
        "GlyphScene ")
    string(FIND "${example_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Text example directly constructs forbidden engine type: ${forbidden}")
    endif()
endforeach()
