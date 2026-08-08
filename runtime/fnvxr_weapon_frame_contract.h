#pragma once

#include <cmath>
#include <cstdint>

namespace fnvxr::weapon_frame
{
enum class Failure : std::uint32_t
{
    None = 0,
    Unavailable = 1,
    NotCommitted = 2,
    PoseMismatch = 3,
    Incomplete = 4,
    MissingNodes = 5,
    TransformOverwritten = 6,
};

constexpr Failure validateIdentity(
    std::uint32_t status,
    std::uint32_t committedStatus,
    std::uint32_t flags,
    std::uint32_t requiredFlags,
    std::uint32_t committedPoseSequence,
    std::uint64_t committedPoseFrame,
    std::uint32_t renderedPoseSequence,
    std::uint64_t renderedPoseFrame,
    std::uint32_t rightHandAddress,
    std::uint32_t weaponAddress) noexcept
{
    if (status != committedStatus)
        return Failure::NotCommitted;
    if (committedPoseSequence != renderedPoseSequence
        || committedPoseFrame != renderedPoseFrame)
        return Failure::PoseMismatch;
    if ((flags & requiredFlags) != requiredFlags)
        return Failure::Incomplete;
    if (!rightHandAddress || !weaponAddress)
        return Failure::MissingNodes;
    return Failure::None;
}

inline bool transformMatches(
    const float* live,
    const float* committed,
    int count,
    float tolerance) noexcept
{
    if (!live || !committed || count <= 0 || tolerance < 0.0f)
        return false;
    for (int i = 0; i < count; ++i)
    {
        if (!std::isfinite(live[i])
            || !std::isfinite(committed[i])
            || std::fabs(live[i] - committed[i]) > tolerance)
            return false;
    }
    return true;
}
}
