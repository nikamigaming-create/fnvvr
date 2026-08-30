#pragma once

#include "../frame_math.h"
#include "../runtime_config.h"

#include <cstdint>

namespace fnvxr::kernel::rig
{
enum class Handedness : std::uint8_t
{
    Left,
    Right,
};

enum class RigSolveError : std::uint8_t
{
    None,
    InvalidConfiguration,
    InvalidShoulderPose,
    InvalidWristPose,
};

struct ArmRigSolution final
{
    RigSolveError error = RigSolveError::None;
    bool reachClamped = false;
    Pose shoulder {};
    Pose elbow {};
    Pose wrist {};

    explicit operator bool() const noexcept;
};

ArmRigSolution solveArm(
    const BodyRigConfig& config,
    Handedness hand,
    const Pose& shoulderAnchor,
    const Pose& trackedWrist) noexcept;
}
