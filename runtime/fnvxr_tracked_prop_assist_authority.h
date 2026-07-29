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
}
