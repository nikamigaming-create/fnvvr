if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required")
endif()

set(controller "${ROOT}/renderhook/fnvxr_retail_ui_quad_capture.h")
set(hook_header "${ROOT}/renderhook/fnvxr_retail_ui_quad_capture_win32.h")
set(hook_source "${ROOT}/renderhook/fnvxr_retail_ui_quad_capture_win32.cpp")
set(proxy "${ROOT}/renderhook/fnvxr_d3d9_proxy.cpp")
set(bridge "${ROOT}/renderhook/fnvxr_retail_vr_bridge_win32.h")
foreach(path IN LISTS controller hook_header hook_source proxy bridge)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing UI capture source: ${path}")
    endif()
endforeach()

file(READ "${controller}" controller_text)
file(READ "${hook_header}" hook_header_text)
file(READ "${hook_source}" hook_source_text)
file(READ "${proxy}" proxy_text)
file(READ "${bridge}" bridge_text)
foreach(variable IN ITEMS
        controller_text hook_header_text hook_source_text proxy_text bridge_text)
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

set(isolated_text "${controller_text}\n${hook_header_text}\n${hook_source_text}")
foreach(forbidden IN ITEMS
        "hookedPresent"
        "installDeviceHooks"
        "captureSharedVideoFrame"
        "GetRenderTargetData"
        "LockRect"
        "CreateFileMapping"
        "GetEnvironmentVariable"
        "getenv"
        "readEnv"
        "testPattern"
        "StereoReplay"
        "DrawPrimitive")
    forbid_text(
        "${isolated_text}"
        "${forbidden}"
        "The isolated UI-only Present seam must not reach legacy replay/readback/configuration paths")
endforeach()

require_text(
    "${controller_text}"
    "validateRetailTrackedUiFrame(frame)"
    "Every UI copy must require a stable, explicitly classified retail UI frame")
string(FIND "${controller_text}" "validateRetailTrackedUiFrame(frame)" validation_at)
string(FIND "${controller_text}" "copyBackBufferToMonoTargets(" copy_at)
string(FIND "${controller_text}" "publishMonoUiQuad(" publish_at)
if(validation_at EQUAL -1
    OR copy_at EQUAL -1
    OR publish_at EQUAL -1
    OR validation_at GREATER copy_at
    OR copy_at GREATER publish_at)
    message(FATAL_ERROR
        "UI evidence, GPU copy, and v5 publication must remain strictly ordered")
endif()

require_text(
    "${hook_header_text}"
    "RetailD3D9PresentVtableSlot = 17u"
    "The isolated hook must patch only the IDirect3DDevice9 Present slot")
require_text(
    "${hook_source_text}"
    "mPresentEntry = &originalVtable[RetailD3D9PresentVtableSlot];"
    "The hook must lease only the native Present entry")
require_text(
    "${hook_source_text}"
    "PAGE_EXECUTE_READWRITE"
    "The native Present entry must be made writable only for the bounded lease update")
foreach(forbidden IN ITEMS
        "RetailD3D9ExDeviceMethodCount"
        "VirtualAlloc("
        "mPrivateVtable"
        "std::memcpy(privateVtable"
        "reinterpret_cast<void* volatile*>(deviceVtableAddress)")
    forbid_text(
        "${hook_header_text}\n${hook_source_text}"
        "${forbidden}"
        "The Present hook must never replace or truncate the concrete D3D9 device vtable")
endforeach()
require_text(
    "${hook_source_text}"
    "return original("
    "The adapter must always forward to the saved original Present")
require_text(
    "${hook_source_text}"
    "if (mInstalled)\n        return device == mDevice && ready();"
    "Present installation must be idempotent for the exact authorized device")

require_text(
    "${bridge_text}"
    "PresentationMode::MonoUiQuad"
    "The bridge must label confirmed UI as ABI-v5 MonoUiQuad")
require_text(
    "${bridge_text}"
    "identity.runtimeStateSample = tracked.runtime.frame;"
    "UI publication must retain the exact runtime-state source identity")
require_text(
    "${bridge_text}"
    "mPublicationSequence.claim("
    "World and UI publications must share one monotonic transaction domain")
forbid_text(
    "${bridge_text}"
    "mNextUiTransactionId"
    "UI transaction IDs must not use a disjoint range that regresses on return to world")
forbid_text(
    "${proxy_text}"
    "gRetailUiPresentHook.initializeAuthorizedDevice("
    "The retail product must not mutate the concrete D3D9 device's Present slot")
forbid_text(
    "${proxy_text}"
    "fnvxr_retail_ui_quad_capture_win32.h"
    "The product proxy must not import the native Present-slot hook")
forbid_text(
    "${proxy_text}"
    "initializeRetailVrPresentBootstrap(*returnedDevice)"
    "CreateDevice must return Fallout's ordinary device without installing a vtable hook")
forbid_text(
    "${proxy_text}"
    "initializeRetailVrBridge(*returnedDevice)"
    "CreateDevice must not eagerly initialize the VR bridge before Fallout binds depth")

message(STATUS "Retail UI-only Present capture source fuse PASS")
