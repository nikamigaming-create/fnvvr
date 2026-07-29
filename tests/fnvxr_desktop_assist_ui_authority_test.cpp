#include "fnvxr_desktop_assist_ui_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::engine::DesktopAssistUiCaptureRequest;

DesktopAssistUiCaptureRequest completeRequest()
{
    DesktopAssistUiCaptureRequest request {};
    request.desktopAssistProfile = true;
    request.cameraOnlyRequested = true;
    request.uiCaptureRequested = true;
    request.exactRetailD3D9Bootstrap = true;
    request.nativePresentSlotLeaseOnly = true;
    request.confirmedRetailUiRequired = true;
    request.dedicatedUiMapping = true;
    request.cpuReadbackOnly = true;
    return request;
}

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using fnvxr::engine::desktopAssistUiCaptureRequestIsNarrow;

    require(
        desktopAssistUiCaptureRequestIsNarrow(completeRequest()),
        "complete desktop UI capture request was rejected");

    for (int missing = 0; missing < 8; ++missing)
    {
        DesktopAssistUiCaptureRequest request = completeRequest();
        bool* fields[] = {
            &request.desktopAssistProfile,
            &request.cameraOnlyRequested,
            &request.uiCaptureRequested,
            &request.exactRetailD3D9Bootstrap,
            &request.nativePresentSlotLeaseOnly,
            &request.confirmedRetailUiRequired,
            &request.dedicatedUiMapping,
            &request.cpuReadbackOnly,
        };
        *fields[missing] = false;
        require(
            !desktopAssistUiCaptureRequestIsNarrow(request),
            "incomplete desktop UI capture request was authorized");
    }

    DesktopAssistUiCaptureRequest request = completeRequest();
    request.worldStereoRequested = true;
    require(
        !desktopAssistUiCaptureRequestIsNarrow(request),
        "desktop UI capture authorized world stereo");
    request = completeRequest();
    request.legacyStereoReplayRequested = true;
    require(
        !desktopAssistUiCaptureRequestIsNarrow(request),
        "desktop UI capture authorized legacy replay");
    request = completeRequest();
    request.openXrPresentationRequested = true;
    require(
        !desktopAssistUiCaptureRequestIsNarrow(request),
        "desktop UI capture authorized OpenXR presentation");
    request = completeRequest();
    request.inputInjectionRequested = true;
    require(
        !desktopAssistUiCaptureRequestIsNarrow(request),
        "desktop UI capture authorized input injection");
    request = completeRequest();
    request.worldTransformWriteRequested = true;
    require(
        !desktopAssistUiCaptureRequestIsNarrow(request),
        "desktop UI capture authorized a world-transform write");
    request = completeRequest();
    request.rigOrWeaponMutationRequested = true;
    require(
        !desktopAssistUiCaptureRequestIsNarrow(request),
        "desktop UI capture authorized a rig or weapon mutation");

    std::cout << "desktop assist UI authority gate passed\n";
    return EXIT_SUCCESS;
}
