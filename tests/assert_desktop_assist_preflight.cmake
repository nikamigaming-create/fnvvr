if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
    message(FATAL_ERROR "SOURCE_ROOT must name the repository root")
endif()

set(script "${SOURCE_ROOT}/scripts/validate-desktop-assist.ps1")
if(NOT EXISTS "${script}")
    message(FATAL_ERROR "Desktop-assist preflight is missing: ${script}")
endif()

file(READ "${script}" content)

foreach(required IN ITEMS
    "fnvxr-desktop-assist-preflight-v1"
    "read-only preflight; does not build, stage, launch, stop, mutate a game tree, or set process environment"
    "FNVXR_DESKTOP_ASSIST_CAMERA_ONLY = \"1\""
    "FNVXR_DESKTOP_ASSIST_UI_CAPTURE = \"1\""
    "FNVXR_CAMERA_APPLY_TRANSLATION = \"0\""
    "FNVXR_CAMERA_WRITE_WORLD = \"0\""
    "FNVXR_CAMERA_UPDATE_TRANSFORM = \"0\""
    "FNVXR_RETAIL_RIG_ENABLE = \"0\""
    "FNVXR_RETAIL_WEAPON_APPLY = \"0\""
    "FNVXR_DISABLE_STEREO_WORLD = \"1\""
    "win32D3D9Proxy"
    "retailRuntimeProbe"
    "commandHelper"
    "RequireAutomationRecoverySave"
    "automationRecoverySave"
    "fixed recovery load plus two verified Escape taps"
    "installedD3D9ProxyObservation")
    string(FIND "${content}" "${required}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "Desktop-assist preflight lost required fail-closed contract: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "Start-Process"
    "Stop-Process"
    "Copy-Item"
    "Move-Item"
    "Remove-Item"
    "Set-Item Env:"
    "[Environment]::SetEnvironmentVariable")
    string(FIND "${content}" "${forbidden}" offset)
    if(NOT offset EQUAL -1)
        message(FATAL_ERROR "Desktop-assist preflight must stay read-only; found ${forbidden}")
    endif()
endforeach()

message(STATUS "Desktop assist preflight fuse PASS (read-only; no stage, launch, process control, or environment mutation)")
