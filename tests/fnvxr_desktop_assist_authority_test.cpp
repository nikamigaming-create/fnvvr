#include "fnvxr_desktop_assist_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::engine::DesktopAssistCameraRequest;
using fnvxr::engine::compatibility::RetailCompatibilityFailure;
using fnvxr::engine::compatibility::RetailCompatibilityProof;

RetailCompatibilityProof completeProof()
{
    RetailCompatibilityProof proof {};
    proof.evidence = {
        true, true, true, true, true, true,
        true, true, true, true, true,
    };
    proof.diagnostics.runtimeImageBase = 0x400000u;
    proof.diagnostics.processId = 42u;
    proof.diagnostics.processCreationTime100ns = 99u;
    proof.failure = RetailCompatibilityFailure::None;
    proof.compatible = true;
    return proof;
}

DesktopAssistCameraRequest completeRequest()
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
    using fnvxr::engine::desktopAssistCameraAuthorized;
    using fnvxr::engine::desktopAssistCameraRequestIsNarrow;

    require(
        desktopAssistCameraAuthorized(completeProof(), completeRequest()),
        "complete rotation-only desktop-assist request was rejected");
    require(
        desktopAssistCameraRequestIsNarrow(completeRequest()),
        "complete rotation-only desktop-assist request was not narrow");

    for (int missing = 0; missing < 5; ++missing)
    {
        DesktopAssistCameraRequest request = completeRequest();
        bool* fields[] = {
            &request.desktopAssistProfile,
            &request.cameraOnlyRequested,
            &request.cameraPoseApplicationRequested,
            &request.appliesLocalRotation,
            &request.yawPitchRollEnabled,
        };
        *fields[missing] = false;
        require(
            !desktopAssistCameraAuthorized(completeProof(), request),
            "incomplete desktop-assist request authorized a camera hook");
        require(
            !desktopAssistCameraRequestIsNarrow(request),
            "incomplete desktop-assist request was treated as narrow");
    }

    DesktopAssistCameraRequest request = completeRequest();
    request.writesWorldTransform = true;
    require(
        !desktopAssistCameraAuthorized(completeProof(), request),
        "desktop assist authorized a world-transform write");
    request = completeRequest();
    request.callsUnverifiedTransformUpdate = true;
    require(
        !desktopAssistCameraAuthorized(completeProof(), request),
        "desktop assist authorized the unverified transform update");
    request = completeRequest();
    request.appliesLocalTranslation = true;
    require(
        !desktopAssistCameraAuthorized(completeProof(), request),
        "rotation-only desktop assist authorized translation");

    RetailCompatibilityProof proof = completeProof();
    proof.evidence.protectedFunctionInventoryMatched = false;
    require(
        !desktopAssistCameraAuthorized(proof, completeRequest()),
        "incomplete retail function proof authorized a camera hook");
    proof = completeProof();
    proof.evidence.moduleSnapshotStable = false;
    require(
        !desktopAssistCameraAuthorized(proof, completeRequest()),
        "unstable module inventory authorized a camera hook");
    proof = completeProof();
    proof.compatible = false;
    require(
        !desktopAssistCameraAuthorized(proof, completeRequest()),
        "incompatible process authorized a camera hook");

    std::cout << "desktop assist authority gate passed\n";
    return EXIT_SUCCESS;
}
