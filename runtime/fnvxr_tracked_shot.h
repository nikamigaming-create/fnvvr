#pragma once

#include <cmath>
#include <cstdint>

namespace fnvxr::tracked_shot
{
struct Pose
{
    bool valid {};
    std::uintptr_t actor {};
    std::uintptr_t weapon {};
    std::uint32_t cell {};
    std::uint64_t epoch {};
    std::uint32_t referenceSpace {};
    std::uint32_t sequence {};
    std::uint64_t frame {};
    std::uint64_t sampledAtMs {};
    float position[3] {};
    float forward[3] {};
};

enum class Decision { Stock, Tracked, Unavailable };

inline bool current(const Pose& pose, std::uintptr_t actor, std::uintptr_t weapon,
    std::uint32_t cell, std::uint64_t epoch, std::uint32_t referenceSpace,
    std::uint64_t frame, std::uint64_t nowMs, bool tracking) noexcept
{
    if (!tracking || !pose.valid || !actor || !weapon || !cell || !epoch
        || !referenceSpace || !pose.sequence || pose.actor != actor
        || pose.weapon != weapon || pose.cell != cell || pose.epoch != epoch
        || pose.referenceSpace != referenceSpace || frame < pose.frame
        || frame - pose.frame > 6u || nowMs < pose.sampledAtMs
        || nowMs - pose.sampledAtMs > 100u)
        return false;
    float lengthSquared = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (!std::isfinite(pose.position[i]) || !std::isfinite(pose.forward[i]))
            return false;
        lengthSquared += pose.forward[i] * pose.forward[i];
    }
    return std::fabs(lengthSquared - 1.0f) < 0.01f;
}

// Gamebryo's world basis is +Y forward, +Z up. Native projectile pitch is
// positive downward; yaw increases from +Y toward +X. Spread is applied later.
inline void angles(const Pose& pose, float& pitch, float& yaw) noexcept
{
    const float horizontal = std::sqrt(pose.forward[0] * pose.forward[0]
        + pose.forward[1] * pose.forward[1]);
    pitch = std::atan2(-pose.forward[2], horizontal);
    yaw = std::atan2(pose.forward[0], pose.forward[1]);
}
}
