#pragma once

#include "frame_math.h"

#include <array>
#include <cstdint>
#include <type_traits>

namespace fnvxr::kernel
{
enum class ClockDomain : std::uint8_t
{
    Unknown,
    OpenXr,
};

struct Timestamp final
{
    std::int64_t nanoseconds = 0;
    ClockDomain domain = ClockDomain::Unknown;
};

struct PoseViewFrameData final
{
    Timestamp sampleTime {};
    Pose head {};
    std::array<Pose, 2> grips {};
    std::array<Pose, 2> aims {};
    std::array<View, 2> views {};
    std::uint32_t trackingFlags = 0;
};

static_assert(std::is_trivially_copyable_v<PoseViewFrameData>);
static_assert(std::is_standard_layout_v<PoseViewFrameData>);

// An owned value snapshot. Its contents cannot be mutated after construction.
class PoseViewFrame final
{
public:
    PoseViewFrame() noexcept = default;
    explicit PoseViewFrame(const PoseViewFrameData& data) noexcept : data_(data) {}
    PoseViewFrame(const PoseViewFrame&) noexcept = default;
    PoseViewFrame(PoseViewFrame&&) noexcept = default;
    PoseViewFrame& operator=(const PoseViewFrame&) = delete;
    PoseViewFrame& operator=(PoseViewFrame&&) = delete;

    [[nodiscard]] const PoseViewFrameData& data() const noexcept { return data_; }

private:
    PoseViewFrameData data_ {};
};
}
