#pragma once

#include "../runtime_config.h"
#include "pose_snapshot.h"

namespace fnvxr::kernel::wrist
{
enum class ActivationReason
{
    TrackingUnavailable,
    InvalidInput,
    OutsideDistance,
    AimOutsideAngle,
    Activated,
    RemainedActive,
};

struct ActivationDecision final
{
    bool active = false;
    ActivationReason reason = ActivationReason::TrackingUnavailable;
    float headDistanceMeters = 0.0F;
    float aimAngleDegrees = 0.0F;
};

class WristActivation final
{
public:
    [[nodiscard]] ActivationDecision advance(
        const WristUiConfig& config,
        const PoseSnapshot& head,
        const PoseSnapshot& rightAim,
        const PoseSnapshot& screen) noexcept;

    void reset() noexcept;

private:
    bool active_ = false;
};
}
