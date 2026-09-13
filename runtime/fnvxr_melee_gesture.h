#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fnvxr::melee
{
enum class Strike { None, Normal, Power };
struct Motion
{
    bool allowed{};
    std::uint64_t epoch{}, timeMs{};
    std::uint32_t space{}, weapon{}, cell{};
    // Metres in tracking space, with common head translation removed. The
    // adapter supplies the held weapon's motion point, never camera rotation.
    std::array<float, 3> point{};
};
struct Result { Strike strike{}; float travel{}, peakSpeed{}; };

class Gesture
{
public:
    void reset() noexcept { *this = {}; }
    Result sample(const Motion& m) noexcept
    {
        Result result{};
        if (!m.allowed || !m.epoch || !m.space || !m.cell || !m.timeMs
            || !std::all_of(m.point.begin(), m.point.end(), [](float f) { return std::isfinite(f); }))
        { reset(); return result; }
        if (!previous_.allowed || previous_.epoch != m.epoch || previous_.space != m.space
            || previous_.weapon != m.weapon || previous_.cell != m.cell
            || m.timeMs < previous_.timeMs || m.timeMs - previous_.timeMs > 100)
        { reset(); previous_ = m; return result; }
        if (m.timeMs == previous_.timeMs) return result;
        float distanceSquared = 0;
        for (unsigned i = 0; i < 3; ++i)
            distanceSquared += (m.point[i] - previous_.point[i]) * (m.point[i] - previous_.point[i]);
        const float distance = std::sqrt(distanceSquared);
        const float speed = distance * 1000.0f / static_cast<float>(m.timeMs - previous_.timeMs);
        previous_ = m;
        // A tracking jump must neither attack nor immediately rearm.
        if (speed > 8.0f) { reset(); previous_ = m; return result; }
        if (speed < 0.25f)
        {
            if (!quietSince_) quietSince_ = m.timeMs;
        }
        else quietSince_ = 0;
        if (!armed_)
        {
            if (quietSince_ && m.timeMs - quietSince_ >= 180 && m.timeMs >= cooldownUntil_)
                armed_ = true;
            return result;
        }
        if (!moving_)
        {
            if (speed < 0.65f) return result;
            moving_ = true; started_ = m.timeMs; travel_ = 0; peak_ = 0;
        }
        travel_ += distance;
        peak_ = (std::max)(peak_, speed);
        const bool power = travel_ >= 0.38f && peak_ >= 1.4f;
        const bool ended = quietSince_ && m.timeMs - quietSince_ >= 45;
        const bool expired = m.timeMs - started_ >= 650;
        if (!power && !ended && !expired) return result;
        if (power || (travel_ >= 0.12f && peak_ >= 0.65f))
            result = { power ? Strike::Power : Strike::Normal, travel_, peak_ };
        moving_ = false; armed_ = false; quietSince_ = 0;
        cooldownUntil_ = m.timeMs + 350;
        return result;
    }
private:
    Motion previous_{};
    bool armed_{}, moving_{};
    float travel_{}, peak_{};
    std::uint64_t quietSince_{}, started_{}, cooldownUntil_{};
};

// Button and motion inputs meet at the native attack control. Retail still
// chooses animations, attack speed, AP cost, perks, blocking and damage.
class AttackHold
{
public:
    void reset() noexcept { until_ = 0; last_ = 0; }
    bool update(bool allowed, bool manual, Strike strike, std::uint64_t nowMs) noexcept
    {
        if (!allowed || (last_ && (nowMs < last_ || nowMs - last_ > 125))) reset();
        if (!allowed) return false;
        last_ = nowMs;
        if (manual) { until_ = 0; return true; }
        if (strike != Strike::None && nowMs >= until_)
            until_ = nowMs + (strike == Strike::Power ? 650u : 80u);
        return nowMs < until_;
    }
private:
    std::uint64_t until_{}, last_{};
};
}
