cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED EVIDENCE OR NOT EXISTS "${EVIDENCE}")
    message(FATAL_ERROR "EVIDENCE must name an existing Gallery evidence file")
endif()
if(NOT DEFINED EXPECTED_SCOPE OR "${EXPECTED_SCOPE}" STREQUAL "")
    message(FATAL_ERROR "EXPECTED_SCOPE is required")
endif()

file(READ "${EVIDENCE}" evidence)
foreach(required IN ITEMS
        "scope=${EXPECTED_SCOPE}"
        "result=passed"
        "catalog_version=6.5.0"
        "catalog_commit=740ad964dc2397f33e40944367b0536a7314cc32"
        "catalog_hash=4f1cd61bfa697e88b4c41407d5d3a0141d02560acfa606d89e98d5a2dc7aa642"
        "category_counts=4,7,7,18,20,11,5"
        "support_counts=implemented:0,partial:5,planned:67,web-only:0,deprecated:0,out-of-scope:0"
        "source_introduction=https://ant.design/docs/spec/introduce"
        "source_components=https://ant.design/components/overview"
        "viewport_wide="
        "viewport_narrow="
        "section_count=6"
        "component_count=72"
        "reachable_components=72"
        "button_state_contract=passed"
        "reference_interactions=0"
        "scene_rebuilds="
        "quad_uploads="
        "glyph_uploads="
        "effect_uploads="
        "submits="
        "idle_waits="
        "preset="
        "generator=Ninja Multi-Config"
        "dependency_mode=BUNDLED"
        "compiler="
        "actual_platform="
        "gpu_driver="
        "shader_format="
        "font_source="
        "ctest_result=164/164"
        "dependency_lock=passed"
        "license_inventory=passed"
        "offline_runtime=passed"
        "python_cache=clean"
        "screenshot_path=")
    string(FIND "${evidence}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Gallery evidence is missing: ${required}")
    endif()
endforeach()

if(evidence MATCHES "planning-only|result=planned|implemented=false")
    message(FATAL_ERROR "Planning-only Gallery evidence cannot pass")
endif()

if(evidence MATCHES "actual_platform=windows")
    if(NOT evidence MATCHES "compiler=MSVC" OR NOT evidence MATCHES "preset=windows-msvc")
        message(FATAL_ERROR "Windows Gallery evidence has a cross-platform identity")
    endif()
elseif(evidence MATCHES "actual_platform=linux")
    if(evidence MATCHES "compiler=MSVC" OR evidence MATCHES "preset=windows-msvc")
        message(FATAL_ERROR "Linux Gallery evidence has a cross-platform identity")
    endif()
else()
    message(FATAL_ERROR "Gallery evidence actual_platform is unsupported")
endif()

if(evidence MATCHES "screenshot_path=not-captured"
        AND NOT evidence MATCHES "screenshot_reason=user-requested-direct-testing")
    message(FATAL_ERROR "Missing Gallery screenshot requires an explicit reason")
endif()
