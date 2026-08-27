cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EXAMPLE_SOURCE OR "${EXAMPLE_SOURCE}" STREQUAL "")
    message(FATAL_ERROR "EXAMPLE_SOURCE is required")
endif()
if(NOT EXISTS "${EXAMPLE_SOURCE}")
    message(FATAL_ERROR "Layout example source is missing")
endif()

file(READ "${EXAMPLE_SOURCE}" example_source)
foreach(required IN ITEMS
        "ryn::Content{"
        "ryn::Flex("
        "ryn::Space("
        "ryn::Text("
        "ryn::Button("
        ".vertical(vertical)"
        ".wrap(wrap)"
        ".justify(justify)"
        ".align(align)"
        ".gap(gap)"
        ".flex_grow(grow)"
        ".flex_shrink(1.0F)"
        ".order(order)"
        ".onClick(toggle_layout)"
        "Latin + 中文"
        "content_runs"
        "prop_updates")
    string(FIND "${example_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Layout example is missing contract token: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "ComponentHost"
        "NodeStore"
        "NodeId"
        "LayoutEngine"
        "SDL_Event"
        "PointerInputEvent"
        "KeyboardInputEvent")
    string(FIND "${example_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Layout example directly depends on forbidden internal type: ${forbidden}")
    endif()
endforeach()
