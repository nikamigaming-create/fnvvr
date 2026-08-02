if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(proxy "${SOURCE_ROOT}/renderhook/fnvxr_d3d9_proxy.cpp")
set(authority "${SOURCE_ROOT}/runtime/fnvxr_desktop_assist_ui_authority.h")
set(evidence "${SOURCE_ROOT}/runtime/fnvxr_desktop_assist_ui_evidence.h")
set(protocol "${SOURCE_ROOT}/protocol/fnvxr_shared_state.h")
set(assist "${SOURCE_ROOT}/tools/fnvxr_assist.cpp")
foreach(path IN LISTS proxy authority evidence protocol assist)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing desktop-assist UI source: ${path}")
    endif()
endforeach()

file(READ "${proxy}" proxy_text)
file(READ "${authority}" authority_text)
file(READ "${evidence}" evidence_text)
file(READ "${protocol}" protocol_text)
file(READ "${assist}" assist_text)
foreach(variable IN ITEMS proxy_text authority_text evidence_text protocol_text assist_text)
    string(REPLACE "\r\n" "\n" ${variable} "${${variable}}")
endforeach()

function(require_text haystack needle reason)
    string(FIND "${haystack}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${reason}: missing '${needle}'")
    endif()
endfunction()

function(forbid_text haystack needle reason)
    string(FIND "${haystack}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "${reason}: found '${needle}'")
    endif()
endfunction()

function(extract_region output haystack begin_marker end_marker)
    string(FIND "${haystack}" "${begin_marker}" begin_at)
    string(FIND "${haystack}" "${end_marker}" end_at)
    if(begin_at EQUAL -1 OR end_at EQUAL -1 OR end_at LESS_EQUAL begin_at)
        message(FATAL_ERROR "Could not isolate '${begin_marker}' through '${end_marker}'")
    endif()
    math(EXPR region_length "${end_at} - ${begin_at}")
    string(SUBSTRING "${haystack}" ${begin_at} ${region_length} region)
    set(${output} "${region}" PARENT_SCOPE)
endfunction()

require_text(
    "${authority_text}"
    "desktopAssistUiCaptureRequestIsNarrow"
    "UI capture needs a pure narrow authority predicate")
foreach(required IN ITEMS
        "desktopAssistProfile"
        "cameraOnlyRequested"
        "uiCaptureRequested"
        "exactRetailD3D9Bootstrap"
        "nativePresentSlotLeaseOnly"
        "confirmedRetailUiRequired"
        "dedicatedUiMapping"
        "cpuReadbackOnly")
    require_text(
        "${authority_text}"
        "${required}"
        "UI capture policy is missing required evidence")
endforeach()
foreach(forbidden IN ITEMS
        "worldStereoRequested"
        "legacyStereoReplayRequested"
        "openXrPresentationRequested"
        "inputInjectionRequested"
        "worldTransformWriteRequested"
        "rigOrWeaponMutationRequested")
    require_text(
        "${authority_text}"
        "!request.${forbidden}"
        "UI capture policy failed to reject an unrelated authority")
endforeach()

foreach(required IN ITEMS
        "desktopAssistUiQuadHeaderIsComplete"
        "desktopAssistUiQuadPayloadLayoutIsValid"
        "desktopAssistUiQuadPixelHash"
        "DesktopAssistUiQuadRequiredFlags")
    require_text(
        "${evidence_text}"
        "${required}"
        "UI capture evidence contract is incomplete")
endforeach()
require_text(
    "${proxy_text}"
    "fnvxr_desktop_assist_ui_evidence.h"
    "UI producer must use the shared evidence contract")
foreach(required IN ITEMS
        "desktopAssistUiQuadPixelsMatchHeader"
        "desktopAssistUiQuadPixelsVerifiedObserved"
        "desktopAssistUiQuadInvalidatedAfterUiObserved"
        "desktopAssistUiQuadExitPendingInvalidation"
        "snapshot.desktopAssistUiQuadPixelsVerified"
        "snapshot.desktopAssistUiQuadHeader.captureFailure != 0u")
    require_text(
        "${assist_text}"
        "${required}"
        "assist harness must prove pixel evidence and a post-UI invalidation")
endforeach()

require_text(
    "${protocol_text}"
    "DesktopAssistUiQuadSharedMappingName"
    "UI capture must use a dedicated mapping")
require_text(
    "${protocol_text}"
    "SharedDesktopAssistUiQuadHeader"
    "UI capture must carry explicit lineage metadata")
foreach(required IN ITEMS
        "runtimeStateSample"
        "poseProducerEpoch"
        "captureOrdinal"
        "DesktopAssistUiQuadFlagRuntimeUiConfirmed"
        "DesktopAssistUiQuadFlagPixelCopyComplete"
        "DesktopAssistUiQuadFlagPoseEpochCurrent")
    require_text(
        "${protocol_text}"
        "${required}"
        "UI capture protocol is missing lineage or completion evidence")
endforeach()

extract_region(
    profile_region
    "${proxy_text}"
    "bool desktopAssistUiCaptureRequested()\n{"
    "std::uint32_t nextNativeStereoPairBits()")
foreach(required IN ITEMS
        "runProfileIs(\"desktop-assist\")"
        "FNVXR_DESKTOP_ASSIST_CAMERA_ONLY"
        "FNVXR_DESKTOP_ASSIST_UI_CAPTURE")
    require_text(
        "${profile_region}"
        "${required}"
        "UI capture profile is not a two-switch desktop-only opt-in")
endforeach()

extract_region(
    bootstrap_region
    "${proxy_text}"
    "bool initializeDesktopAssistUiPresentBootstrap(\n    IDirect3DDevice9* device) noexcept\n{"
    "bool initializeRetailVrPresentBootstrap")
foreach(required IN ITEMS
        "desktopAssistUiCaptureRequested()"
        "currentLoadedExecutableMatchesSupportedRetail()"
        "desktopAssistUiCaptureRequestIsNarrow(request)"
        "logDesktopAssistD3D9DeviceCapability(device)"
        "gDesktopAssistUiTrackedFrames.initialize()"
        "readDesktopAssistUiPublishedFrame"
        "copyDesktopAssistUiBackBuffer"
        "publishDesktopAssistUiQuad"
        "withholdDesktopAssistUiQuad"
        "initializeAuthorizedDevice")
    require_text(
        "${bootstrap_region}"
        "${required}"
        "desktop UI bootstrap is incomplete")
endforeach()
foreach(forbidden IN ITEMS
        "initializeRetailVrBridge("
        "prepareRetailEngineEyeTargetOperations"
        "publishRetailVrCpuPair"
        "gLeftEyeSurface"
        "gRightEyeSurface")
    forbid_text(
        "${bootstrap_region}"
        "${forbidden}"
        "desktop UI bootstrap reached the world-stereo path")
endforeach()

extract_region(
    capability_region
    "${proxy_text}"
    "void logDesktopAssistD3D9DeviceCapability"
    "bool initializeDesktopAssistUiPresentBootstrap(\n    IDirect3DDevice9* device) noexcept\n{")
foreach(required IN ITEMS
        "__uuidof(IDirect3DDevice9Ex)"
        "device->GetCreationParameters(&creation)"
        "deviceEx->Release()"
        "capability evidence only, no GPU transport or world-stereo authorization")
    require_text(
        "${capability_region}"
        "${required}"
        "desktop capability record is incomplete or can be mistaken for renderer authorization")
endforeach()
foreach(forbidden IN ITEMS
        "CreateTexture("
        "CreateRenderTarget("
        "initializeRetailVrBridge("
        "prepareRetailEngineEyeTargetOperations")
    forbid_text(
        "${capability_region}"
        "${forbidden}"
        "desktop capability record must stay read-only and outside the world-stereo path")
endforeach()

extract_region(
    capture_region
    "${proxy_text}"
    "bool copyDesktopAssistUiBackBuffer(\n    void* opaque,\n    void* deviceOpaque) noexcept\n{"
    "bool publishDesktopAssistUiQuad(")
foreach(required IN ITEMS
        "desktopAssistUiCaptureRequested()"
        "ensureDesktopAssistUiQuadMapping()"
        "GetRenderTargetData("
        "desktopAssistUiQuadPixelHash("
        "DesktopAssistUiQuadFlagPresentHookInstalled"
        "gDesktopAssistUiQuadCopyPending = true")
    require_text(
        "${capture_region}"
        "${required}"
        "desktop UI capture does not publish a bounded Present-copy transaction")
endforeach()
foreach(forbidden IN ITEMS
        "initializeRetailVrBridge("
        "prepareRetailEngineEyeTargetOperations"
        "gLeftEyeSurface"
        "gRightEyeSurface"
        "gStereoReplayEnabled"
        "gNativeStereoEnabled")
    forbid_text(
        "${capture_region}"
        "${forbidden}"
        "desktop UI copy reached the stereo renderer")
endforeach()

extract_region(
    publication_region
    "${proxy_text}"
    "bool publishDesktopAssistUiQuad(\n    void*,\n    const fnvxr::engine::RetailTrackedFrame& frame) noexcept\n{"
    "void withholdDesktopAssistUiQuad")
foreach(required IN ITEMS
        "validateRetailTrackedUiFrame(frame).complete()"
        "runtimeStateSample = frame.runtime.frame"
        "poseProducerEpoch = frame.pose.producerEpoch"
        "DesktopAssistUiQuadFlagRuntimeUiConfirmed"
        "DesktopAssistUiQuadFlagPixelCopyComplete"
        "DesktopAssistUiQuadFlagPoseEpochCurrent")
    require_text(
        "${publication_region}"
        "${required}"
        "desktop UI publication does not bind its evidence to UI/runtime/pose lineage")
endforeach()

extract_region(
    create_device_region
    "${proxy_text}"
    "if (desktopAssistUiCaptureRequested())\n            {"
    "else if (retailVrVisualTrialRequested())")
require_text(
    "${create_device_region}"
    "initializeDesktopAssistUiPresentBootstrap(*returnedDevice)"
    "desktop UI capture must install only its narrow bootstrap")
forbid_text(
    "${create_device_region}"
    "initializeRetailVrBridge("
    "desktop UI CreateDevice branch reached the world bridge")

message(STATUS "Desktop assist UI capture fuse PASS")
