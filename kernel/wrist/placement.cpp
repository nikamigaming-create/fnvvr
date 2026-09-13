#include "placement.h"

#include "geometry.h"

#include <cmath>
#include <algorithm>

namespace fnvxr::kernel::wrist
{
float DeviceScaleTransition::advance(float target, std::int64_t displayTime) noexcept
{
    if (!std::isfinite(target) || target <= 0.0F || displayTime <= 0
        || (mDisplayTime > 0 && displayTime <= mDisplayTime))
        return mScale;
    if (mDisplayTime > 0 && displayTime > mDisplayTime)
    {
        // A 1.0 -> 1.35 change takes 200 ms. Clamp suspended-frame time so
        // restoring tracking cannot turn an interrupted transition into a pop.
        const float seconds = static_cast<float>(
            std::min<std::int64_t>(displayTime - mDisplayTime, 50000000LL)) * 1.0e-9F;
        const float step = 1.75F * seconds;
        mScale += std::clamp(target - mScale, -step, step);
    }
    mDisplayTime = displayTime;
    return mScale;
}

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

    Pose local = calibration
        ? calibration->gripToScreen()
        : fallbackGripToScreenPose();
    if (!finitePose(local))
        return std::nullopt;

    // The attachment point inside the forearm stays fixed. The controller
    // grip is at the hand: scaling about it slides the casing along the arm.
    const Vec3 pivot = calibration ? calibration->gripLocalScalePivot() : Vec3 {};
    if (!std::isfinite(pivot.x) || !std::isfinite(pivot.y) || !std::isfinite(pivot.z))
        return std::nullopt;
    local.position.x = pivot.x + (local.position.x - pivot.x) * scale;
    local.position.y = pivot.y + (local.position.y - pivot.y) * scale;
    local.position.z = pivot.z + (local.position.z - pivot.z) * scale;
    const Pose world = compose(leftGrip.pose(), local);
    const float width = config.widthMeters * scale;
    const float height = config.heightMeters * scale;
    if (!finitePose(world) || !std::isfinite(width) || !std::isfinite(height))
        return std::nullopt;

    return WristSurface { world, width, height, calibration.has_value() };
}
}
