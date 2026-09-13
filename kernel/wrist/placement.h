#pragma once

#include "../runtime_config.h"
#include "pose_snapshot.h"

#include <optional>
#include <cstdint>

namespace fnvxr::kernel::wrist
{
struct WristSurface final
{
    Pose pose {};
    float widthMeters = 0.0F;
    float heightMeters = 0.0F;
    bool calibrated = false;
};

// Presentation-only enlargement. Tracked poses remain unfiltered; the casing,
// screen and interaction plane all use the same accepted scale.
class DeviceScaleTransition final
{
public:
    [[nodiscard]] float advance(float target, std::int64_t displayTime) noexcept;
    [[nodiscard]] float value() const noexcept { return mScale; }
    void reset() noexcept { *this = {}; }

private:
    float mScale = 1.0F;
    std::int64_t mDisplayTime = 0;
};

[[nodiscard]] Pose fallbackGripToScreenPose() noexcept;

// Scale the complete device around its calibrated forearm attachment point.
[[nodiscard]] std::optional<WristSurface> placeWristSurface(
    const WristUiConfig& config,
    const PoseSnapshot& leftGrip,
    const std::optional<LocalScreenTransform>& calibration = std::nullopt,
    float scale = 1.0F) noexcept;
}
