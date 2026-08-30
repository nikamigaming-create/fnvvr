#pragma once

#include "../runtime_config.h"
#include "pose_snapshot.h"

#include <optional>

namespace fnvxr::kernel::wrist
{
struct WristSurface final
{
    Pose pose {};
    float widthMeters = 0.0F;
    float heightMeters = 0.0F;
    bool calibrated = false;
};

[[nodiscard]] Pose fallbackGripToScreenPose() noexcept;

[[nodiscard]] std::optional<WristSurface> placeWristSurface(
    const WristUiConfig& config,
    const PoseSnapshot& leftGrip,
    const std::optional<LocalScreenTransform>& calibration = std::nullopt,
    float scale = 1.0F) noexcept;
}
