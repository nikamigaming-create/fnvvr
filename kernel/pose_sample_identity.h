#pragma once

#include <cstdint>

namespace fnvxr::kernel
{
using OpenXrTime = std::int64_t;

struct PoseDomain final
{
    std::uint64_t producerEpoch = 0;
    std::uint64_t referenceSpaceGeneration = 0;
};

struct PoseSampleIdentity final
{
    std::uint64_t producerEpoch = 0;
    std::uint64_t referenceSpaceGeneration = 0;
    std::uint64_t poseSequence = 0;
    OpenXrTime displayTime = 0;
};

[[nodiscard]] constexpr bool operator==(PoseDomain lhs, PoseDomain rhs) noexcept
{
    return lhs.producerEpoch == rhs.producerEpoch &&
        lhs.referenceSpaceGeneration == rhs.referenceSpaceGeneration;
}

[[nodiscard]] constexpr bool operator==(PoseSampleIdentity lhs, PoseSampleIdentity rhs) noexcept
{
    return lhs.producerEpoch == rhs.producerEpoch &&
        lhs.referenceSpaceGeneration == rhs.referenceSpaceGeneration &&
        lhs.poseSequence == rhs.poseSequence && lhs.displayTime == rhs.displayTime;
}

[[nodiscard]] constexpr bool operator!=(PoseDomain lhs, PoseDomain rhs) noexcept
{
    return !(lhs == rhs);
}

[[nodiscard]] constexpr bool operator!=(PoseSampleIdentity lhs, PoseSampleIdentity rhs) noexcept
{
    return !(lhs == rhs);
}

[[nodiscard]] constexpr bool isValid(PoseDomain domain) noexcept
{
    return domain.producerEpoch != 0 && domain.referenceSpaceGeneration != 0;
}

[[nodiscard]] constexpr bool isValid(PoseSampleIdentity identity) noexcept
{
    return isValid(PoseDomain {
               identity.producerEpoch, identity.referenceSpaceGeneration }) &&
        identity.poseSequence != 0 && identity.displayTime != 0;
}

[[nodiscard]] constexpr PoseDomain domainOf(PoseSampleIdentity identity) noexcept
{
    return { identity.producerEpoch, identity.referenceSpaceGeneration };
}

[[nodiscard]] constexpr bool isStrictlyNewer(
    PoseSampleIdentity candidate,
    PoseSampleIdentity boundary) noexcept
{
    return isValid(candidate) && isValid(boundary) &&
        domainOf(candidate) == domainOf(boundary) &&
        candidate.poseSequence > boundary.poseSequence &&
        candidate.displayTime > boundary.displayTime;
}
}
