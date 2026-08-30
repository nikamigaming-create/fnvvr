#pragma once

#include <cstdint>

namespace fnvxr::host
{
using TrackingTime = std::int64_t;

struct TrackingGenerationChange final
{
    bool activated = false;
    std::uint32_t generation = 0;
    TrackingTime scheduledTime = 0;
};

// Owns the LOCAL-space identity boundary. Consumers reset from one returned
// transition instead of independently interpreting session/reference events.
class TrackingLifetime final
{
public:
    void scheduleReferenceSpaceChange(TrackingTime changeTime) noexcept
    {
        pending_ = true;
        changeTime_ = changeTime;
    }

    [[nodiscard]] TrackingGenerationChange sessionRegained() noexcept
    {
        const TrackingTime scheduled = changeTime_;
        clearPending();
        return activate(scheduled);
    }

    [[nodiscard]] TrackingGenerationChange advance(
        TrackingTime predictedDisplayTime) noexcept
    {
        if (!pending_ || (changeTime_ > 0
                && predictedDisplayTime < changeTime_))
        {
            return { false, generation_, changeTime_ };
        }
        const TrackingTime scheduled = changeTime_;
        clearPending();
        return activate(scheduled);
    }

    [[nodiscard]] std::uint32_t generation() const noexcept
    {
        return generation_;
    }

    [[nodiscard]] bool pending() const noexcept { return pending_; }
    [[nodiscard]] TrackingTime changeTime() const noexcept { return changeTime_; }

private:
    [[nodiscard]] TrackingGenerationChange activate(
        TrackingTime scheduled) noexcept
    {
        ++generation_;
        if (generation_ == 0)
            generation_ = 1;
        return { true, generation_, scheduled };
    }

    void clearPending() noexcept
    {
        pending_ = false;
        changeTime_ = 0;
    }

    std::uint32_t generation_ = 1;
    bool pending_ = false;
    TrackingTime changeTime_ = 0;
};
}
