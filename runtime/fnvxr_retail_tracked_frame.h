#pragma once

#include "../protocol/fnvxr_shared_state.h"
#include "fnvxr_live_pipboy_contract.h"

#include <cmath>
#include <cstdint>

namespace fnvxr::engine
{
enum class RetailTrackedFrameFailure : std::uint8_t
{
    None = 0u,
    PosePublicationInvalid,
    RuntimePublicationInvalid,
    PoseHeaderInvalid,
    RuntimeHeaderInvalid,
    PoseIdentityInvalid,
    HmdTrackingUnavailable,
    PoseNumbersInvalid,
    EyeFovInvalid,
    RuntimeNotUi,
    RuntimeNotGameplay,
};

struct RetailTrackedFrame
{
    shared::SharedVrPoseState pose {};
    shared::SharedRuntimeState runtime {};
    LONG poseSequence = 0;
    LONG runtimeSequence = 0;
};

struct RetailTrackedFrameValidation
{
    RetailTrackedFrameFailure failure =
        RetailTrackedFrameFailure::PosePublicationInvalid;

    constexpr bool complete() const noexcept
    {
        return failure == RetailTrackedFrameFailure::None;
    }
};

// A presentation decision must have one owner. In particular, menu
// transitions must never leave a frame eligible for both the flat UI quad and
// the binocular world renderer. Any stable frame that is neither confirmed
// UI nor confirmed gameplay is withheld rather than guessed at.
enum class RetailTrackedPresentationRoute : std::uint8_t
{
    Withhold = 0u,
    MonoUiQuad,
    BinocularWorld,
};

inline bool retailTrackedFinite(const float* values, std::size_t count) noexcept
{
    if (!values)
        return false;
    for (std::size_t index = 0u; index < count; ++index)
    {
        if (!std::isfinite(values[index]))
            return false;
    }
    return true;
}

inline bool retailTrackedQuaternionValid(const float value[4]) noexcept
{
    if (!retailTrackedFinite(value, 4u))
        return false;
    const float lengthSquared = value[0] * value[0]
        + value[1] * value[1]
        + value[2] * value[2]
        + value[3] * value[3];
    return std::isfinite(lengthSquared)
        && lengthSquared >= 0.25f
        && lengthSquared <= 4.0f;
}

inline bool retailTrackedPositionValid(const float value[3]) noexcept
{
    constexpr float MaximumTrackedPositionMeters = 100.0f;
    return retailTrackedFinite(value, 3u)
        && std::fabs(value[0]) <= MaximumTrackedPositionMeters
        && std::fabs(value[1]) <= MaximumTrackedPositionMeters
        && std::fabs(value[2]) <= MaximumTrackedPositionMeters;
}

// OpenXR order is angleLeft, angleRight, angleUp, angleDown.
inline bool retailTrackedFovValid(const float value[4]) noexcept
{
    constexpr float HalfPi = 1.57079632679489661923f;
    return retailTrackedFinite(value, 4u)
        && value[0] > -HalfPi
        && value[0] < HalfPi
        && value[1] > -HalfPi
        && value[1] < HalfPi
        && value[2] > -HalfPi
        && value[2] < HalfPi
        && value[3] > -HalfPi
        && value[3] < HalfPi
        && value[0] < value[1]
        && value[3] < value[2];
}

inline RetailTrackedFrameValidation validateRetailTrackedPublishedFrame(
    const RetailTrackedFrame& frame) noexcept
{
    if (!shared::sequencedValueIsPublished(frame.poseSequence))
    {
        return { RetailTrackedFrameFailure::PosePublicationInvalid };
    }
    if (!shared::sequencedValueIsPublished(frame.runtimeSequence))
    {
        return { RetailTrackedFrameFailure::RuntimePublicationInvalid };
    }
    if (frame.pose.magic != shared::VrPoseSharedMagic
        || frame.pose.version != shared::VrPoseSharedVersion)
    {
        return { RetailTrackedFrameFailure::PoseHeaderInvalid };
    }
    if (frame.runtime.magic != shared::RuntimeSharedMagic
        || frame.runtime.version != shared::RuntimeSharedVersion)
    {
        return { RetailTrackedFrameFailure::RuntimeHeaderInvalid };
    }
    if (frame.pose.frame == 0u
        || frame.pose.predictedDisplayTime == 0
        || frame.pose.referenceSpaceGeneration == 0u
        || frame.pose.producerEpoch == 0u
        || frame.runtime.frame == 0u)
    {
        return { RetailTrackedFrameFailure::PoseIdentityInvalid };
    }
    if ((frame.pose.trackingFlags & shared::VrPoseTrackingHmd) == 0u)
    {
        return { RetailTrackedFrameFailure::HmdTrackingUnavailable };
    }
    if (!retailTrackedQuaternionValid(frame.pose.hmdRot)
        || !retailTrackedQuaternionValid(frame.pose.leftEyeRot)
        || !retailTrackedQuaternionValid(frame.pose.rightEyeRot)
        || !retailTrackedPositionValid(frame.pose.hmdPos)
        || !retailTrackedPositionValid(frame.pose.leftEyePos)
        || !retailTrackedPositionValid(frame.pose.rightEyePos))
    {
        return { RetailTrackedFrameFailure::PoseNumbersInvalid };
    }
    if (!retailTrackedFovValid(frame.pose.leftFov)
        || !retailTrackedFovValid(frame.pose.rightFov))
    {
        return { RetailTrackedFrameFailure::EyeFovInvalid };
    }
    return { RetailTrackedFrameFailure::None };
}

inline bool retailTrackedRuntimeUiConfirmed(
    const shared::SharedRuntimeState& runtime) noexcept
{
    if (shared::runtimeGameplayPhase(
            runtime.phase,
            runtime.menuBits,
            runtime.showroomActive))
    {
        return false;
    }
    return runtime.phase == shared::RuntimePhaseMenu
        || runtime.phase == shared::RuntimePhaseLoading
        || (runtime.menuBits
                & (shared::RuntimeInteractiveMenuBits
                    | shared::RuntimeLoadingMenuBit))
            != 0u;
}

// Call only after validateRetailTrackedPublishedFrame() succeeds. Keeping the
// classification separate lets both the world hook and the UI-present path
// consume the same mutually-exclusive runtime decision.
inline RetailTrackedPresentationRoute retailTrackedPublishedPresentationRoute(
    const RetailTrackedFrame& frame) noexcept
{
    if (live_pipboy::worldPresentationContinues(
            frame.runtime.phase,
            frame.runtime.menuBits,
            frame.runtime.showroomActive,
            frame.runtime.cameraActive != 0u))
    {
        return RetailTrackedPresentationRoute::BinocularWorld;
    }
    if (retailTrackedRuntimeUiConfirmed(frame.runtime))
        return RetailTrackedPresentationRoute::MonoUiQuad;
    if (shared::runtimeGameplayPhase(
            frame.runtime.phase,
            frame.runtime.menuBits,
            frame.runtime.showroomActive)
        && frame.runtime.cameraActive != 0u)
    {
        return RetailTrackedPresentationRoute::BinocularWorld;
    }
    return RetailTrackedPresentationRoute::Withhold;
}

inline RetailTrackedPresentationRoute retailTrackedPresentationRoute(
    const RetailTrackedFrame& frame) noexcept
{
    if (!validateRetailTrackedPublishedFrame(frame).complete())
        return RetailTrackedPresentationRoute::Withhold;
    return retailTrackedPublishedPresentationRoute(frame);
}

inline RetailTrackedFrameValidation validateRetailTrackedUiFrame(
    const RetailTrackedFrame& frame) noexcept
{
    const RetailTrackedFrameValidation published =
        validateRetailTrackedPublishedFrame(frame);
    if (!published.complete())
        return published;
    if (retailTrackedPublishedPresentationRoute(frame)
        != RetailTrackedPresentationRoute::MonoUiQuad)
        return { RetailTrackedFrameFailure::RuntimeNotUi };
    return { RetailTrackedFrameFailure::None };
}

inline RetailTrackedFrameValidation validateRetailTrackedGameplayFrame(
    const RetailTrackedFrame& frame) noexcept
{
    const RetailTrackedFrameValidation published =
        validateRetailTrackedPublishedFrame(frame);
    if (!published.complete())
        return published;
    if (retailTrackedPublishedPresentationRoute(frame)
        != RetailTrackedPresentationRoute::BinocularWorld)
    {
        return { RetailTrackedFrameFailure::RuntimeNotGameplay };
    }
    return { RetailTrackedFrameFailure::None };
}
}
