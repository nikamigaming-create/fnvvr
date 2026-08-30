#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::kernel
{
struct SpatialCalibrationIdentity final
{
    std::uint64_t producerEpoch = 0;
    std::uint32_t referenceSpaceGeneration = 0;
    std::uint64_t poseSequence = 0;

    friend constexpr bool operator==(
        SpatialCalibrationIdentity left,
        SpatialCalibrationIdentity right) noexcept
    {
        return left.producerEpoch == right.producerEpoch
            && left.referenceSpaceGeneration
                == right.referenceSpaceGeneration
            && left.poseSequence == right.poseSequence;
    }
};

constexpr bool isValid(SpatialCalibrationIdentity identity) noexcept
{
    return identity.producerEpoch != 0
        && identity.referenceSpaceGeneration != 0
        && identity.poseSequence != 0;
}

// Fixed-memory, exact-identity calibration storage. Lookup and publication are
// O(1); a slot collision evicts the older calibration and therefore fails
// closed for an in-flight frame that has exceeded the supported history.
template <typename Value, std::size_t Capacity = 128>
class SpatialCalibrationHistory final
{
    static_assert(Capacity > 0);

public:
    void clear() noexcept
    {
        entries_ = {};
    }

    [[nodiscard]] bool record(
        SpatialCalibrationIdentity identity,
        const Value& value) noexcept
    {
        if (!isValid(identity))
            return false;
        entries_[index(identity)] = { identity, value, true };
        return true;
    }

    [[nodiscard]] bool find(
        SpatialCalibrationIdentity identity,
        Value& found) const noexcept
    {
        found = {};
        if (!isValid(identity))
            return false;
        const Entry& candidate = entries_[index(identity)];
        if (!candidate.occupied || !(candidate.identity == identity))
            return false;
        found = candidate.value;
        return true;
    }

private:
    struct Entry final
    {
        SpatialCalibrationIdentity identity {};
        Value value {};
        bool occupied = false;
    };

    static constexpr std::size_t index(
        SpatialCalibrationIdentity identity) noexcept
    {
        return static_cast<std::size_t>(identity.poseSequence % Capacity);
    }

    std::array<Entry, Capacity> entries_ {};
};
}
