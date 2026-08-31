cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS DEFINITION_SOURCE RUNTIME_SOURCE EXAMPLES_CMAKE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
    if(NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR "${required_variable} does not exist")
    endif()
endforeach()

file(READ "${DEFINITION_SOURCE}" definition_source)
foreach(required IN ITEMS
        "#include <ryn/rynui.hpp>"
        "ryn::Theme("
        "ryn::Flex("
        "ryn::Space("
        "ryn::Text("
        "ryn::Button("
        "ryn::ant_design_default_shadows()"
        "gallery.theme.default"
        "gallery.theme.dark"
        "gallery.theme.compact"
        "gallery.theme.nested-brand"
        "ant.map.colorPrimary"
        "ant.map.colorSuccess"
        "ant.map.colorWarning"
        "ant.map.colorError"
        "ant.map.colorInfo"
        "ant.map.fontSizeSM"
        "ant.map.fontSizeLG"
        "ant.map.controlHeightSM"
        "ant.map.controlHeightLG"
        "ant.map.borderRadiusSM"
        "ant.map.borderRadiusLG"
        "ant.alias.boxShadowTertiary"
        "ant.alias.boxShadowSecondary"
        "ant.alias.boxShadow"
        "ant.component.Button.defaultShadow"
        "ant.component.Button.primaryShadow"
        "ant.component.Button.dangerShadow"
        "ant.alias.boxShadowDrawerLeft"
        "ant.alias.boxShadowDrawerRight"
        "ant.alias.boxShadowDrawerUp"
        "ant.alias.boxShadowDrawerDown"
        "ant.alias.boxShadowPopoverArrow"
        "ant.alias.dropShadowPopover"
        "ant.alias.boxShadowCard"
        "ant.alias.boxShadowTabsOverflowLeft"
        "ant.alias.boxShadowTabsOverflowRight"
        "ant.alias.boxShadowTabsOverflowTop"
        "ant.alias.boxShadowTabsOverflowBottom"
        "gallery.state.focus-visible"
        "gallery.state.hover"
        "gallery.state.active"
        "gallery.state.disabled"
        "gallery.state.loading")
    string(FIND "${definition_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Token Gallery public DSL is missing: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "component/button_component.hpp"
        "runtime/"
        "renderer/"
        "SDL_"
        "ButtonComponentHost"
        "RoundedEffectStore")
    string(FIND "${definition_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Token Gallery public DSL references an internal implementation: ${forbidden}")
    endif()
endforeach()

file(READ "${RUNTIME_SOURCE}" runtime_source)
foreach(required IN ITEMS
        "catalog_hash="
        "gpu_driver="
        "shader_format="
        "display_scale="
        "window_system="
        "stable_test_ids="
        "snapshot_identity="
        "content_runs="
        "theme_content_runs="
        "document_sections="
        "component_entries="
        "reference_surfaces="
        "reference_content_runs="
        "live_samples="
        "reference_interactions=0"
        "theme_updates="
        "brand_updates="
        "motion_updates="
        "viewport_updates="
        "pointer_input_events="
        "pointer_routes="
        "hover_enters="
        "hover_leaves="
        "captures_started="
        "captures_released="
        "keyboard_events="
        "focus_traversals="
        "focus_changes="
        "keyboard_activations="
        "effect_layers="
        "effect_uploads="
        "effect_draws="
        "submits="
        "idle_waits="
        "animation_frames="
        "idle_after_animation="
        "automated_input_events="
        "--animation-acceptance"
        "--motion-disabled"
        "--reduced-motion"
        "are mutually exclusive"
        "motion_mode="
        "--acceptance-scale must be 1.0, 1.25, 1.5, or 2.0"
        "exit_code=0")
    string(FIND "${runtime_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Token Gallery runtime telemetry is missing: ${required}")
    endif()
endforeach()

file(READ "${EXAMPLES_CMAKE}" examples_cmake)
foreach(required IN ITEMS
        "add_executable(\n    rynui_token_gallery"
        "rounded_effect.vertex.dxil"
        "rounded_effect.vertex.spv"
        "RYNUI_TOKEN_CATALOG_HASH")
    string(FIND "${examples_cmake}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Token Gallery build contract is missing: ${required}")
    endif()
endforeach()
