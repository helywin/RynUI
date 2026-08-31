cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS PUBLIC_UMBRELLA SURFACE_HEADER SURFACE_SOURCE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

file(READ "${PUBLIC_UMBRELLA}" public_umbrella)
file(READ "${SURFACE_HEADER}" surface_header)
file(READ "${SURFACE_SOURCE}" surface_source)

if(public_umbrella MATCHES "ReferenceSurface")
    message(FATAL_ERROR "Gallery ReferenceSurface leaked into rynui.hpp.")
endif()
foreach(required_text IN ITEMS
        "class ReferenceSurfaceProps"
        "ReferenceSurfaceContentSlot"
        "Prop<GallerySupportStatus>"
        "Prop<std::optional<ryn::Color>>"
        "Prop<bool> visible_"
        "ryn::LayoutStyle")
    if(NOT surface_header MATCHES "${required_text}")
        message(FATAL_ERROR
            "ReferenceSurface typed header contract is missing: ${required_text}")
    endif()
endforeach()
foreach(required_text IN ITEMS
        "create_surface"
        "focus_enabled = false"
        "apply_visible"
        "connect_layout_style"
        "build.mount_slot")
    if(NOT surface_source MATCHES "${required_text}")
        message(FATAL_ERROR
            "ReferenceSurface retained source contract is missing: ${required_text}")
    endif()
endforeach()
if(surface_header MATCHES "Modifier" OR surface_source MATCHES "Modifier"
        OR surface_header MATCHES "SDL" OR surface_source MATCHES "SDL"
        OR surface_source MATCHES "interactions_\\.create")
    message(FATAL_ERROR
        "ReferenceSurface introduced Modifier, SDL, or Interaction registration.")
endif()
