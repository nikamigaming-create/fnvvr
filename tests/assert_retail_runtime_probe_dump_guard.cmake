if(NOT DEFINED PROBE OR NOT EXISTS "${PROBE}")
    message(FATAL_ERROR "PROBE must name the built fnvxr_retail_runtime_probe")
endif()
if(NOT DEFINED EXISTING_DIR OR NOT IS_DIRECTORY "${EXISTING_DIR}")
    message(FATAL_ERROR "EXISTING_DIR must name an existing directory")
endif()

execute_process(
    COMMAND "${PROBE}" --help
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
if(NOT help_result EQUAL 0)
    message(FATAL_ERROR "probe --help failed: ${help_error}")
endif()
if(NOT help_output MATCHES "--dump-dir <path>")
    message(FATAL_ERROR "probe --help does not document --dump-dir")
endif()

if(NOT help_output MATCHES "--dump-code <preferred-address> <bytes>")
    message(FATAL_ERROR "probe --help does not document --dump-code")
endif()

execute_process(
    COMMAND "${PROBE}" --dump-dir "${EXISTING_DIR}"
    RESULT_VARIABLE guard_result
    OUTPUT_VARIABLE guard_output
    ERROR_VARIABLE guard_error
)
if(guard_result EQUAL 0)
    message(FATAL_ERROR "probe accepted an existing dump directory")
endif()
set(guard_combined "${guard_output}\n${guard_error}")
if(NOT guard_combined MATCHES
        "dump directory already exists; refusing to overwrite")
    message(FATAL_ERROR
        "probe did not fail at the no-overwrite guard: ${guard_combined}")
endif()
