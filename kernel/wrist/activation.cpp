#include "activation.h"

#include "geometry.h"

#include <cmath>

namespace fnvxr::kernel::wrist
{
ActivationDecision WristActivation::advance(
    const WristUiConfig& config,
    const PoseSnapshot& head,
    const PoseSnapshot& rightAim,
    const PoseSnapshot& screen) noexcept
{
    if (!head.tracked() || !rightAim.tracked() || !screen.tracked())
    {
        active_ = false;
        return {};
    }
    if (!finitePose(head.pose())
        || !finitePose(rightAim.pose())
        || !finitePose(screen.pose())
        || !std::isfinite(config.activationDistanceMeters)
        || !std::isfinite(config.deactivationDistanceMeters)
        || !std::isfinite(config.activationAngleDegrees)
        || config.activationDistanceMeters <= 0.0F
        || config.deactivationDistanceMeters < config.activationDistanceMeters
        || config.activationAngleDegrees <= 0.0F
        || config.activationAngleDegrees >= 180.0F)
    {
        active_ = false;
        return { false, ActivationReason::InvalidInput };
    }

    const float headDistance = distance(
        head.pose().position, screen.pose().position);
    const Vec3 aimForward = rotate(
        rightAim.pose().orientation, { 0.0F, 0.0F, -1.0F });
    const Vec3 aimToScreen {
        screen.pose().position.x - rightAim.pose().position.x,
        screen.pose().position.y - rightAim.pose().position.y,
        screen.pose().position.z - rightAim.pose().position.z,
    };
    const float aimAngle = angleDegrees(aimForward, aimToScreen);
    if (!std::isfinite(headDistance) || !std::isfinite(aimAngle))
    {
        active_ = false;
        return { false, ActivationReason::InvalidInput };
    }

    const float distanceLimit = active_
        ? config.deactivationDistanceMeters
        : config.activationDistanceMeters;
    if (headDistance > distanceLimit)
    {
        active_ = false;
        return { false, ActivationReason::OutsideDistance,
            headDistance, aimAngle };
    }
    if (aimAngle > config.activationAngleDegrees)
    {
        active_ = false;
        return { false, ActivationReason::AimOutsideAngle,
            headDistance, aimAngle };
    }

    const bool wasActive = active_;
    active_ = true;
    return { true, wasActive ? ActivationReason::RemainedActive
                             : ActivationReason::Activated,
        headDistance, aimAngle };
}

void WristActivation::reset() noexcept
{
    active_ = false;
}
}
