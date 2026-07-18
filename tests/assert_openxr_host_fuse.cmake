if(NOT DEFINED HOST_EXE OR NOT EXISTS "${HOST_EXE}")
    message(FATAL_ERROR "Missing host executable: ${HOST_EXE}")
endif()

execute_process(
    COMMAND "${HOST_EXE}" 1
    RESULT_VARIABLE live_result
    OUTPUT_VARIABLE live_stdout
    ERROR_VARIABLE live_stderr
    TIMEOUT 5
)
if(NOT live_result EQUAL 26)
    message(FATAL_ERROR
        "Live host fuse returned ${live_result}, expected 26. stdout=${live_stdout} stderr=${live_stderr}")
endif()
string(CONCAT live_output "${live_stdout}" "${live_stderr}")
if(NOT live_output MATCHES "No OpenXR loader or headset session was touched")
    message(FATAL_ERROR "Live host fuse did not emit its pre-loader proof marker: ${live_output}")
endif()

foreach(bad_argument IN ITEMS "" "-1" "0" "garbage" "7201")
    if(bad_argument STREQUAL "")
        execute_process(
            COMMAND "${HOST_EXE}"
            RESULT_VARIABLE parse_result
            OUTPUT_VARIABLE parse_stdout
            ERROR_VARIABLE parse_stderr
            TIMEOUT 5
        )
    else()
        execute_process(
            COMMAND "${HOST_EXE}" "${bad_argument}"
            RESULT_VARIABLE parse_result
            OUTPUT_VARIABLE parse_stdout
            ERROR_VARIABLE parse_stderr
            TIMEOUT 5
        )
    endif()
    if(NOT parse_result EQUAL 24)
        message(FATAL_ERROR
            "Malformed frame argument '${bad_argument}' returned ${parse_result}, expected 24. stdout=${parse_stdout} stderr=${parse_stderr}")
    endif()
endforeach()

message(STATUS "OpenXR host live fuse PASS (exit 26 before loader; malformed inputs exit 24)")
