#pragma once
#include "fnvxr_tracked_shot.h"
#include <array>
#include <cmath>

namespace fnvxr::throw_motion
{
struct Release
{
    tracked_shot::Pose pose {};
    std::array<float, 3> velocity {}; // game units per second
};

class History
{
public:
    void reset() noexcept { count_ = 0; }
    void sample(const tracked_shot::Pose& pose, std::uint64_t timeMs) noexcept
    {
        if (!pose.valid || !timeMs) { reset(); return; }
        for (float component : pose.position)
            if (!std::isfinite(component)) { reset(); return; }
        if (count_)
        {
            const auto& prior = samples_[(count_ - 1) % samples_.size()];
            if (prior.pose.actor != pose.actor || prior.pose.weapon != pose.weapon
                || prior.pose.cell != pose.cell || prior.pose.epoch != pose.epoch
                || prior.pose.referenceSpace != pose.referenceSpace
                || timeMs < prior.timeMs || timeMs - prior.timeMs > 100)
                reset();
            else if (timeMs == prior.timeMs) return;
            else
            {
                float distance2 = 0;
                for (unsigned i = 0; i < 3; ++i)
                    distance2 += std::pow((pose.position[i] - prior.pose.position[i]) / 70.0f, 2.0f);
                const float speed = std::sqrt(distance2) * 1000.0f / float(timeMs - prior.timeMs);
                if (!std::isfinite(speed) || speed > 20.0f) { reset(); return; }
            }
        }
        samples_[count_++ % samples_.size()] = {pose, timeMs};
    }
    Release release(std::uint64_t nowMs) const noexcept
    {
        Release result {};
        if (count_ < 2) return result;
        const auto& last = samples_[(count_ - 1) % samples_.size()];
        if (nowMs < last.pose.sampledAtMs || nowMs - last.pose.sampledAtMs > 100) return result;
        // Fit the last 80 ms of committed hand positions. Repeated render
        // samples do not count twice and a pause/recenter cannot become speed.
        double sumT = 0, sumTT = 0, sumP[3] {}, sumTP[3] {};
        unsigned used = 0;
        std::uint64_t span = 0;
        for (unsigned n = 0; n < count_ && n < samples_.size(); ++n)
        {
            const auto& s = samples_[(count_ - 1 - n) % samples_.size()];
            span = last.timeMs - s.timeMs;
            if (span > 80) break;
            const double t = -double(span) / 1000.0;
            sumT += t; sumTT += t * t; ++used;
            for (unsigned i = 0; i < 3; ++i)
            {
                const double p = double(s.pose.position[i]) - last.pose.position[i];
                sumP[i] += p; sumTP[i] += t * p;
            }
        }
        const double denominator = used * sumTT - sumT * sumT;
        if (used < 2 || denominator < 1.0e-5) return result;
        result.pose = last.pose;
        for (unsigned i = 0; i < 3; ++i)
            result.velocity[i] = float((used * sumTP[i] - sumT * sumP[i]) / denominator);
        return result;
    }
private:
    struct Sample { tracked_shot::Pose pose; std::uint64_t timeMs; };
    std::array<Sample, 16> samples_ {};
    unsigned count_ = 0;
};
}
