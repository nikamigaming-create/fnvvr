#pragma once

#include <cstdint>

namespace fnvxr::kernel::presentation
{
struct SourceKey
{
    std::uint64_t epoch = 0;
    std::uint64_t frame = 0;
    std::uint64_t transaction = 0;
};

constexpr bool operator==(const SourceKey& left, const SourceKey& right)
{
    return left.epoch == right.epoch
        && left.frame == right.frame
        && left.transaction == right.transaction;
}

constexpr bool operator!=(const SourceKey& left, const SourceKey& right)
{
    return !(left == right);
}

constexpr bool isValid(const SourceKey& key)
{
    return key.epoch != 0 && key.frame != 0 && key.transaction != 0;
}

// Epochs are identity domains, not timestamps. Cross-epoch ordering is never
// inferred. The producer must establish a new domain through reset().
constexpr bool isStrictlyNewer(const SourceKey& candidate, const SourceKey& boundary)
{
    return isValid(candidate)
        && isValid(boundary)
        && candidate.epoch == boundary.epoch
        && candidate.frame > boundary.frame
        && candidate.transaction > boundary.transaction;
}

constexpr bool isSameOrNewer(const SourceKey& candidate, const SourceKey& boundary)
{
    return candidate == boundary || isStrictlyNewer(candidate, boundary);
}
}
