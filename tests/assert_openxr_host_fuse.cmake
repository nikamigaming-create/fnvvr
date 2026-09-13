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
        "const bool stereoVisualTrialActive = productionBinocularWorld;"
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

foreach(required_source_identity_text IN ITEMS
        "stereoGamePublicationGeneration"
        "lastSharedStereoPublicationGeneration"
        "sourceStereoPublicationGeneration"
        "nonzeroSharedGenerationAdvanced(")
    string(FIND "${host_source}" "${required_source_identity_text}" source_identity_at)
    if(source_identity_at EQUAL -1)
        message(FATAL_ERROR
            "Host lost the 64-bit shared-stereo publication identity: ${required_source_identity_text}")
    endif()
endforeach()

foreach(required_cpu_transition_text IN ITEMS
        "#include \"fnvxr_cpu_engine_presentation.h\""
        "readSharedD3D9MonoUiFrame("
        "uploadCpuEngineUiTexture("
        "flatUiFrameEligible("
        "assessBinocularWorldFrame("
        "cpuUiBoundaryTransactionId"
        "#include \"fnvxr_game_plane_surface.h\""
        "game_plane_surface::select("
        "game_plane_surface::permitsSourceSurround("
        "Kind::CurvedGameplay"
        "D3D9StereoFrameHostReaderMutexName"
        "&& !productionCpuEngineStereo")
    string(FIND "${host_source}" "${required_cpu_transition_text}" cpu_transition_at)
    if(cpu_transition_at EQUAL -1)
        message(FATAL_ERROR
            "Host lost the CPU UI/world transition boundary: ${required_cpu_transition_text}")
    endif()
endforeach()

string(FIND "${host_source}" "Stereo_HostReader_v7" retired_cpu_mutex_at)
if(NOT retired_cpu_mutex_at EQUAL -1)
    message(FATAL_ERROR
        "Host still leases the retired v7 stereo reader mutex")
endif()

foreach(required_product_kernel_text IN ITEMS
        "#include \"fnvxr_eye_readback_policy.h\""
        "#include \"fnvxr_openxr_spatial_adapter.h\""
        "const fnvxr::host::PresentationTransportProof presentationTransport"
        "exactWristPoseReady"
        "productDecision.spatialRigMayRender"
        "productDecision.wristScreenMayRender"
        "runtimeConfig.get().performance.eyeTransport"
        "fnvxr::gpu::color_v5::FrameChannel::Ui"
        "auxiliaryPipBoyUiRequested"
        "pipBoySourceRuntimeEligible"
        "rightHandGripCalibrationHistory.find("
        "leftPipBoyScreenCalibrationHistory.find("
        "candidateWristActivation.advance("
        "&& pipBoySpatialScreenVisible)"
        "runtimeDidNotRequestRender"
        "cadenceTracker.abortFrame("
        "deriveCadencePerformance("
        "resolvePresentationAuthority("
        "assessContentReadiness("
        "screenContentReady("
        "fnvxrFrameJoinSummary"
        "retainedUiResourceGeneration"
        "retainedUiRendererProducerEpoch"
        "const bool runtimeDidNotRequestRender = !runtimeShouldRender;"
        "presentedExactFrameJoin"
        "FNVXR_VERIFY_EYE_PIXELS")
    string(FIND "${host_source}" "${required_product_kernel_text}" product_kernel_at)
    if(product_kernel_at EQUAL -1)
        message(FATAL_ERROR
            "Host lost a production kernel boundary: ${required_product_kernel_text}")
    endif()
endforeach()

foreach(retired_product_path IN ITEMS
        "prepareProductUiWindowFallback("
        "hostUiValidatedRuntime"
        "envEnabled(\"FNVXR_ENABLE_ENGINE_CENTER_STEREO\""
        "FNVXR_RENDER_OUTPUT_PROOF"
        "renderProof.valid = true;")
    string(FIND "${host_source}" "${retired_product_path}" retired_product_at)
    if(NOT retired_product_at EQUAL -1)
        message(FATAL_ERROR
            "Host still contains a retired proof-fabricating product path: ${retired_product_path}")
    endif()
endforeach()

foreach(retired_inline_policy IN ITEMS
        "finalObservedFps"
        "finalRequestedFrames"
        "retainedPipBoyContentFresh"
        "poseHistoryExactJoins"
        "(!productionGpuColorV5")
    string(FIND "${host_source}" "${retired_inline_policy}" retired_inline_at)
    if(NOT retired_inline_at EQUAL -1)
        message(FATAL_ERROR
            "Host revived duplicated product policy: ${retired_inline_policy}")
    endif()
endforeach()

string(FIND "${host_source}" "candidateWristActivation.advance(" wrist_candidate_at)
string(FIND "${host_source}" "const XrResult endResult = xr.endFrame" end_frame_at)
string(FIND "${host_source}" "wristActivation = candidateWristActivation;" wrist_commit_at)
if(wrist_candidate_at EQUAL -1
    OR end_frame_at EQUAL -1
    OR wrist_commit_at EQUAL -1
    OR NOT wrist_candidate_at LESS end_frame_at
    OR NOT end_frame_at LESS wrist_commit_at)
    message(FATAL_ERROR
        "Exact-source wrist state must be computed before xrEndFrame and committed only after success")
endif()

# A submitted device must remain focusable before its menu has opened. The
# UI pixel gate belongs to screen rendering, while input retains only the
# exact source-matched device pose after successful binocular submission.
string(SUBSTRING "${host_source}" ${end_frame_at} -1 submitted_frame_source)
foreach(required_wrist_commit IN ITEMS
        "endResult == XR_SUCCESS"
        "&& submitProjectionLayer"
        "&& candidateWristEvaluated"
        "&& wristSourcePoseMatched"
        "&& spatialPropsSourcePoseMatched"
        "submittedWristPlane = candidateWristPlane;")
    string(FIND "${submitted_frame_source}" "${required_wrist_commit}" wrist_guard_at)
    if(wrist_guard_at EQUAL -1)
        message(FATAL_ERROR "Submitted wrist interaction lost its guard: ${required_wrist_commit}")
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
        "MaximumProductHostFrames = 2000000000u"
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
foreach(bad_argument IN ITEMS "" "-1" "0" "garbage" "2000000001")
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
