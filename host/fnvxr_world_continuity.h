#pragma once

#include <cstdint>

namespace fnvxr::host
{
// Retain only an already submitted, privately owned binocular pair. This is
// a presentation hold, never permission to advance simulation or admit pixels
// with expired poses. Native cell loads can stop the runtime publisher too.
struct WorldContinuityInput
{
    bool lifetimeMatches = false;
    bool currentWorldReady = false;
    bool runtimeFresh = false;
    bool worldContext = false;
    bool menuActive = false;
    std::int64_t displayTime = 0;
    std::int64_t submittedDisplayTime = 0;
    std::int64_t lastMenuDisplayTime = 0;
    // Native arm pixels and their device mount are one presentation unit.
    // A fresh image without its matching mount cannot replace that unit.
    bool currentRigCalibrationReady = true;
};

constexpr bool retainSubmittedWorld(const WorldContinuityInput& input) noexcept
{
    if (!input.lifetimeMatches
        || (input.currentWorldReady && input.currentRigCalibrationReady)
        || input.submittedDisplayTime <= 0
        || input.displayTime < input.submittedDisplayTime
        || (input.runtimeFresh && !input.worldContext))
        return false;
    if (input.runtimeFresh && input.menuActive)
        return true;
    // Bounded producer suspension, including the gap before Loading is
    // published and the first pose/texture join in the new cell.
    if (input.displayTime - input.submittedDisplayTime <= 30000000000LL)
        return true;
    return input.lastMenuDisplayTime > 0
        && input.displayTime >= input.lastMenuDisplayTime
        && input.displayTime - input.lastMenuDisplayTime <= 1000000000LL;
}
}
