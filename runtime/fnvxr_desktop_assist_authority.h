#pragma once

#include "fnvxr_retail_observation_authority.h"

namespace fnvxr::engine
{
// This is intentionally narrower than RetailMutationEvidenceToken.  It can
// authorize only the reversible, camera-local desktop assist experiment; it
// never authorizes D3D/OpenXR presentation, actor/body writes, input injection,
// weapon/rig mutation, or the production stereo path.
struct DesktopAssistCameraRequest
{
    bool desktopAssistProfile = false;
    bool cameraOnlyRequested = false;
    bool cameraPoseApplicationRequested = false;
    bool appliesLocalRotation = false;
    bool yawPitchRollEnabled = false;
    bool writesWorldTransform = false;
    bool callsUnverifiedTransformUpdate = false;
    bool appliesLocalTranslation = false;
};

constexpr bool desktopAssistCameraRequestIsNarrow(
    const DesktopAssistCameraRequest& request) noexcept
{
    // The first desktop transaction is deliberately rotation-only.  It gives
    // the assist harness a way to prove that the player body remains still
    // while the camera follows head yaw/pitch/roll before calibration enables
    // any local translation.
    return request.desktopAssistProfile
        && request.cameraOnlyRequested
        && request.cameraPoseApplicationRequested
        && request.appliesLocalRotation
        && request.yawPitchRollEnabled
        && !request.writesWorldTransform
        && !request.callsUnverifiedTransformUpdate
        && !request.appliesLocalTranslation;
}

constexpr bool desktopAssistCameraAuthorized(
    const compatibility::RetailCompatibilityProof& proof,
    const DesktopAssistCameraRequest& request) noexcept
{
    return desktopAssistCameraRequestIsNarrow(request)
        && retailObservationAuthorized(proof);
}
}
