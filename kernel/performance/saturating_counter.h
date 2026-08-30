#pragma once

#include <cstdint>
#include <limits>

namespace fnvxr::kernel::performance
{
class SaturatingCounter final
{
public:
    constexpr explicit SaturatingCounter(std::uint64_t initial = 0) noexcept
        : value_(initial)
    {
    }

    [[nodiscard]] constexpr bool increment() noexcept
    {
        if (value_ == std::numeric_limits<std::uint64_t>::max())
            return false;
        ++value_;
        return true;
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    constexpr void reset() noexcept { value_ = 0; }

private:
    std::uint64_t value_ = 0;
};
}
