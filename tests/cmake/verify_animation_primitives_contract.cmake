cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RYNUI_SOURCE_DIR OR "${RYNUI_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "RYNUI_SOURCE_DIR is required")
endif()

set(public_aggregate "${RYNUI_SOURCE_DIR}/include/ryn/rynui.hpp")
set(time_header "${RYNUI_SOURCE_DIR}/src/animation/time.hpp")
set(value_header "${RYNUI_SOURCE_DIR}/src/animation/value.hpp")
set(easing_header "${RYNUI_SOURCE_DIR}/src/animation/easing.hpp")
set(easing_source "${RYNUI_SOURCE_DIR}/src/animation/easing.cpp")
set(runtime_header "${RYNUI_SOURCE_DIR}/src/animation/runtime.hpp")
set(motion_policy_header
    "${RYNUI_SOURCE_DIR}/src/animation/motion_policy.hpp")
set(deadline_header
    "${RYNUI_SOURCE_DIR}/src/runtime/animation_frame_deadline.hpp")
set(submitter_header
    "${RYNUI_SOURCE_DIR}/src/runtime/animation_frame_submitter.hpp")
set(source_lock
    "${RYNUI_SOURCE_DIR}/design-tokens/ant-design/6.5.0/sources.lock.yaml")

foreach(required IN ITEMS
        "${public_aggregate}"
        "${time_header}"
        "${value_header}"
        "${easing_header}"
        "${easing_source}"
        "${runtime_header}"
        "${motion_policy_header}"
        "${deadline_header}"
        "${submitter_header}"
        "${source_lock}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Animation primitive contract input is missing: ${required}")
    endif()
endforeach()

file(READ "${public_aggregate}" public_source)
foreach(forbidden IN ITEMS
        "animation/"
        "AnimationClock"
        "AnimationRuntime"
        "AnimationValue"
        "AntEasingPreset"
        "MotionPolicy"
        "MotionTokenSet"
        "MotionPreference"
        "FrameDeadlineSource"
        "AnimationFrameSubmitter")
    string(FIND "${public_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Public aggregate exposed internal animation primitive: ${forbidden}")
    endif()
endforeach()

foreach(header IN ITEMS
        "${time_header}"
        "${value_header}"
        "${easing_header}"
        "${runtime_header}"
        "${motion_policy_header}"
        "${deadline_header}"
        "${submitter_header}")
    file(READ "${header}" header_source)
    foreach(forbidden IN ITEMS
            "SDL"
            "component/"
            "graphics/"
            "platform/"
            "renderer/")
        string(FIND "${header_source}" "${forbidden}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "Animation primitive header leaked forbidden dependency: ${forbidden}")
        endif()
    endforeach()
endforeach()

file(READ "${source_lock}" lock_source)
foreach(required_lock IN ITEMS
        "\"commit\": \"740ad964dc2397f33e40944367b0536a7314cc32\""
        "\"tag\": \"6.5.0\""
        "components/theme/interface/seeds.ts"
        "components/theme/themes/seed.ts")
    string(FIND "${lock_source}" "${required_lock}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Ant Design motion source lock drifted: ${required_lock}")
    endif()
endforeach()

file(READ "${easing_source}" easing_source_contents)
foreach(required_curve IN ITEMS
        "{0.08F, 0.82F, 0.17F, 1.0F}"
        "{0.78F, 0.14F, 0.15F, 0.86F}"
        "{0.215F, 0.61F, 0.355F, 1.0F}"
        "{0.645F, 0.045F, 0.355F, 1.0F}"
        "{0.12F, 0.4F, 0.29F, 1.46F}"
        "{0.71F, -0.46F, 0.88F, 0.6F}"
        "{0.755F, 0.05F, 0.855F, 0.06F}"
        "{0.23F, 1.0F, 0.32F, 1.0F}")
    string(FIND "${easing_source_contents}" "${required_curve}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Ant Design motion easing source contract drifted: ${required_curve}")
    endif()
endforeach()
