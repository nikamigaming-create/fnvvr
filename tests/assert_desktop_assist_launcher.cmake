if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
    message(FATAL_ERROR "SOURCE_ROOT must name the repository root")
endif()

set(script "${SOURCE_ROOT}/scripts/start-desktop-assist.ps1")
if(NOT EXISTS "${script}")
    message(FATAL_ERROR "Desktop-assist launcher is missing: ${script}")
endif()

file(READ "${script}" content)

foreach(required IN ITEMS
    "fnvxr-desktop-assist-launch-v1"
    "[switch]$ValidateOnly"
    "[switch]$RunAcceptanceTrial"
    "[switch]$AutomateAcceptance"
    "[switch]$CollectEngineEvidence"
    "[switch]$ApproveStageAndLaunch"
    "Desktop-assist staging and launch require -ApproveStageAndLaunch."
    "if (-not $ValidateOnly -and -not $ApproveStageAndLaunch)"
    "-ValidateOnly is read-only and cannot be combined with -ApproveStageAndLaunch."
    "-CollectEngineEvidence requires a live approved desktop-assist session and cannot be combined with -ValidateOnly."
    "-AutomateAcceptance requires -RunAcceptanceTrial so its recovery load and two Escape taps are evidence-bound."
    "Get-DesktopAssistStagePlan"
    "Get-DesktopAssistBuildPlan"
    "Invoke-DesktopAssistBuildPlan"
    "Desktop-assist build failed"
    "older DLL merely because a previous product build happened to exist"
    "x86/d3d9.dll"
    "x86/nvse_fnvxr.dll"
    "Desktop-assist stage plan must contain exactly the D3D9 proxy and NVSE plugin."
    "FNVXR_RUN_PROFILE = \"desktop-assist\""
    "FNVXR_DESKTOP_ASSIST_UI_CAPTURE = \"1\""
    "FNVXR_CAMERA_APPLY_TRANSLATION = \"0\""
    "FNVXR_CAMERA_WRITE_WORLD = \"0\""
    "FNVXR_CAMERA_UPDATE_TRANSFORM = \"0\""
    "FNVXR_ENABLE_ENGINE_CENTER_STEREO = \"0\""
    "FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = \"0\""
    "FNVXR_DISABLE_STEREO_WORLD = \"1\""
    "liveActionsTaken = $false"
    "Install-FnvxrProductArtifactSet"
    "Get-FnvxrProductLoadedModuleCensus"
    "Restore-FnvxrProductArtifactSet -Records $staged"
    "$manifest.cleanup.stagedArtifactsRestored = $true"
    "--require-desktop-assist-ready"
    "--require-desktop-assist"
    "--require-ui-quad-transition"
    "fnvxr_retail_runtime_probe.exe"
    "fnvxr_command.exe"
    "Invoke-FnvxrProductReadOnlyRetailRuntimeProbe"
    "desktop-assist-engine-evidence-captured"
    "this is an observation, not a stereo authorization"
    "$manifest.acceptance.passed"
    "Get-DesktopAssistAcceptanceReportEvidence"
    "bodyPositionUnits"
    "desktopAssistHeadBodyDecoupled"
    "headTranslationBodyDecoupled"
    "$manifest.acceptance.evidence.complete -eq $true"
    "FNVXR_DESKTOP_ASSIST_AUTOMATION = \"1\""
    "load FNVXR_HostExitRecovery"
    "SendEscapeToProcess"
    "exact Fallout foreground check followed by two Escape taps"
    "$manifest.automation.passed"
    "desktop-assist-acceptance-passed"
    "current-phase local-camera, body-root rotation/position, and UI-transition evidence")
    string(FIND "${content}" "${required}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Desktop-assist launcher lost required contract: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "FNVXR_HOST_MODE"
    "fnvxr_openxr_pose_host"
    "openxr_loader"
    "dinput8.dll"
    "xinput1_3.dll"
    "FNVXR_ENABLE_ENGINE_CENTER_STEREO = \"1\""
    "FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = \"1\""
    "FNVXR_CAMERA_APPLY_TRANSLATION = \"1\""
    "FNVXR_CAMERA_WRITE_WORLD = \"1\""
    "FNVXR_CAMERA_UPDATE_TRANSFORM = \"1\"")
    string(FIND "${content}" "${forbidden}" offset)
    if(NOT offset EQUAL -1)
        message(FATAL_ERROR "Desktop-assist launcher contains forbidden expansion: ${forbidden}")
    endif()
endforeach()

string(FIND "${content}" "if (-not $ValidateOnly -and -not $ApproveStageAndLaunch)" approval_offset)
string(FIND "${content}" "Install-FnvxrProductArtifactSet" stage_offset)
string(FIND "${content}" "Start-Process" launch_offset)
string(FIND "${content}" "New-Item -ItemType Directory -Path $runDirectory" run_directory_offset)
if(approval_offset EQUAL -1 OR stage_offset EQUAL -1 OR launch_offset EQUAL -1
    OR run_directory_offset EQUAL -1
    OR approval_offset GREATER stage_offset
    OR approval_offset GREATER launch_offset
    OR approval_offset GREATER run_directory_offset)
    message(FATAL_ERROR "Desktop-assist approval guard must precede every stage, launch, and run-directory mutation")
endif()

string(FIND "${content}" "if ($ValidateOnly) {" validate_offset)
string(FIND "${content}" "liveActionsTaken = $false" validate_result_offset)
if(validate_offset EQUAL -1 OR validate_result_offset EQUAL -1 OR validate_offset GREATER validate_result_offset
    OR validate_offset GREATER stage_offset)
    message(FATAL_ERROR "Desktop-assist validate-only path must report before live staging")
endif()

message(STATUS "Desktop assist launcher fuse PASS (explicit approval, two-file temporary stage, automatic restoration, no OpenXR/stereo expansion)")
