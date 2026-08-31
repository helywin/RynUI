cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS PUBLIC_UMBRELLA VIEWPORT_HEADER VIEWPORT_SOURCE)
    if(NOT DEFINED ${required_variable} OR NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR "${required_variable} is required and must exist")
    endif()
endforeach()

file(READ "${PUBLIC_UMBRELLA}" public_umbrella)
if(public_umbrella MATCHES "gallery_document_viewport")
    message(FATAL_ERROR "GalleryDocumentViewport leaked into the stable public API")
endif()

file(READ "${VIEWPORT_HEADER}" viewport_header)
foreach(required IN ITEMS
        "class GalleryDocumentViewport"
        "GalleryDocumentAnchorId"
        "GalleryDocumentViewportDiagnostics"
        "capture_resize_anchor"
        "restore_resize_anchor"
        "apply_subtree_translation")
    if(NOT viewport_header MATCHES "${required}")
        message(FATAL_ERROR "Gallery viewport contract is missing ${required}")
    endif()
endforeach()
foreach(forbidden IN ITEMS "SDL" "Modifier" "InteractionId")
    if(viewport_header MATCHES "${forbidden}")
        message(FATAL_ERROR "Gallery viewport leaked forbidden dependency ${forbidden}")
    endif()
endforeach()

file(READ "${VIEWPORT_SOURCE}" viewport_source)
foreach(required IN ITEMS
        "maximum_offset"
        "replace_anchors"
        "anchor_generation_"
        "translation_passes"
        "NodePropertyWriter"
        "translate_subtree")
    if(NOT viewport_source MATCHES "${required}")
        message(FATAL_ERROR "Gallery viewport implementation is missing ${required}")
    endif()
endforeach()
