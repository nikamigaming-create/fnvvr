#pragma once

#include "../protocol/fnvxr_shared_state.h"

#include <cstdint>

namespace fnvxr::host::cpu_engine_presentation
{
// The ordinary-D3D9 CPU transport carries both a true binocular world pair
// and an explicitly flat menu frame.  Keep this small policy independent of
// D3D/OpenXR objects so the transition contract is executable in a headless
// test.  It deliberately authorizes presentation resources only; it has no
// controller, player-body, weapon, or gameplay-mutation authority.
struct RuntimeSample
{
    std::uint64_t sample = 0u;
    std::uint32_t phase = shared::RuntimePhaseUnknown;
    std::uint32_t menuBits = 0u;
    std::uint32_t showroomActive = 0u;
    bool cameraActive = false;
    bool fresh = false;
};

struct FrameIdentity
{
    std::uint64_t transactionId = 0u;
    std::uint64_t sourceFrame = 0u;
    std::uint64_t runtimeStateSample = 0u;
    std::uint32_t producerMode = shared::StereoProducerUnknown;
    bool separated = false;
    bool worldCandidate = false;
    bool uiActive = false;
    bool pixelsComplete = false;
};

struct UiBoundary
{
    std::uint64_t transactionId = 0u;
    std::uint64_t sourceFrame = 0u;
};

constexpr bool runtimeUiConfirmed(const RuntimeSample& runtime) noexcept
{
    return runtime.fresh
        && runtime.sample != 0u
        && runtime.phase != shared::RuntimePhaseUnknown
        && !shared::runtimeGameplayPhase(
            runtime.phase,
            runtime.menuBits,
            runtime.showroomActive);
}

constexpr bool runtimeGameplayConfirmed(const RuntimeSample& runtime) noexcept
{
    return runtime.fresh
        && runtime.sample != 0u
        && runtime.cameraActive
        && shared::runtimeGameplayPhase(
            runtime.phase,
            runtime.menuBits,
            runtime.showroomActive);
}

constexpr bool identityComplete(const FrameIdentity& frame) noexcept
{
    return frame.transactionId != 0u
        && frame.sourceFrame != 0u
        && frame.runtimeStateSample != 0u;
}

constexpr bool flatUiBoundaryValid(const FrameIdentity& frame) noexcept
{
    return identityComplete(frame)
        && frame.producerMode == shared::StereoProducerMonoUiQuad
        && !frame.separated
        && !frame.worldCandidate
        && frame.uiActive;
}

constexpr bool flatUiFrameEligible(
    const FrameIdentity& frame,
    const RuntimeSample& runtime) noexcept
{
    return flatUiBoundaryValid(frame)
        && frame.pixelsComplete
        && runtimeUiConfirmed(runtime)
        && frame.runtimeStateSample == runtime.sample;
}

constexpr UiBoundary boundaryFromUi(
    const FrameIdentity& frame) noexcept
{
    return { frame.transactionId, frame.sourceFrame };
}

constexpr bool worldFollowsUiBoundary(
    const FrameIdentity& frame,
    const UiBoundary& boundary) noexcept
{
    return boundary.transactionId == 0u
        || (frame.transactionId > boundary.transactionId
            && frame.sourceFrame > boundary.sourceFrame);
}

constexpr bool binocularWorldFrameEligible(
    const FrameIdentity& frame,
    const RuntimeSample& runtime,
    const UiBoundary& lastUiBoundary) noexcept
{
    return identityComplete(frame)
        && frame.producerMode == shared::StereoProducerEngineCenter
        && frame.separated
        && frame.worldCandidate
        && !frame.uiActive
        && frame.pixelsComplete
        && runtimeGameplayConfirmed(runtime)
        && frame.runtimeStateSample == runtime.sample
        && worldFollowsUiBoundary(frame, lastUiBoundary);
}
}
