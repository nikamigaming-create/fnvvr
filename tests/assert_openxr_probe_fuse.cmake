if(NOT DEFINED PROBE_EXE OR NOT EXISTS "${PROBE_EXE}")
    message(FATAL_ERROR "Missing OpenXR probe executable: ${PROBE_EXE}")
endif()

if(NOT DEFINED PROBE_SOURCE OR NOT EXISTS "${PROBE_SOURCE}")
    message(FATAL_ERROR "Missing OpenXR probe source: ${PROBE_SOURCE}")
endif()

if(NOT DEFINED PROBE_SCRIPT OR NOT EXISTS "${PROBE_SCRIPT}")
    message(FATAL_ERROR "Missing OpenXR probe script: ${PROBE_SCRIPT}")
endif()

if(NOT DEFINED POWERSHELL OR NOT EXISTS "${POWERSHELL}")
    message(FATAL_ERROR "Missing powershell.exe: ${POWERSHELL}")
endif()

if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR must name an isolated probe-fuse test directory")
endif()

# Refuse to execute an old/unfused binary during the TDD red phase. The runtime
# assertion below runs only after the source proves that the fail-closed branch
# precedes the loader call.
file(READ "${PROBE_SOURCE}" probe_source)
set(source_fuse "constexpr bool OpenXrDiagnosticRuntimeProofComplete = false;")
set(source_branch "if (!OpenXrDiagnosticRuntimeProofComplete)")
set(loader_call "HMODULE loader = loadOpenXrLoader();")
string(FIND "${probe_source}" "${source_fuse}" source_fuse_index)
string(FIND "${probe_source}" "${source_branch}" source_branch_index)
string(FIND "${probe_source}" "${loader_call}" loader_call_index)
if(source_fuse_index EQUAL -1
    OR source_branch_index EQUAL -1
    OR loader_call_index EQUAL -1
    OR source_fuse_index GREATER source_branch_index
    OR source_branch_index GREATER loader_call_index)
    message(FATAL_ERROR
        "OpenXR probe source fuse is absent or does not execute before the loader call")
endif()

file(READ "${PROBE_SCRIPT}" probe_script)
set(script_fuse "$OpenXrDiagnosticRuntimeProofComplete = $false")
set(script_branch "if (-not $OpenXrDiagnosticRuntimeProofComplete)")
set(script_build "cmake -S $Root -B $BuildDir")
string(FIND "${probe_script}" "${script_fuse}" script_fuse_index)
string(FIND "${probe_script}" "${script_branch}" script_branch_index)
string(FIND "${probe_script}" "${script_build}" script_build_index)
if(script_fuse_index EQUAL -1
    OR script_branch_index EQUAL -1
    OR script_build_index EQUAL -1
    OR script_fuse_index GREATER script_branch_index
    OR script_branch_index GREATER script_build_index)
    message(FATAL_ERROR
        "OpenXR probe script fuse is absent or does not execute before configure/build")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(step_file "${WORK_DIR}/local/openxr-probe-step.txt")
file(REMOVE "${step_file}")

execute_process(
    COMMAND "${PROBE_EXE}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_stdout
    ERROR_VARIABLE probe_stderr
    TIMEOUT 5
)
if(NOT probe_result EQUAL 27)
    message(FATAL_ERROR
        "OpenXR probe fuse returned ${probe_result}, expected 27. stdout=${probe_stdout} stderr=${probe_stderr}")
endif()
string(CONCAT probe_output "${probe_stdout}" "${probe_stderr}")
if(NOT probe_output MATCHES "No OpenXR loader or runtime was touched")
    message(FATAL_ERROR
        "OpenXR probe fuse did not emit its pre-loader proof marker: ${probe_output}")
endif()
if(EXISTS "${step_file}")
    message(FATAL_ERROR
        "OpenXR probe entered its loader/trace path despite the source fuse: ${step_file}")
endif()

execute_process(
    COMMAND "${POWERSHELL}"
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${PROBE_SCRIPT}"
    RESULT_VARIABLE script_result
    OUTPUT_VARIABLE script_stdout
    ERROR_VARIABLE script_stderr
    TIMEOUT 5
)
if(NOT script_result EQUAL 1)
    message(FATAL_ERROR
        "OpenXR probe script fuse returned ${script_result}, expected 1. stdout=${script_stdout} stderr=${script_stderr}")
endif()
string(CONCAT script_output "${script_stdout}" "${script_stderr}")
if(NOT script_output MATCHES "blocked before configure, build, or OpenXR runtime touch")
    message(FATAL_ERROR
        "OpenXR probe script fuse hid its pre-build diagnostic: ${script_output}")
endif()

message(STATUS "OpenXR diagnostic fuse PASS (probe exit 27 before loader; script exits before build)")
