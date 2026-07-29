#pragma once

#include <cstdint>

namespace fnvxr::host::headset_mirror
{
// Captures are scheduled in whole stereo pairs.  The host asks this policy
// once per eye, so repeated requests for the same OpenXR frame must reuse the
// exact ordinal rather than consuming two entries from the bounded capture.
struct ScheduleState
{
    std::uint64_t firstEligibleFrame = 0u;
    std::uint64_t activeFrame = 0u;
    std::uint32_t activeOrdinal = 0u;
    std::uint32_t scheduledPairs = 0u;
};

struct ScheduleRequest
{
    bool enabled = false;
    std::uint64_t frame = 0u;
    std::uint32_t everyFrames = 1u;
    std::uint32_t maximumPairs = 0u;
};

struct ScheduleDecision
{
    bool capture = false;
    std::uint32_t ordinal = 0u;
};

constexpr ScheduleDecision schedule(
    ScheduleState& state,
    const ScheduleRequest& request) noexcept
{
    if (!request.enabled
        || request.frame == 0u
        || request.everyFrames == 0u
        || request.maximumPairs == 0u)
    {
        return {};
    }

    if (state.activeFrame == request.frame && state.activeOrdinal != 0u)
        return { true, state.activeOrdinal };

    if (state.scheduledPairs >= request.maximumPairs)
        return {};

    if (state.firstEligibleFrame == 0u)
        state.firstEligibleFrame = request.frame;

    if ((request.frame - state.firstEligibleFrame) % request.everyFrames != 0u)
        return {};

    ++state.scheduledPairs;
    state.activeFrame = request.frame;
    state.activeOrdinal = state.scheduledPairs;
    return { true, state.activeOrdinal };
}
}
