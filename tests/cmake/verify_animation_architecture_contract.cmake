cmake_minimum_required(VERSION 3.25)

foreach(variable IN ITEMS ARCHITECTURE_FILE README_FILE AGENTS_FILE)
    if(NOT DEFINED ${variable} OR NOT EXISTS "${${variable}}")
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

file(READ "${ARCHITECTURE_FILE}" architecture)
foreach(required IN ITEMS
        "Motion 与基础动画 Runtime"
        "internal `AnimationRuntime`"
        "整数 microseconds"
        "slot + generation identity"
        "可选 deadline source"
        "blocking one-shot"
        "effective `MotionPolicy`"
        "motion disabled 或 reduced"
        "Button 是首个 consumer"
        "focus-visible outline 保持 `0s`"
        "固定八段 rounded-quad topology"
        "后续 OpenSpec change")
    string(FIND "${architecture}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Animation architecture baseline is missing: ${required}")
    endif()
endforeach()

file(READ "${README_FILE}" readme)
foreach(internal IN ITEMS
        "AnimationTargetId"
        "spinner_phase"
        "lifecycle_created="
        "FrameDeadlineSource")
    string(FIND "${readme}" "${internal}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "README contains internal animation architecture or evidence: ${internal}")
    endif()
endforeach()

file(READ "${AGENTS_FILE}" agents)
foreach(workflow IN ITEMS
        "每完成一个可独立验证的小阶段就创建一次 Git commit"
        "不主动 push、创建 PR 或 archive change"
        "平台无关的实现")
    string(FIND "${agents}" "${workflow}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "AGENTS workflow baseline is missing: ${workflow}")
    endif()
endforeach()
