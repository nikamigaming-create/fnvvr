#include "../kernel/rig/first_person_rig.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
constexpr float Tolerance = 1.0e-4F;

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

bool close(float left, float right) noexcept
{
    return std::fabs(left - right) <= Tolerance;
}

bool mirrored(
    const fnvxr::kernel::Vec3& left,
    const fnvxr::kernel::Vec3& right) noexcept
{
    return close(left.x, -right.x) && close(left.y, right.y) && close(left.z, right.z);
}

bool finite(const fnvxr::kernel::Pose& pose) noexcept
{
    return std::isfinite(pose.position.x) && std::isfinite(pose.position.y)
        && std::isfinite(pose.position.z) && std::isfinite(pose.orientation.x)
        && std::isfinite(pose.orientation.y) && std::isfinite(pose.orientation.z)
        && std::isfinite(pose.orientation.w);
}

float distance(
    const fnvxr::kernel::Vec3& left,
    const fnvxr::kernel::Vec3& right) noexcept
{
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z);
}

bool same(const fnvxr::kernel::Pose& left, const fnvxr::kernel::Pose& right) noexcept
{
    return close(left.position.x, right.position.x)
        && close(left.position.y, right.position.y)
        && close(left.position.z, right.position.z)
        && close(left.orientation.x, right.orientation.x)
        && close(left.orientation.y, right.orientation.y)
        && close(left.orientation.z, right.orientation.z)
        && close(left.orientation.w, right.orientation.w);
}
}

int main()
{
    using namespace fnvxr::kernel;
    using namespace fnvxr::kernel::rig;

    const BodyRigConfig config {};
    const Pose leftShoulder { { -0.19F, 1.45F, 0.0F }, {} };
    const Pose rightShoulder { { 0.19F, 1.45F, 0.0F }, {} };
    const Pose leftWrist { { -0.48F, 1.20F, -0.28F }, {} };
    const Pose rightWrist { { 0.48F, 1.20F, -0.28F }, {} };

    const ArmRigSolution left = solveArm(config, Handedness::Left, leftShoulder, leftWrist);
    const ArmRigSolution right = solveArm(config, Handedness::Right, rightShoulder, rightWrist);
    if (!left || !right || !mirrored(left.elbow.position, right.elbow.position)
        || !mirrored(left.wrist.position, right.wrist.position))
        return fail("left and right arm solves must be mirror symmetric");

    const Pose unreachable { { 0.19F, 1.45F, -4.0F }, {} };
    const ArmRigSolution clamped = solveArm(
        config, Handedness::Right, rightShoulder, unreachable);
    if (!clamped || !clamped.reachClamped
        || !same(clamped.wrist, unreachable)
        || distance(clamped.shoulder.position, clamped.elbow.position)
            > config.upperArmLengthMeters + Tolerance)
        return fail("reach clamping must stabilize the elbow without detaching the tracked wrist");

    const Pose singularTarget { { 0.305F, 1.12F, -0.066F }, {} };
    const ArmRigSolution singular = solveArm(
        config, Handedness::Right, rightShoulder, singularTarget);
    if (!singular || !finite(singular.shoulder) || !finite(singular.elbow)
        || !finite(singular.wrist))
        return fail("near-singular elbow planes must retain finite output");

    const ArmRigSolution repeated = solveArm(
        config, Handedness::Right, rightShoulder, singularTarget);
    if (!repeated || repeated.reachClamped != singular.reachClamped
        || !same(repeated.shoulder, singular.shoulder)
        || !same(repeated.elbow, singular.elbow)
        || !same(repeated.wrist, singular.wrist))
        return fail("identical inputs must produce deterministic results");

    Pose invalidWrist = rightWrist;
    invalidWrist.position.x = std::numeric_limits<float>::quiet_NaN();
    if (solveArm(config, Handedness::Right, rightShoulder, invalidWrist).error
        != RigSolveError::InvalidWristPose)
        return fail("unusable tracking poses must fail closed");

    BodyRigConfig invalidConfig = config;
    invalidConfig.forearmLengthMeters = 0.0F;
    if (solveArm(invalidConfig, Handedness::Right, rightShoulder, rightWrist).error
        != RigSolveError::InvalidConfiguration)
        return fail("unusable rig configuration must fail closed");

    return EXIT_SUCCESS;
}
