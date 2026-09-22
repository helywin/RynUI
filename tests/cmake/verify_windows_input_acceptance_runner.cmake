cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED RUNNER OR NOT EXISTS "${RUNNER}")
    message(FATAL_ERROR "RUNNER must name the Windows Input acceptance script")
endif()

file(READ "${RUNNER}" runner)
foreach(required IN ITEMS
        "ValidateSet('Debug', 'Release')"
        "windows-msvc"
        "rynui_token_gallery.exe"
        "@(1.0, 1.25, 1.5, 2.0)"
        "--input-acceptance"
        "--acceptance-scale="
        "windows-input"
        "input_latin=passed"
        "input_selection=passed"
        "input_clipboard=passed"
        "input_undo=passed"
        "input_redo=passed"
        "input_theme_status=passed"
        "input_caret_idle=passed"
        "Tee-Object"
        "LASTEXITCODE")
    string(FIND "${runner}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Windows Input acceptance runner is missing: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS "capture-window" "screenshot" "tasks.md" "manual_ime_visual_result=passed")
    string(FIND "${runner}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Windows Input runner contains forbidden automation: ${forbidden}")
    endif()
endforeach()
