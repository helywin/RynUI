cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EXAMPLE_SOURCE OR "${EXAMPLE_SOURCE}" STREQUAL "")
    message(FATAL_ERROR "EXAMPLE_SOURCE is required")
endif()
if(NOT EXISTS "${EXAMPLE_SOURCE}")
    message(FATAL_ERROR "Button example source is missing")
endif()

file(READ "${EXAMPLE_SOURCE}" example_source)
foreach(required IN ITEMS
        "application.mount(ryn::Content"
        "ryn::Button("
        "ryn::ButtonType::Default"
        "ryn::ButtonType::Primary"
        "ryn::ButtonType::Danger"
        "ryn::ControlSize::Small"
        "ryn::ControlSize::Middle"
        "ryn::ControlSize::Large"
        ".disabled(true)"
        ".loading(true)"
        ".type(reactive_type)"
        ".size(reactive_size)"
        ".disabled(reactive_disabled)"
        ".loading(reactive_loading)"
        ".onClick("
        "Latin"
        "中文"
        "observed_clicks"
        "input_events="
        "hit_test_queries="
        "routes_dispatched="
        "captures_started="
        "captures_released="
        "focus_changes="
        "clicks="
        "layout_passes="
        "scene_rebuilds="
        "quad_uploads="
        "glyph_uploads="
        "effect_uploads="
        "quad_draws="
        "glyph_draws="
        "effect_draws="
        "submits="
        "idle_waits="
        "exit_code=0")
    string(FIND "${example_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Button example is missing contract token: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "InteractionRegistry "
        "FocusManager "
        "ButtonSceneService "
        "TextState "
        "SDL_Event")
    string(FIND "${example_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Button example directly constructs forbidden internal type: ${forbidden}")
    endif()
endforeach()
