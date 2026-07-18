if(NOT DEFINED HOST_EXE OR NOT EXISTS "${HOST_EXE}")
    message(FATAL_ERROR "Missing host executable: ${HOST_EXE}")
endif()

if(NOT DEFINED HOST_SOURCE OR NOT EXISTS "${HOST_SOURCE}")
    message(FATAL_ERROR "Missing host source: ${HOST_SOURCE}")
endif()

if(NOT DEFINED HOST_AUTHORITY OR NOT EXISTS "${HOST_AUTHORITY}")
    message(FATAL_ERROR "Missing host initialization authority: ${HOST_AUTHORITY}")
endif()

file(READ "${HOST_SOURCE}" host_source)
file(READ "${HOST_AUTHORITY}" host_authority)

foreach(required_trial_text IN ITEMS
        "#include \"fnvxr_stereo_visual_trial.h\""
        "stereoVisualTrialDecision.bindsStereoVisuals()"
        "const bool presentedBinocularWorld = productionBinocularWorld"
        "stereoVisualTrialFullProductAccepted"
        "controllerMutationAuthorized"
        "trackedWeaponAuthorized")
    string(FIND "${host_source}" "${required_trial_text}" trial_at)
    if(trial_at EQUAL -1)
        message(FATAL_ERROR
            "Host lost its explicit visual-trial/full-product boundary: ${required_trial_text}")
    endif()
endforeach()

foreach(retired_fuse IN ITEMS
        "OpenXrLiveRuntimeProofComplete"
        "ProductPresentationControllerIntegrated")
    string(FIND "${host_source}" "${retired_fuse}" retired_in_host)
    string(FIND "${host_authority}" "${retired_fuse}" retired_in_authority)
    if(NOT retired_in_host EQUAL -1 OR NOT retired_in_authority EQUAL -1)
        message(FATAL_ERROR
            "Retired boolean host fuse remains in the product initialization path: ${retired_fuse}")
    endif()
endforeach()

foreach(required_authority_text IN ITEMS
        "MaximumProductHostFrames = 7200u"
        "struct ProductOpenXrInitializationProof"
        "assessProductOpenXrInitialization("
        "CompiledProductOpenXrInitializationProof"
        "CompiledProductOpenXrInitializationAuthorization"
        "static_assert(CompiledProductOpenXrInitializationAuthorization.authorized())")
    string(FIND "${host_authority}" "${required_authority_text}" authority_at)
    if(authority_at EQUAL -1)
        message(FATAL_ERROR
            "Host initialization authority is missing: ${required_authority_text}")
    endif()
endforeach()

string(FIND "${host_source}"
    "#include \"fnvxr_openxr_live_authority.h\""
    authority_include_at)
string(FIND "${host_source}"
    "const uint64_t targetFrames = static_cast<uint64_t>(parsed);"
    parsed_bound_at)
string(FIND "${host_source}"
    "fnvxr::host::CompiledProductOpenXrInitializationAuthorization"
    authorization_value_at)
string(FIND "${host_source}"
    "if (!openXrInitializationAuthorization.authorized())"
    authorization_branch_at)
string(FIND "${host_source}" "OpenXr xr {};" loader_object_at)

if(authority_include_at EQUAL -1
    OR parsed_bound_at EQUAL -1
    OR authorization_value_at EQUAL -1
    OR authorization_branch_at EQUAL -1
    OR loader_object_at EQUAL -1)
    message(FATAL_ERROR
        "Host source is missing the bounded pre-loader initialization authorization path")
endif()
if(NOT parsed_bound_at LESS authorization_value_at
    OR NOT authorization_value_at LESS authorization_branch_at
    OR NOT authorization_branch_at LESS loader_object_at)
    message(FATAL_ERROR
        "Host must parse the finite bound and enforce product initialization authority before constructing OpenXR")
endif()

# A valid invocation is intentionally not executed by this test because it is
# now authorized to touch the configured headset runtime. Parser failures must
# still terminate before that boundary.
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

message(STATUS
    "OpenXR host bounded initialization authority PASS (valid live execution not invoked)")
