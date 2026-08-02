#include "fnvxr_tracked_prop_assist_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::engine::TrackedPropAssistRequest;
using fnvxr::engine::HeadlessStereoRigVisualTrialRequest;
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

TrackedPropAssistRequest completeRequest()
{
    TrackedPropAssistRequest request {};
    request.trackedPropAssistProfile = true;
    request.visualOnlyRequested = true;
    request.cameraPoseApplicationRequested = true;
    request.appliesLocalCameraRotation = true;
    request.yawPitchRollEnabled = true;
    request.rigHookRequested = true;
    request.rigTransformWritesRequested = true;
    request.weaponTransformWritesRequested = true;
    request.rightGripAndAimRequired = true;
    return request;
}

HeadlessStereoRigVisualTrialRequest completeHeadlessStereoRigRequest()
{
    HeadlessStereoRigVisualTrialRequest request {};
    request.stereoVisualTrialProfile = true;
    request.headlessSimulator = true;
    request.ownedHeadsetFixture = true;
    request.worldOnlyCapture = true;
    request.stockWeaponDrawRequested = true;
    request.rigHookRequested = true;
    request.rigTransformWritesRequested = true;
    request.weaponTransformWritesRequested = true;
    request.rightGripAndAimRequired = true;
    request.engineCenterStereoRequested = true;
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
    using fnvxr::engine::trackedPropAssistAuthorized;
    using fnvxr::engine::trackedPropAssistRequestIsNarrow;
    using fnvxr::engine::headlessStereoRigVisualTrialAuthorized;
    using fnvxr::engine::headlessStereoRigVisualTrialRequestIsNarrow;

    require(
        trackedPropAssistAuthorized(completeProof(), completeRequest()),
        "complete visual tracked-prop request was rejected");
    require(
        trackedPropAssistRequestIsNarrow(completeRequest()),
        "complete visual tracked-prop request was not narrow");

    for (int missing = 0; missing < 9; ++missing)
    {
        TrackedPropAssistRequest request = completeRequest();
        bool* fields[] = {
            &request.trackedPropAssistProfile,
            &request.visualOnlyRequested,
            &request.cameraPoseApplicationRequested,
            &request.appliesLocalCameraRotation,
            &request.yawPitchRollEnabled,
            &request.rigHookRequested,
            &request.rigTransformWritesRequested,
            &request.weaponTransformWritesRequested,
            &request.rightGripAndAimRequired,
        };
        *fields[missing] = false;
        require(
            !trackedPropAssistAuthorized(completeProof(), request),
            "incomplete tracked-prop request authorized rig/weapon writes");
        require(
            !trackedPropAssistRequestIsNarrow(request),
            "incomplete tracked-prop request was treated as narrow");
    }

    TrackedPropAssistRequest request = completeRequest();
    bool* forbidden[] = {
        &request.appliesLocalCameraTranslation,
        &request.writesWorldTransform,
        &request.callsUnverifiedTransformUpdate,
        &request.projectileNodeHookRequested,
        &request.projectileOrHitMutationRequested,
        &request.inputInjectionRequested,
        &request.worldStereoRequested,
        &request.legacyReplayRequested,
        &request.openXrPresentationRequested,
        &request.uiCaptureRequested,
    };
    for (bool* field : forbidden)
    {
        request = completeRequest();
        *field = true;
        require(
            !trackedPropAssistAuthorized(completeProof(), request),
            "tracked-prop request authorized a forbidden side effect");
    }

    RetailCompatibilityProof proof = completeProof();
    proof.evidence.protectedFunctionInventoryMatched = false;
    require(
        !trackedPropAssistAuthorized(proof, completeRequest()),
        "incomplete retail function proof authorized tracked-prop writes");
    proof = completeProof();
    proof.evidence.moduleSnapshotStable = false;
    require(
        !trackedPropAssistAuthorized(proof, completeRequest()),
        "unstable module inventory authorized tracked-prop writes");
    proof = completeProof();
    proof.compatible = false;
    require(
        !trackedPropAssistAuthorized(proof, completeRequest()),
        "incompatible process authorized tracked-prop writes");

    require(
        headlessStereoRigVisualTrialAuthorized(
            completeProof(),
            completeHeadlessStereoRigRequest()),
        "complete headless stereo visual-rig request was rejected");
    require(
        headlessStereoRigVisualTrialRequestIsNarrow(
            completeHeadlessStereoRigRequest()),
        "complete headless stereo visual-rig request was not narrow");

    for (int missing = 0; missing < 10; ++missing)
    {
        HeadlessStereoRigVisualTrialRequest candidate =
            completeHeadlessStereoRigRequest();
        bool* fields[] = {
            &candidate.stereoVisualTrialProfile,
            &candidate.headlessSimulator,
            &candidate.ownedHeadsetFixture,
            &candidate.worldOnlyCapture,
            &candidate.stockWeaponDrawRequested,
            &candidate.rigHookRequested,
            &candidate.rigTransformWritesRequested,
            &candidate.weaponTransformWritesRequested,
            &candidate.rightGripAndAimRequired,
            &candidate.engineCenterStereoRequested,
        };
        *fields[missing] = false;
        require(
            !headlessStereoRigVisualTrialAuthorized(completeProof(), candidate),
            "incomplete headless stereo visual-rig request authorized writes");
    }

    HeadlessStereoRigVisualTrialRequest headlessRequest =
        completeHeadlessStereoRigRequest();
    bool* headlessForbidden[] = {
        &headlessRequest.finalStockFrameCaptureRequested,
        &headlessRequest.cameraHookRequested,
        &headlessRequest.projectileNodeHookRequested,
        &headlessRequest.projectileOrHitMutationRequested,
        &headlessRequest.inputInjectionRequested,
        &headlessRequest.legacyReplayRequested,
        &headlessRequest.uiCaptureRequested,
        &headlessRequest.physicalHeadsetRequested,
    };
    for (bool* field : headlessForbidden)
    {
        headlessRequest = completeHeadlessStereoRigRequest();
        *field = true;
        require(
            !headlessStereoRigVisualTrialAuthorized(
                completeProof(),
                headlessRequest),
            "headless stereo visual-rig request authorized a forbidden side effect");
    }
    proof = completeProof();
    proof.evidence.protectedVtableBlocksMatched = false;
    require(
        !headlessStereoRigVisualTrialAuthorized(
            proof,
            completeHeadlessStereoRigRequest()),
        "incomplete retail proof authorized headless stereo visual-rig writes");

    std::cout << "tracked-prop assist authority gate passed\n";
    return EXIT_SUCCESS;
}
