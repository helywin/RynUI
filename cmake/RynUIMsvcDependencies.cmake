# CMake can decode localized cl /showIncludes output using the ANSI code page
# even when cl writes UTF-8 to a UTF-8 console. A mismatched prefix silently
# leaves Ninja with zero header dependencies and permits stale object ABIs.
if(WIN32 AND MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    # chcp reports the input code page, which may differ from the output page.
    execute_process(COMMAND powershell -NoProfile -Command "[Console]::OutputEncoding.CodePage"
        OUTPUT_VARIABLE rynui_console_codepage
        OUTPUT_STRIP_TRAILING_WHITESPACE ENCODING UTF-8)
    if(rynui_console_codepage MATCHES "65001$")
        set(rynui_dependency_probe "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rynui-showincludes-probe")
        file(WRITE "${rynui_dependency_probe}.h" "#pragma once\n")
        file(WRITE "${rynui_dependency_probe}.cpp" "#include \"rynui-showincludes-probe.h\"\n")
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c
                "/Fo${rynui_dependency_probe}.obj" "${rynui_dependency_probe}.cpp"
            RESULT_VARIABLE rynui_dependency_probe_result
            OUTPUT_VARIABLE rynui_dependency_probe_output
            ERROR_VARIABLE rynui_dependency_probe_error
            ENCODING UTF-8)
        string(REGEX MATCH "(^|\n)([^\r\n]*: +)[^\r\n]*rynui-showincludes-probe[.]h"
            rynui_dependency_probe_match
            "${rynui_dependency_probe_output}\n${rynui_dependency_probe_error}")
        if(NOT rynui_dependency_probe_result EQUAL 0 OR NOT rynui_dependency_probe_match)
            message(FATAL_ERROR "Cannot determine the UTF-8 MSVC /showIncludes prefix")
        endif()
        set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
    endif()
endif()
