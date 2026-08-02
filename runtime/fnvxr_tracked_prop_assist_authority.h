#pragma once

#include "fnvxr_retail_observation_authority.h"

namespace fnvxr::engine
{
// This is a deliberately bounded bridge between the camera-local assist proof
// and the existing first-person rig solver.  It exists only for a visual
// tracked-prop trial: camera-local head rotation plus controller-driven hand
// and weapon transforms.  It does not authorize the projectile path, input,
// world stereo/replay, UI capture, or OpenXR presentation.
struct TrackedPropAssistRequest
{
    bool trackedPropAssistProfile = false;
    bool visualOnlyRequested = false;
    bool cameraPoseApplicationRequested = false;
    bool appliesLocalCameraRotation = false;
    bool yawPitchRollEnabled = false;
    bool appliesLocalCameraTranslation = false;
    bool writesWorldTransform = false;
    bool callsUnverifiedTransformUpdate = false;
    bool rigHookRequested = false;
    bool rigTransformWritesRequested = false;
    bool weaponTransformWritesRequested = false;
    bool rightGripAndAimRequired = false;
    bool projectileNodeHookRequested = false;
    bool projectileOrHitMutationRequested = false;
    bool inputInjectionRequested = false;
    bool worldStereoRequested = false;
    bool legacyReplayRequested = false;
    bool openXrPresentationRequested = false;
    bool uiCaptureRequested = false;
};

constexpr bool trackedPropAssistRequestIsNarrow(
    const TrackedPropAssistRequest& request) noexcept
{
    return request.trackedPropAssistProfile
        && request.visualOnlyRequested
        && request.cameraPoseApplicationRequested
        && request.appliesLocalCameraRotation
        && request.yawPitchRollEnabled
        && request.rigHookRequested
        && request.rigTransformWritesRequested
        && request.weaponTransformWritesRequested
        && request.rightGripAndAimRequired
        && !request.appliesLocalCameraTranslation
        && !request.writesWorldTransform
        && !request.callsUnverifiedTransformUpdate
        && !request.projectileNodeHookRequested
        && !request.projectileOrHitMutationRequested
        && !request.inputInjectionRequested
        && !request.worldStereoRequested
        && !request.legacyReplayRequested
        && !request.openXrPresentationRequested
        && !request.uiCaptureRequested;
}

constexpr bool trackedPropAssistAuthorized(
    const compatibility::RetailCompatibilityProof& proof,
    const TrackedPropAssistRequest& request) noexcept
{
    return trackedPropAssistRequestIsNarrow(request)
        && retailObservationAuthorized(proof);
}

// This is a separate, simulator-only visual-rig lease. Unlike the desktop
// tracked-prop assist it deliberately coexists with the already-reviewed
// engine-center stereo visual trial so that both final eye targets can show
// the controller-driven stock first-person weapon. It grants no gameplay
// input, firing, projectile, hit, replay, UI, camera-hook, or physical-HMD
// authority.
struct HeadlessStereoRigVisualTrialRequest
{
    bool stereoVisualTrialProfile = false;
    bool headlessSimulator = false;
    bool ownedHeadsetFixture = false;
    bool worldOnlyCapture = false;
    bool stockWeaponDrawRequested = false;
    bool finalStockFrameCaptureRequested = false;
    bool cameraHookRequested = false;
    bool rigHookRequested = false;
    bool rigTransformWritesRequested = false;
    bool weaponTransformWritesRequested = false;
    bool rightGripAndAimRequired = false;
    bool engineCenterStereoRequested = false;
    bool projectileNodeHookRequested = false;
    bool projectileOrHitMutationRequested = false;
    bool inputInjectionRequested = false;
    bool legacyReplayRequested = false;
    bool uiCaptureRequested = false;
    bool physicalHeadsetRequested = false;
};

constexpr bool headlessStereoRigVisualTrialRequestIsNarrow(
    const HeadlessStereoRigVisualTrialRequest& request) noexcept
{
    return request.stereoVisualTrialProfile
        && request.headlessSimulator
        && request.ownedHeadsetFixture
        && request.worldOnlyCapture
        && request.stockWeaponDrawRequested
        // The product launcher deliberately forbids the legacy stock
        // backbuffer-copy path.  The controller rig must coexist with the
        // engine-rendered stereo route, never depend on that rejected source.
        && !request.finalStockFrameCaptureRequested
        && !request.cameraHookRequested
        && request.rigHookRequested
        && request.rigTransformWritesRequested
        && request.weaponTransformWritesRequested
        && request.rightGripAndAimRequired
        && request.engineCenterStereoRequested
        && !request.projectileNodeHookRequested
        && !request.projectileOrHitMutationRequested
        && !request.inputInjectionRequested
        && !request.legacyReplayRequested
        && !request.uiCaptureRequested
        && !request.physicalHeadsetRequested;
}

constexpr bool headlessStereoRigVisualTrialAuthorized(
    const compatibility::RetailCompatibilityProof& proof,
    const HeadlessStereoRigVisualTrialRequest& request) noexcept
{
    return headlessStereoRigVisualTrialRequestIsNarrow(request)
        && retailObservationAuthorized(proof);
}
}
