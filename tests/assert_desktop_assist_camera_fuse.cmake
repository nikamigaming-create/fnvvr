if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
    message(FATAL_ERROR "SOURCE_ROOT must name the repository root")
endif()

set(authority_header "${SOURCE_ROOT}/runtime/fnvxr_desktop_assist_authority.h")
set(automation_authority_header "${SOURCE_ROOT}/runtime/fnvxr_desktop_assist_automation_authority.h")
set(plugin "${SOURCE_ROOT}/plugin/fnvxr_nvse_plugin.cpp")
set(shared_state "${SOURCE_ROOT}/protocol/fnvxr_shared_state.h")
foreach(path IN LISTS authority_header automation_authority_header plugin shared_state)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "required desktop-assist source is missing: ${path}")
    endif()
endforeach()

file(READ "${authority_header}" authority_text)
file(READ "${automation_authority_header}" automation_authority_text)
file(READ "${plugin}" plugin_text)
file(READ "${shared_state}" shared_state_text)

function(require_source_text text_value needle failure)
    string(FIND "${text_value}" "${needle}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "${failure}")
    endif()
endfunction()

require_source_text(
    "${authority_text}"
    "constexpr bool desktopAssistCameraRequestIsNarrow"
    "Desktop-assist camera policy no longer has a narrow-request gate")
foreach(needle IN ITEMS
    "&& !request.writesWorldTransform"
    "&& !request.callsUnverifiedTransformUpdate"
    "&& !request.appliesLocalTranslation"
    "&& retailObservationAuthorized(proof)")
    require_source_text(
        "${authority_text}"
        "${needle}"
        "Desktop-assist camera policy lost a required no-go condition: ${needle}")
endforeach()

foreach(needle IN ITEMS
    "DesktopAssistAutomationAction"
    "LoadFixedRecoverySave"
    "constexpr bool desktopAssistAutomationAuthorized"
    "&& desktopAssistCameraRequestIsNarrow(cameraRequest)"
    "&& desktopAssistAutomationActionIsNarrow(action)")
    require_source_text(
        "${automation_authority_text}"
        "${needle}"
        "Desktop-assist automation policy lost a required narrow-action gate: ${needle}")
endforeach()

require_source_text(
    "${plugin_text}"
    "#include \"fnvxr_desktop_assist_authority.h\""
    "Desktop-assist camera policy is not compiled into the retail plugin")
require_source_text(
    "${plugin_text}"
    "#include \"fnvxr_desktop_assist_automation_authority.h\""
    "Desktop-assist automation policy is not compiled into the retail plugin")
require_source_text(
    "${plugin_text}"
    "envEnabled(\"FNVXR_DESKTOP_ASSIST_CAMERA_ONLY\", false)"
    "Desktop assist is no longer explicit opt-in")
require_source_text(
    "${plugin_text}"
    "proveCurrentRetailCompatibilityAtDecisionPoint()"
    "Desktop assist no longer re-proves same-process compatibility at its decision point")
require_source_text(
    "${plugin_text}"
    "std::strncmp(\n            request.saveName,\n            DesktopAssistRecoveryLoadCommand,\n            sizeof(request.saveName)) == 0"
    "Desktop-assist recovery command comparison is no longer bounded by the shared mailbox field")
string(FIND
    "${plugin_text}"
    "std::strcmp(request.saveName, DesktopAssistRecoveryLoadCommand)"
    unbounded_recovery_command_compare)
if(NOT unbounded_recovery_command_compare EQUAL -1)
    message(FATAL_ERROR
        "Desktop-assist recovery command comparison can read beyond the shared mailbox field")
endif()
foreach(needle IN ITEMS
    "DesktopAssistSharedMappingName"
    "DesktopAssistFlagCameraPoseApplied"
    "DesktopAssistFlagBodyRootTransformValid"
    "cameraLocalRot[9]"
    "cameraLocalPos[3]"
    "bodyRootWorldRot[9]"
    "bodyRootWorldPos[3]"
    "poseProducerEpoch")
    require_source_text(
        "${shared_state_text}"
        "${needle}"
        "Desktop-assist local-camera evidence record is incomplete: ${needle}")
endforeach()

string(FIND "${plugin_text}" "bool installCameraHook()" camera_start)
string(FIND "${plugin_text}" "Matrix33 transposeMatrix33" camera_end)
if(camera_start EQUAL -1 OR camera_end EQUAL -1 OR camera_end LESS camera_start)
    message(FATAL_ERROR "Could not isolate installCameraHook")
endif()
math(EXPR camera_length "${camera_end} - ${camera_start}")
string(SUBSTRING "${plugin_text}" ${camera_start} ${camera_length} camera_body)
string(FIND "${camera_body}" "if (desktopAssist)" assist_gate)
string(FIND "${camera_body}" "desktopAssistCameraMutationAllowedAtDecision()" assist_proof)
string(FIND "${camera_body}" "if (!retailMutationAllowedForCurrentProcess(requested))" retail_gate)
string(FIND "${camera_body}" "writeJump(PlayerCharacterUpdateCameraAddress" camera_write)
if(assist_gate EQUAL -1 OR assist_proof EQUAL -1 OR retail_gate EQUAL -1 OR camera_write EQUAL -1
    OR assist_gate GREATER camera_write OR assist_proof GREATER camera_write OR retail_gate GREATER camera_write)
    message(FATAL_ERROR
        "Camera hook write is not downstream of both the narrow desktop-assist proof and the full-retail hard gate")
endif()

string(FIND "${plugin_text}" "bool ensureAuthorizedDesktopAssistBridgeStarted()" bridge_start)
string(FIND "${plugin_text}" "bool ensureAuthorizedTrackedPropAssistBridgeStarted()" bridge_end)
if(bridge_start EQUAL -1 OR bridge_end EQUAL -1 OR bridge_end LESS bridge_start)
    message(FATAL_ERROR "Could not isolate desktop-assist bridge")
endif()
math(EXPR bridge_length "${bridge_end} - ${bridge_start}")
string(SUBSTRING "${plugin_text}" ${bridge_start} ${bridge_length} bridge_body)
foreach(required IN ITEMS
    "initSharedVrPose();"
    "initSharedCamera();"
    "initSharedPlayer();"
    "initSharedDesktopAssist();"
    "installCameraHook()"
    "input=0 commandRecovery=%d renderer=0 weapon=0 rig=0 openxr=0")
    require_source_text(
        "${bridge_body}"
        "${required}"
        "Desktop-assist bridge lost required camera-only behavior: ${required}")
endforeach()
foreach(forbidden IN ITEMS
    "initSharedXInput();"
    "initSharedDInput();"
    "initSharedCommand();"
    "initSharedInputEvents();"
    "installRetailRigHook()"
    "startBridge()")
    string(FIND "${bridge_body}" "${forbidden}" forbidden_offset)
    if(NOT forbidden_offset EQUAL -1)
        message(FATAL_ERROR
            "Desktop-assist bridge gained forbidden broader path: ${forbidden}")
    endif()
endforeach()

string(FIND "${plugin_text}" "void processDesktopAssistMainLoop(const RuntimeObservation& observation)" desktop_loop_start)
string(FIND "${plugin_text}" "void handleNvseMessage" desktop_loop_end)
if(desktop_loop_start EQUAL -1 OR desktop_loop_end EQUAL -1 OR desktop_loop_end LESS desktop_loop_start)
    message(FATAL_ERROR "Could not isolate desktop-assist main-loop consumer")
endif()
math(EXPR desktop_loop_length "${desktop_loop_end} - ${desktop_loop_start}")
string(SUBSTRING "${plugin_text}" ${desktop_loop_start} ${desktop_loop_length} desktop_loop_body)
foreach(required IN ITEMS
    "if (desktopAssistAutomationRequested())"
    "consumeDesktopAssistRecoveryLoad("
    "two verified Escape taps"
    "never consumes external input")
    require_source_text(
        "${desktop_loop_body}"
        "${required}"
        "Desktop-assist loop lost required bounded automation behavior: ${required}")
endforeach()
foreach(forbidden IN ITEMS
    "consumeSharedCommand("
    "consumeExternalDInputBridge("
    "consumeExternalXInputBridge("
    "executeAcceptClickOnGameThread(")
    string(FIND "${desktop_loop_body}" "${forbidden}" forbidden_offset)
    if(NOT forbidden_offset EQUAL -1)
        message(FATAL_ERROR
            "Desktop-assist loop gained an unbounded input/command consumer: ${forbidden}")
    endif()
endforeach()

string(FIND "${plugin_text}" "void consumeDesktopAssistRecoveryLoad(" recovery_start)
string(FIND "${plugin_text}" "bool runShowroomCommand" recovery_end)
if(recovery_start EQUAL -1 OR recovery_end EQUAL -1 OR recovery_end LESS recovery_start)
    message(FATAL_ERROR "Could not isolate desktop-assist recovery-load consumer")
endif()
math(EXPR recovery_length "${recovery_end} - ${recovery_start}")
string(SUBSTRING "${plugin_text}" ${recovery_start} ${recovery_length} recovery_body)
foreach(required IN ITEMS
    "DesktopAssistRecoveryLoadCommand"
    "desktopAssistRecoveryLoadCommandIsExact(request)"
    "RuntimeStartMenuBit"
    "recoveryLoadAlreadySubmitted"
    "desktopAssistAutomationAuthorized("
    "DesktopAssistAutomationAction::LoadFixedRecoverySave")
    require_source_text(
        "${recovery_body}"
        "${required}"
        "Desktop-assist recovery-load consumer lost required bound: ${required}")
endforeach()
foreach(forbidden IN ITEMS
    "buildSharedCommandLine("
    "consumeSharedCommand("
    "CommandTypeSave"
    "CommandTypeQuit"
    "SendInput(")
    string(FIND "${recovery_body}" "${forbidden}" forbidden_offset)
    if(NOT forbidden_offset EQUAL -1)
        message(FATAL_ERROR
            "Desktop-assist recovery-load consumer accepts a broader action: ${forbidden}")
    endif()
endforeach()

string(FIND "${plugin_text}" "void updateSharedDesktopAssist(UInt64 frame)" state_start)
string(FIND "${plugin_text}" "void updateNiAvObjectTransform" state_end)
if(state_start EQUAL -1 OR state_end EQUAL -1 OR state_end LESS state_start)
    message(FATAL_ERROR "Could not isolate desktop-assist local-camera observer")
endif()
math(EXPR state_length "${state_end} - ${state_start}")
string(SUBSTRING "${plugin_text}" ${state_start} ${state_length} state_body)
foreach(required IN ITEMS
    "DesktopAssistFlagCameraPoseApplied"
    "cameraLocalRot"
    "cameraLocalPos"
    "cameraWorldRot"
    "cameraWorldPos"
    "retrievePlayerRootNode(false)"
    "bodyRootWorldRot"
    "bodyRootWorldPos"
    "finiteMatrix33(localRotation)"
    "finiteVec3(localPosition)"
    "inCameraGameplay()"
    "g_lastAppliedCamera == camera"
    "g_lastCameraPoseProducerEpoch")
    require_source_text(
        "${state_body}"
        "${required}"
        "Desktop-assist observer lost post-hook evidence: ${required}")
endforeach()
foreach(forbidden IN ITEMS
    "writeMatrix33("
    "writeVec3("
    "updateNiAvObjectTransform(")
    string(FIND "${state_body}" "${forbidden}" forbidden_offset)
    if(NOT forbidden_offset EQUAL -1)
        message(FATAL_ERROR
            "Desktop-assist observer gained an unsafe transform mutation: ${forbidden}")
    endif()
endforeach()

string(FIND "${plugin_text}" "bool ensureAuthorizedSharedBridgeStarted()" full_bridge_start)
string(FIND "${plugin_text}" "void processMainGameLoop" full_bridge_end)
if(full_bridge_start EQUAL -1 OR full_bridge_end EQUAL -1 OR full_bridge_end LESS full_bridge_start)
    message(FATAL_ERROR "Could not isolate full shared bridge")
endif()
math(EXPR full_bridge_length "${full_bridge_end} - ${full_bridge_start}")
string(SUBSTRING "${plugin_text}" ${full_bridge_start} ${full_bridge_length} full_bridge_body)
string(FIND "${full_bridge_body}"
    "desktopAssistProfileSelected()\n        || trackedPropAssistProfileSelected()"
    full_bridge_desktop_guard)
string(FIND "${full_bridge_body}" "initSharedXInput();" full_bridge_input)
if(full_bridge_desktop_guard EQUAL -1 OR full_bridge_input EQUAL -1
    OR full_bridge_desktop_guard GREATER full_bridge_input)
    message(FATAL_ERROR
        "Desktop-assist profile can fall through to the full input/rig/renderer bridge")
endif()

require_source_text(
    "${plugin_text}"
    "desktopAssist profile selected: deferring to camera-only main-loop authority; full bridge is disabled"
    "NVSE load can start the full bridge in a desktop-assist process")

message(STATUS "Desktop assist camera fuse PASS (camera-local default; only fixed recovery-load automation, no rig, renderer, or OpenXR path)")
