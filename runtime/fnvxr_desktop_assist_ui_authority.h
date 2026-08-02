#pragma once

namespace fnvxr::engine
{
// The menu capture is intentionally its own authority boundary. It permits a
// single native Present-slot lease and an explicitly requested CPU copy of a
// confirmed retail UI frame for desktop evidence. It never authorizes world
// stereo, D3D replay, OpenXR, input, actor transforms, or rig/weapon writes.
struct DesktopAssistUiCaptureRequest
{
    bool desktopAssistProfile = false;
    bool cameraOnlyRequested = false;
    bool uiCaptureRequested = false;
    bool exactRetailD3D9Bootstrap = false;
    bool nativePresentSlotLeaseOnly = false;
    bool confirmedRetailUiRequired = false;
    bool dedicatedUiMapping = false;
    bool cpuReadbackOnly = false;
    bool worldStereoRequested = false;
    bool legacyStereoReplayRequested = false;
    bool openXrPresentationRequested = false;
    bool inputInjectionRequested = false;
    bool worldTransformWriteRequested = false;
    bool rigOrWeaponMutationRequested = false;
};

constexpr bool desktopAssistUiCaptureRequestIsNarrow(
    const DesktopAssistUiCaptureRequest& request) noexcept
{
    return request.desktopAssistProfile
        && request.cameraOnlyRequested
        && request.uiCaptureRequested
        && request.exactRetailD3D9Bootstrap
        && request.nativePresentSlotLeaseOnly
        && request.confirmedRetailUiRequired
        && request.dedicatedUiMapping
        && request.cpuReadbackOnly
        && !request.worldStereoRequested
        && !request.legacyStereoReplayRequested
        && !request.openXrPresentationRequested
        && !request.inputInjectionRequested
        && !request.worldTransformWriteRequested
        && !request.rigOrWeaponMutationRequested;
}
}
