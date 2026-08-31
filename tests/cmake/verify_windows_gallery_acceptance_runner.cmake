cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RUNNER OR NOT EXISTS "${RUNNER}")
    message(FATAL_ERROR "RUNNER must name the Windows Gallery acceptance script")
endif()
file(READ "${RUNNER}" runner)
foreach(required IN ITEMS
        "ValidateSet('Debug', 'Release')"
        "windows-msvc"
        "rynui_token_gallery.exe"
        "@(1.0, 1.25, 1.5, 2.0)"
        "--acceptance-scale="
        "category navigation"
        "support filter"
        "wheel"
        "Tab/Enter"
        "Default/Primary/Danger"
        "Tee-Object"
        "LASTEXITCODE")
    string(FIND "${runner}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Windows Gallery acceptance runner is missing: ${required}")
    endif()
endforeach()
foreach(forbidden IN ITEMS "capture-window" "screenshot" "tasks.md")
    string(FIND "${runner}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Windows Gallery runner contains forbidden automation: ${forbidden}")
    endif()
endforeach()
