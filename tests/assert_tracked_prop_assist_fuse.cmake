if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
    message(FATAL_ERROR "SOURCE_ROOT must name the repository root")
endif()

set(authority "${SOURCE_ROOT}/runtime/fnvxr_tracked_prop_assist_authority.h")
set(plugin "${SOURCE_ROOT}/plugin/fnvxr_nvse_plugin.cpp")
set(launcher "${SOURCE_ROOT}/scripts/start-desktop-assist.ps1")
foreach(path IN LISTS authority plugin launcher)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "tracked-prop visual-trial contract is missing: ${path}")
    endif()
endforeach()

file(READ "${authority}" authority_text)
file(READ "${plugin}" plugin_text)
file(READ "${launcher}" launcher_text)

function(require_source_text text_value needle failure)
    string(FIND "${text_value}" "${needle}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "${failure}")
    endif()
endfunction()

foreach(required IN ITEMS
    "struct TrackedPropAssistRequest"
    "bool visualOnlyRequested = false;"
    "bool rigHookRequested = false;"
    "bool rigTransformWritesRequested = false;"
    "bool weaponTransformWritesRequested = false;"
    "bool rightGripAndAimRequired = false;"
    "bool projectileNodeHookRequested = false;"
    "bool projectileOrHitMutationRequested = false;"
    "bool inputInjectionRequested = false;"
    "bool worldStereoRequested = false;"
    "bool legacyReplayRequested = false;"
    "bool openXrPresentationRequested = false;"
    "bool uiCaptureRequested = false;"
    "retailObservationAuthorized(proof)")
    require_source_text(
        "${authority_text}"
        "${required}"
        "Tracked-prop authority lost required narrow field or same-process compatibility proof: ${required}")
endforeach()

foreach(required IN ITEMS
    "runProfileIs(\"tracked-prop-assist\")"
    "FNVXR_TRACKED_PROP_ASSIST_VISUAL_ONLY"
    "bool trackedPropAssistMutationAllowedAtDecision()"
    "fnvxr::engine::trackedPropAssistAuthorized"
    "bool ensureAuthorizedTrackedPropAssistBridgeStarted()"
    "void processTrackedPropAssistMainLoop"
    "captureTrackedPropAssistRigOrigin"
    "first-person-camera-at-latch"
    "right-grip-or-aim-not-current"
    "projectile=0 hit=0 renderer=0 replay=0 openxr=0")
    require_source_text(
        "${plugin_text}"
        "${required}"
        "Tracked-prop visual trial lost required implementation contract: ${required}")
endforeach()

string(FIND "${plugin_text}" "bool ensureAuthorizedTrackedPropAssistBridgeStarted()" bridge_start)
string(FIND "${plugin_text}" "bool ensureAuthorizedHeadlessStereoRigVisualTrialBridgeStarted()" bridge_end)
if(bridge_start EQUAL -1 OR bridge_end EQUAL -1 OR bridge_end LESS bridge_start)
    message(FATAL_ERROR "Could not isolate tracked-prop bridge")
endif()
math(EXPR bridge_length "${bridge_end} - ${bridge_start}")
string(SUBSTRING "${plugin_text}" ${bridge_start} ${bridge_length} bridge_body)
foreach(forbidden IN ITEMS
    "initSharedXInput();"
    "initSharedDInput();"
    "initSharedCommand();"
    "initSharedInputEvents();"
    "startBridge()")
    string(FIND "${bridge_body}" "${forbidden}" forbidden_offset)
    if(NOT forbidden_offset EQUAL -1)
        message(FATAL_ERROR "Tracked-prop bridge must not initialize forbidden side effect: ${forbidden}")
    endif()
endforeach()

string(FIND "${plugin_text}" "void onRetailPostAnimation(void* animData)" rig_start)
string(FIND "${plugin_text}" "bool installRetailRigHook()" rig_end)
if(rig_start EQUAL -1 OR rig_end EQUAL -1 OR rig_end LESS rig_start)
    message(FATAL_ERROR "Could not isolate tracked-prop post-animation path")
endif()
math(EXPR rig_length "${rig_end} - ${rig_start}")
string(SUBSTRING "${plugin_text}" ${rig_start} ${rig_length} rig_body)
string(FIND "${rig_body}" "if (!trackedPropAssist)" projectile_guard)
string(FIND "${rig_body}" "installProjectileNodeConsumeHook(playerProcess);" projectile_install)
if(projectile_guard EQUAL -1 OR projectile_install EQUAL -1 OR projectile_guard GREATER projectile_install)
    message(FATAL_ERROR "Tracked-prop visual rig can reach the projectile-node hook without its explicit exclusion")
endif()

string(FIND "${plugin_text}" "bool installRetailRigHook()" hook_start)
string(FIND "${plugin_text}" "TileValue* getTileValue" hook_end)
if(hook_start EQUAL -1 OR hook_end EQUAL -1 OR hook_end LESS hook_start)
    message(FATAL_ERROR "Could not isolate retail rig-hook installation")
endif()
math(EXPR hook_length "${hook_end} - ${hook_start}")
string(SUBSTRING "${plugin_text}" ${hook_start} ${hook_length} hook_body)
string(FIND "${hook_body}" "if (!trackedPropAssistMutationAllowedAtDecision())" trial_gate)
string(FIND "${hook_body}" "else if (!retailMutationAllowedForCurrentProcess(requested))" retail_gate)
string(FIND "${hook_body}" "writeJump(PlayerAnimationApplyCallSiteAddress" hook_write)
if(trial_gate EQUAL -1 OR retail_gate EQUAL -1 OR hook_write EQUAL -1
    OR trial_gate GREATER hook_write OR retail_gate GREATER hook_write)
    message(FATAL_ERROR "Rig-hook write is not downstream of both the visual-trial and full-retail gates")
endif()

foreach(required IN ITEMS
    "[switch]$TrackedPropVisualTrial"
    "FNVXR_RUN_PROFILE = \"tracked-prop-assist\""
    "FNVXR_TRACKED_PROP_ASSIST_VISUAL_ONLY = \"1\""
    "FNVXR_RETAIL_RIG_ENABLE = \"1\""
    "FNVXR_RETAIL_RIG_APPLY = \"1\""
    "FNVXR_RETAIL_WEAPON_APPLY = \"1\""
    "FNVXR_RETAIL_PROJECTILE_NODE_HOOK = \"0\""
    "FNVXR_TRACKED_PROP_ASSIST_PROJECTILE_OR_HIT_MUTATION = \"0\""
    "FNVXR_TRACKED_PROP_ASSIST_OPENXR_PRESENTATION = \"0\""
    "FNVXR_DESKTOP_ASSIST_UI_CAPTURE = \"0\""
    "FNVXR_D3D9_STEREO_REPLAY = \"0\""
    "FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY = \"0\""
    "FNVXR_D3D9_WIDE_WORLD_REPLAY = \"0\"")
    require_source_text(
        "${launcher_text}"
        "${required}"
        "Tracked-prop launcher lost required visual-only disarm: ${required}")
endforeach()

message(STATUS "Tracked-prop assist fuse PASS (body-anchored visual rig only; no input, projectile/hit, renderer/replay, or OpenXR path)")
