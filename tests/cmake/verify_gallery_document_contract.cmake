cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS DEFINITION_SOURCE MODEL_SOURCE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
    if(NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR "${required_variable} does not exist")
    endif()
endforeach()

file(READ "${DEFINITION_SOURCE}" definition_source)
foreach(required IN ITEMS
        "source_section(state)"
        "gallery_document_sections()[1]"
        "design_values(state)"
        "foundation_tokens(state)"
        "component_overview(state)"
        "add_live_samples(state)"
        "ReferenceSurface("
        "ryn::Space("
        "GallerySupportFilter::all"
        "ant_design_reference_categories()"
        "ant_design_reference_entries()"
        "supported_scope"
        "missing_scope"
        "evidence_identifiers"
        "source_path")
    string(FIND "${definition_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gallery reference document is missing: ${required}")
    endif()
endforeach()

string(REGEX MATCHALL "ryn::Button\\(" button_calls "${definition_source}")
list(LENGTH button_calls button_call_count)
if(NOT button_call_count EQUAL 1)
    message(FATAL_ERROR
        "Gallery catalog must not simulate unsupported components with Button; found ${button_call_count} Button call sites")
endif()

foreach(forbidden IN ITEMS
        "primary_surface("
        "shadow_surface("
        "radius_surface("
        "import React"
        "export default"
        "<img"
        "https://*.png"
        "https://*.jpg"
        "https://*.svg")
    string(FIND "${definition_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Gallery reference document contains forbidden copied or simulated content: ${forbidden}")
    endif()
endforeach()

file(READ "${MODEL_SOURCE}" model_source)
foreach(required IN ITEMS
        "GalleryDocumentSectionKind::header_source"
        "GalleryDocumentSectionKind::introduction"
        "GalleryDocumentSectionKind::design_values"
        "GalleryDocumentSectionKind::foundation_tokens"
        "GalleryDocumentSectionKind::component_overview"
        "GalleryDocumentSectionKind::live_samples"
        "Natural"
        "Certain"
        "Meaningful"
        "Growing")
    string(FIND "${model_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gallery document model is missing: ${required}")
    endif()
endforeach()
