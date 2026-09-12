cmake_minimum_required(VERSION 3.25)
foreach(contract_case RANGE 0 6)
    set(case_binary_dir "${TEST_BINARY_DIR}/${contract_case}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        -S "${TEST_SOURCE_DIR}/tests/cmake/input-public-contract"
        -B "${case_binary_dir}" -G "${TEST_GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
        "-DRYNUI_ROOT=${TEST_SOURCE_DIR}" "-DINPUT_CONTRACT_CASE=${contract_case}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Input contract configure failed: ${out}\n${err}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${case_binary_dir}"
        --config "${TEST_CONFIGURATION}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(contract_case EQUAL 0 AND NOT result EQUAL 0)
        message(FATAL_ERROR "Input positive header-isolation control failed: ${out}\n${err}")
    elseif(NOT contract_case EQUAL 0 AND result EQUAL 0)
        message(FATAL_ERROR "Input forbidden case ${contract_case} unexpectedly compiled")
    endif()
endforeach()
file(READ "${TEST_SOURCE_DIR}/include/ryn/input.hpp" source)
foreach(forbidden IN ITEMS "SDL" "runtime/" "graphics/" "renderer/" "font/" "NodeId" "ComponentId" "InteractionId" "Modifier" "Shader")
    string(FIND "${source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Input public header leaked ${forbidden}")
    endif()
endforeach()
