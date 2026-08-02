#include "fnvxr_desktop_assist_automation_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::engine::DesktopAssistAutomationAction;
using fnvxr::engine::DesktopAssistCameraRequest;

DesktopAssistCameraRequest narrowRequest()
{
    DesktopAssistCameraRequest request {};
    request.desktopAssistProfile = true;
    request.cameraOnlyRequested = true;
    request.cameraPoseApplicationRequested = true;
    request.appliesLocalRotation = true;
    request.yawPitchRollEnabled = true;
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
    using fnvxr::engine::desktopAssistAutomationActionIsNarrow;
    using fnvxr::engine::desktopAssistAutomationAuthorized;

    require(
        desktopAssistAutomationActionIsNarrow(
            DesktopAssistAutomationAction::LoadFixedRecoverySave),
        "fixed recovery-load action was not recognized as narrow");
    require(
        !desktopAssistAutomationActionIsNarrow(DesktopAssistAutomationAction::None),
        "empty automation action was treated as authorized");
    require(
        desktopAssistAutomationAuthorized(
            narrowRequest(),
            true,
            DesktopAssistAutomationAction::LoadFixedRecoverySave),
        "explicit fixed recovery-load action was rejected");
    require(
        !desktopAssistAutomationAuthorized(
            narrowRequest(),
            false,
            DesktopAssistAutomationAction::LoadFixedRecoverySave),
        "recovery load was authorized without explicit automation opt-in");
    require(
        !desktopAssistAutomationAuthorized(
            narrowRequest(),
            true,
            DesktopAssistAutomationAction::None),
        "empty automation action was authorized");

    DesktopAssistCameraRequest expanded = narrowRequest();
    expanded.appliesLocalTranslation = true;
    require(
        !desktopAssistAutomationAuthorized(
            expanded,
            true,
            DesktopAssistAutomationAction::LoadFixedRecoverySave),
        "recovery load was authorized with a broadened camera request");

    std::cout << "desktop assist automation authority gate passed\n";
    return EXIT_SUCCESS;
}
