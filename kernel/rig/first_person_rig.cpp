#include "first_person_rig.h"

#include <algorithm>
#include <cmath>

namespace fnvxr::kernel::rig
{
namespace
{
constexpr float DirectionEpsilon = 1.0e-6F;
constexpr float ReachMarginMeters = 1.0e-4F;

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

float dot(const Vec3& left, const Vec3& right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(const Vec3& left, const Vec3& right) noexcept
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float lengthSquared(const Vec3& value) noexcept
{
    return dot(value, value);
}

bool finite(const Vec3& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite(const Quaternion& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z) && std::isfinite(value.w);
}

bool normalize(const Vec3& value, Vec3& result) noexcept
{
    const float squared = lengthSquared(value);
    if (!std::isfinite(squared) || squared <= DirectionEpsilon * DirectionEpsilon)
        return false;
    result = scale(value, 1.0F / std::sqrt(squared));
    return finite(result);
}

bool normalize(const Quaternion& value, Quaternion& result) noexcept
{
    if (!finite(value))
        return false;
    const float squared = value.x * value.x + value.y * value.y
        + value.z * value.z + value.w * value.w;
    if (!std::isfinite(squared) || squared <= DirectionEpsilon * DirectionEpsilon)
        return false;
    const float inverseLength = 1.0F / std::sqrt(squared);
    result = {
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength,
        value.w * inverseLength,
    };
    return finite(result);
}

Vec3 rotate(const Quaternion& orientation, const Vec3& value) noexcept
{
    const Vec3 vectorPart { orientation.x, orientation.y, orientation.z };
    const Vec3 firstCross = cross(vectorPart, value);
    const Vec3 secondCross = cross(vectorPart, firstCross);
    return add(value, add(scale(firstCross, 2.0F * orientation.w), scale(secondCross, 2.0F)));
}

Quaternion aimNegativeZ(const Vec3& direction) noexcept
{
    const float alignment = -direction.z;
    if (alignment < -1.0F + DirectionEpsilon)
        return { 0.0F, 1.0F, 0.0F, 0.0F };

    Quaternion result {
        direction.y,
        -direction.x,
        0.0F,
        1.0F + alignment,
    };
    Quaternion normalized {};
    if (!normalize(result, normalized))
        return {};
    return normalized;
}

Vec3 stablePerpendicular(const Vec3& axis, Handedness hand) noexcept
{
    const float side = hand == Handedness::Left ? -1.0F : 1.0F;
    const Vec3 candidates[] = {
        { side, 0.0F, 0.0F },
        { 0.0F, -1.0F, 0.0F },
        { 0.0F, 0.0F, -1.0F },
    };
    for (const Vec3& candidate : candidates)
    {
        Vec3 perpendicular {};
        if (normalize(subtract(candidate, scale(axis, dot(candidate, axis))), perpendicular))
            return perpendicular;
    }
    return { side, 0.0F, 0.0F };
}

bool usableConfig(const BodyRigConfig& config) noexcept
{
    return std::isfinite(config.standingHeightMeters)
        && std::isfinite(config.shoulderWidthMeters)
        && std::isfinite(config.shoulderDropMeters)
        && std::isfinite(config.shoulderBackMeters)
        && std::isfinite(config.upperArmLengthMeters)
        && std::isfinite(config.forearmLengthMeters)
        && std::isfinite(config.handLengthMeters)
        && config.standingHeightMeters >= 1.2F
        && config.standingHeightMeters <= 2.3F
        && config.shoulderWidthMeters >= 0.25F
        && config.shoulderWidthMeters <= 0.7F
        && config.shoulderDropMeters >= 0.1F
        && config.shoulderDropMeters <= 0.5F
        && config.shoulderBackMeters >= 0.0F
        && config.shoulderBackMeters <= 0.3F
        && config.upperArmLengthMeters >= 0.15F
        && config.upperArmLengthMeters <= 0.5F
        && config.forearmLengthMeters >= 0.15F
        && config.forearmLengthMeters <= 0.5F
        && config.handLengthMeters >= 0.1F
        && config.handLengthMeters <= 0.3F
        && config.upperArmLengthMeters + config.forearmLengthMeters
            + config.handLengthMeters < config.standingHeightMeters * 0.65F;
}

ArmRigSolution failure(RigSolveError error) noexcept
{
    ArmRigSolution result {};
    result.error = error;
    return result;
}
}

ArmRigSolution::operator bool() const noexcept
{
    return error == RigSolveError::None;
}

ArmRigSolution solveArm(
    const BodyRigConfig& config,
    Handedness hand,
    const Pose& shoulderAnchor,
    const Pose& trackedWrist) noexcept
{
    if (!usableConfig(config))
        return failure(RigSolveError::InvalidConfiguration);

    Quaternion shoulderReference {};
    if (!finite(shoulderAnchor.position)
        || !normalize(shoulderAnchor.orientation, shoulderReference))
        return failure(RigSolveError::InvalidShoulderPose);

    Quaternion wristOrientation {};
    if (!finite(trackedWrist.position)
        || !normalize(trackedWrist.orientation, wristOrientation))
        return failure(RigSolveError::InvalidWristPose);

    const float upperLength = config.upperArmLengthMeters;
    const float forearmLength = config.forearmLengthMeters;
    const float minimumReach = std::fabs(upperLength - forearmLength) + ReachMarginMeters;
    const float maximumReach = upperLength + forearmLength - ReachMarginMeters;

    const Vec3 requestedOffset = subtract(trackedWrist.position, shoulderAnchor.position);
    const float requestedDistance = std::sqrt(lengthSquared(requestedOffset));
    Vec3 reachDirection {};
    if (!normalize(requestedOffset, reachDirection))
        reachDirection = rotate(shoulderReference, { 0.0F, 0.0F, -1.0F });
    if (!normalize(reachDirection, reachDirection))
        return failure(RigSolveError::InvalidShoulderPose);

    const float reach = std::clamp(requestedDistance, minimumReach, maximumReach);
    const bool reachClamped = !std::isfinite(requestedDistance)
        || std::fabs(reach - requestedDistance) > DirectionEpsilon;
    const Vec3 wristPosition = add(shoulderAnchor.position, scale(reachDirection, reach));

    const float elbowAlong =
        (upperLength * upperLength - forearmLength * forearmLength + reach * reach)
        / (2.0F * reach);
    const float elbowHeightSquared = std::max(
        0.0F, upperLength * upperLength - elbowAlong * elbowAlong);

    const float side = hand == Handedness::Left ? -1.0F : 1.0F;
    const Vec3 localHint { side * 0.35F, -1.0F, -0.2F };
    const Vec3 worldHint = rotate(shoulderReference, localHint);
    Vec3 bendDirection {};
    if (!normalize(
            subtract(worldHint, scale(reachDirection, dot(worldHint, reachDirection))),
            bendDirection))
        bendDirection = stablePerpendicular(reachDirection, hand);

    const Vec3 elbowPosition = add(
        shoulderAnchor.position,
        add(scale(reachDirection, elbowAlong),
            scale(bendDirection, std::sqrt(elbowHeightSquared))));
    if (!finite(wristPosition) || !finite(elbowPosition))
        return failure(RigSolveError::InvalidWristPose);

    Vec3 upperDirection {};
    Vec3 lowerDirection {};
    if (!normalize(subtract(elbowPosition, shoulderAnchor.position), upperDirection)
        || !normalize(subtract(trackedWrist.position, elbowPosition), lowerDirection))
        return failure(RigSolveError::InvalidWristPose);

    ArmRigSolution result {};
    result.reachClamped = reachClamped;
    result.shoulder = { shoulderAnchor.position, aimNegativeZ(upperDirection) };
    result.elbow = { elbowPosition, aimNegativeZ(lowerDirection) };
    // The controller is the physical endpoint. Reach clamping stabilizes the
    // analytic elbow only; moving the wrist would detach hands and wrist UI
    // from tracking whenever the anthropometric estimate is too short.
    result.wrist = { trackedWrist.position, wristOrientation };
    return result;
}
}
