#pragma once

#include "source_key.h"

#include <cstdint>

namespace fnvxr::kernel::presentation
{
enum class PresentationMode : std::uint8_t
{
    SafetyBlank,
    UiQuad,
    WorldStereo,
};

enum class DecisionReason : std::uint8_t
{
    RuntimeUnavailable,
    UiFrameReady,
    UiFrameHeld,
    UiFrameIncomplete,
    UiRuntimeLineageInvalid,
    UiGpuEvidenceInvalid,
    UiSourceOlderThanWorld,
    UiSourceRegression,
    AwaitingNewerWorldHeld,
    HoldExpired,
    WorldFrameIncomplete,
    WorldStereoIdentityInvalid,
    WorldRuntimeLineageInvalid,
    WorldGpuEvidenceInvalid,
    WorldRetailPrerequisitesInvalid,
    WorldPoseJoinMissing,
    WorldNotNewerThanUi,
    WorldNotNewerThanAccepted,
    WorldFrameReady,
};

struct PresentationDecision
{
    PresentationMode mode = PresentationMode::SafetyBlank;
    DecisionReason reason = DecisionReason::RuntimeUnavailable;
    SourceKey selectedSource {};
    bool spatialRigMayRender = false;
    bool wristScreenMayRender = false;
    bool pointerMayRender = false;
    bool transitionHold = false;
    SourceKey overlayUiSource {};
};
}
