#include "placement.h"

#include "geometry.h"

#include <cmath>

namespace fnvxr::kernel::wrist
{
Pose fallbackGripToScreenPose() noexcept
{
    // Four centimetres above the grip origin on the dorsal side of the wrist.
    return { { 0.0F, 0.04F, 0.0F }, {} };
}

std::optional<WristSurface> placeWristSurface(
    const WristUiConfig& config,
    const PoseSnapshot& leftGrip,
    const std::optional<LocalScreenTransform>& calibration,
    float scale) noexcept
{
    if (!leftGrip.tracked()
        || !finitePose(leftGrip.pose())
        || !std::isfinite(config.widthMeters)
        || !std::isfinite(config.heightMeters)
        || config.widthMeters <= 0.0F
        || config.heightMeters <= 0.0F
        || !std::isfinite(scale)
        || scale <= 0.0F)
    {
        return std::nullopt;
    }

    const Pose local = calibration
        ? calibration->gripToScreen()
        : fallbackGripToScreenPose();
    if (!finitePose(local))
        return std::nullopt;

    const Pose world = compose(leftGrip.pose(), local);
    const float width = config.widthMeters * scale;
    const float height = config.heightMeters * scale;
    if (!finitePose(world) || !std::isfinite(width) || !std::isfinite(height))
        return std::nullopt;

    return WristSurface { world, width, height, calibration.has_value() };
}
}
