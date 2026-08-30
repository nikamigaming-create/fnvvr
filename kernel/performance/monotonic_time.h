#pragma once

#include <cstdint>

namespace fnvxr::kernel::performance
{
struct MonotonicNanoseconds final
{
    std::uint64_t value = 0;
};

struct DurationNanoseconds final
{
    std::uint64_t value = 0;
};

[[nodiscard]] constexpr bool operator<(
    MonotonicNanoseconds lhs,
    MonotonicNanoseconds rhs) noexcept
{
    return lhs.value < rhs.value;
}

[[nodiscard]] constexpr DurationNanoseconds elapsed(
    MonotonicNanoseconds begin,
    MonotonicNanoseconds end) noexcept
{
    return { end.value - begin.value };
}
}
