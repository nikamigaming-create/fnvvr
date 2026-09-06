#pragma once

#include "../kernel/rig/first_person_rig.h"
#include "../kernel/wrist/placement.h"

#include <openxr/openxr.h>

#include <optional>

namespace fnvxr::host::spatial
{
struct SolvedArm final
{
    bool valid = false;
    bool reachClamped = false;
    XrPosef shoulder {};
    XrPosef elbow {};
    XrPosef wrist {};
    XrPosef forearm {};
    float forearmLength = 0.0F;
};

struct BodyRig final
{
    bool valid = false;
    SolvedArm left {};
    SolvedArm right {};
};

struct WristPlane final
{
    XrPosef pose {};
    float widthMeters = 0.0F;
    float heightMeters = 0.0F;
    bool calibrated = false;
};

[[nodiscard]] kernel::Pose toKernelPose(const XrPosef& pose) noexcept;
[[nodiscard]] XrPosef toOpenXrPose(const kernel::Pose& pose) noexcept;

// The mesh and IK endpoint must use this same grip-local wrist socket.
[[nodiscard]] XrPosef handAttachmentPose(
    const XrPosef& grip, const XrVector3f& localOffset) noexcept;

[[nodiscard]] BodyRig solveBodyRig(
    const XrPosef& head,
    bool headTracked,
    const XrPosef& leftWrist,
    bool leftTracked,
    const XrPosef& rightWrist,
    bool rightTracked,
    const kernel::BodyRigConfig& config) noexcept;

[[nodiscard]] std::optional<WristPlane> placeWristPlane(
    const kernel::WristUiConfig& config,
    const XrPosef& leftGrip,
    bool leftGripTracked,
    float scale,
    const XrPosef* calibratedGripToScreen = nullptr) noexcept;
}
