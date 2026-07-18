if(NOT DEFINED LAUNCHER OR NOT EXISTS "${LAUNCHER}")
    message(FATAL_ERROR "LAUNCHER must name start-openxr-retail-sidecar.ps1")
endif()

if(NOT DEFINED POWERSHELL OR NOT EXISTS "${POWERSHELL}")
    message(FATAL_ERROR "POWERSHELL must name powershell.exe")
endif()

if(NOT DEFINED RUN_ROOT)
    message(FATAL_ERROR "RUN_ROOT must name the launcher run directory")
endif()

file(GLOB before_runs LIST_DIRECTORIES true "${RUN_ROOT}/*")
list(SORT before_runs)

execute_process(
    COMMAND "${POWERSHELL}"
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${LAUNCHER}"
    RESULT_VARIABLE launch_result
    OUTPUT_VARIABLE launch_stdout
    ERROR_VARIABLE launch_stderr
    TIMEOUT 15
)

set(launch_output "${launch_stdout}\n${launch_stderr}")
set(expected_marker "All live OpenXR presentation is intentionally blocked.")

if(NOT launch_result EQUAL 1)
    message(FATAL_ERROR
        "Live launcher fuse returned ${launch_result}, expected 1. Output=${launch_output}")
endif()

string(FIND "${launch_output}" "${expected_marker}" marker_index)
if(marker_index EQUAL -1)
    message(FATAL_ERROR
        "Live launcher fuse hid its product-level diagnostic. Output=${launch_output}")
endif()

foreach(forbidden_text
    "Cannot bind argument to parameter 'LiteralPath'"
    "because it is an empty string"
)
    string(FIND "${launch_output}" "${forbidden_text}" forbidden_index)
    if(NOT forbidden_index EQUAL -1)
        message(FATAL_ERROR
            "Live launcher fuse was masked by '${forbidden_text}'. Output=${launch_output}")
    endif()
endforeach()

file(GLOB after_runs LIST_DIRECTORIES true "${RUN_ROOT}/*")
list(SORT after_runs)
if(NOT "${before_runs}" STREQUAL "${after_runs}")
    message(FATAL_ERROR
        "Refused live launch changed the run-directory set. Before=${before_runs} After=${after_runs}")
endif()

message(STATUS "OpenXR launcher live fuse PASS (clear diagnostic; no run-directory side effect)")
