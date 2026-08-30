#include "fnvxr_openxr_spatial_adapter.h"

#include "../kernel/wrist/geometry.h"

#include <cmath>

namespace fnvxr::host::spatial
{
namespace
{
using kernel::Pose;
using kernel::Quaternion;
using kernel::Vec3;

Vec3 add(const Vec3& left, const Vec3& right) noexcept
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

Vec3 subtract(const Vec3& left, const Vec3& right) noexcept
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

Vec3 scale(const Vec3& value, float factor) noexcept
{
    return { value.x * factor, value.y * factor, value.z * factor };
}

Vec3 cross(const Vec3& left, const Vec3& right) noexcept
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float length(const Vec3& value) noexcept
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

bool normalize(Vec3& value) noexcept
{
    const float magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude <= 0.000001F)
        return false;
    value = scale(value, 1.0F / magnitude);
    return true;
}

Quaternion yawOrientation(const Vec3& forward) noexcept
{
    if (forward.z > 1.0F - 0.000001F)
        return { 0.0F, 1.0F, 0.0F, 0.0F };
    Quaternion result { 0.0F, -forward.x, 0.0F, 1.0F - forward.z };
    const float magnitude = std::sqrt(
        result.y * result.y + result.w * result.w);
    if (!std::isfinite(magnitude) || magnitude <= 0.000001F)
        return {};
    result.y /= magnitude;
    result.w /= magnitude;
    return result;
}

SolvedArm convert(const kernel::rig::ArmRigSolution& solution) noexcept
{
    if (!solution)
        return {};

    SolvedArm arm {};
    arm.valid = true;
    arm.reachClamped = solution.reachClamped;
    arm.shoulder = toOpenXrPose(solution.shoulder);
    arm.elbow = toOpenXrPose(solution.elbow);
    arm.wrist = toOpenXrPose(solution.wrist);
    arm.forearmLength = length(subtract(
        solution.wrist.position, solution.elbow.position));

    // Kernel bones point local -Z along the limb. The authored forearm mesh
    // points local +Z, so apply one explicit adapter-only half turn.
    const Pose meshPose = kernel::wrist::compose(
        { scale(add(solution.elbow.position, solution.wrist.position), 0.5F),
            solution.elbow.orientation },
        { {}, Quaternion { 0.0F, 1.0F, 0.0F, 0.0F } });
    arm.forearm = toOpenXrPose(meshPose);
    return arm;
}
}

kernel::Pose toKernelPose(const XrPosef& pose) noexcept
{
    return {
        { pose.position.x, pose.position.y, pose.position.z },
        { pose.orientation.x, pose.orientation.y,
            pose.orientation.z, pose.orientation.w },
    };
}

XrPosef toOpenXrPose(const kernel::Pose& pose) noexcept
{
    XrPosef result {};
    result.position = { pose.position.x, pose.position.y, pose.position.z };
    result.orientation = { pose.orientation.x, pose.orientation.y,
        pose.orientation.z, pose.orientation.w };
    return result;
}

BodyRig solveBodyRig(
    const XrPosef& head,
    bool headTracked,
    const XrPosef& leftWrist,
    bool leftTracked,
    const XrPosef& rightWrist,
    bool rightTracked,
    const kernel::BodyRigConfig& config) noexcept
{
    BodyRig rig {};
    const Pose headPose = toKernelPose(head);
    if (!headTracked || !kernel::wrist::finitePose(headPose))
        return rig;

    Vec3 forward = kernel::wrist::rotate(
        headPose.orientation, { 0.0F, 0.0F, -1.0F });
    forward.y = 0.0F;
    if (!normalize(forward))
        forward = { 0.0F, 0.0F, -1.0F };
    const Vec3 up { 0.0F, 1.0F, 0.0F };
    Vec3 right = cross(forward, up);
    if (!normalize(right))
        right = { 1.0F, 0.0F, 0.0F };

    const Vec3 center = subtract(
        subtract(headPose.position, scale(up, config.shoulderDropMeters)),
        scale(forward, config.shoulderBackMeters));
    const float halfWidth = config.shoulderWidthMeters * 0.5F;
    const Quaternion torsoOrientation = yawOrientation(forward);
    const Pose leftShoulder {
        subtract(center, scale(right, halfWidth)), torsoOrientation };
    const Pose rightShoulder {
        add(center, scale(right, halfWidth)), torsoOrientation };

    if (leftTracked)
    {
        rig.left = convert(kernel::rig::solveArm(
            config,
            kernel::rig::Handedness::Left,
            leftShoulder,
            toKernelPose(leftWrist)));
    }
    if (rightTracked)
    {
        rig.right = convert(kernel::rig::solveArm(
            config,
            kernel::rig::Handedness::Right,
            rightShoulder,
            toKernelPose(rightWrist)));
    }
    rig.valid = rig.left.valid && rig.right.valid;
    return rig;
}

std::optional<WristPlane> placeWristPlane(
    const kernel::WristUiConfig& config,
    const XrPosef& leftGrip,
    bool leftGripTracked,
    float scaleValue,
    const XrPosef* calibratedGripToScreen) noexcept
{
    std::optional<kernel::wrist::LocalScreenTransform> calibration;
    if (calibratedGripToScreen != nullptr)
        calibration.emplace(toKernelPose(*calibratedGripToScreen));

    const std::optional<kernel::wrist::WristSurface> surface =
        kernel::wrist::placeWristSurface(
            config,
            kernel::wrist::PoseSnapshot(
                toKernelPose(leftGrip), leftGripTracked),
            calibration,
            scaleValue);
    if (!surface)
        return std::nullopt;
    return WristPlane {
        toOpenXrPose(surface->pose),
        surface->widthMeters,
        surface->heightMeters,
        surface->calibrated,
    };
}
}
