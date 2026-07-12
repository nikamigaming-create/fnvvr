#include "fnvxr_fabrik.h"

#include <cmath>
#include <iostream>

namespace
{
using fnvxr::ik::Vec3;

int fail(const char* message)
{
    std::cerr << message << "\n";
    return 1;
}

bool nearlyEqual(float lhs, float rhs, float tolerance = 0.001f)
{
    return std::fabs(lhs - rhs) <= tolerance;
}

bool segmentLengthsPreserved(const Vec3* joints, const float* lengths, std::size_t count)
{
    for (std::size_t index = 0; index + 1 < count; ++index)
    {
        if (!nearlyEqual(fnvxr::ik::distance(joints[index], joints[index + 1]), lengths[index]))
            return false;
    }
    return true;
}
}

int main()
{
    const float armLengths[2] { 0.32f, 0.28f };

    Vec3 reachable[3] {
        { 0.0f, 0.0f, 0.0f },
        { 0.32f, 0.0f, 0.0f },
        { 0.60f, 0.0f, 0.0f }
    };
    const Vec3 reachableTarget { 0.35f, 0.20f, -0.05f };
    const auto reachableResult = fnvxr::ik::solveFabrik(
        reachable,
        3,
        armLengths,
        reachableTarget,
        { 0.0f, 0.0f, -1.0f });
    if (!reachableResult.reachable || !reachableResult.solved)
        return fail("reachable arm target should converge");
    if (fnvxr::ik::distance(reachable[2], reachableTarget) > 0.01f)
        return fail("reachable wrist should land on its controller target");
    if (!segmentLengthsPreserved(reachable, armLengths, 3))
        return fail("reachable solve must preserve retail upper/lower arm lengths");
    if (reachable[1].z >= 0.0f)
        return fail("elbow pole should select the negative-Z bend side");

    Vec3 unreachable[3] {
        { 1.0f, 2.0f, 3.0f },
        { 1.32f, 2.0f, 3.0f },
        { 1.60f, 2.0f, 3.0f }
    };
    const Vec3 unreachableTarget { 4.0f, 2.0f, 3.0f };
    const auto unreachableResult = fnvxr::ik::solveFabrik(
        unreachable,
        3,
        armLengths,
        unreachableTarget,
        { 1.0f, 2.0f, 2.0f });
    if (unreachableResult.reachable || !unreachableResult.solved)
        return fail("unreachable arm target should extend safely without claiming reachability");
    if (!segmentLengthsPreserved(unreachable, armLengths, 3))
        return fail("unreachable solve must not stretch the retail skeleton");
    if (!nearlyEqual(unreachable[2].x, 1.0f + armLengths[0] + armLengths[1]))
        return fail("unreachable wrist should stop at full anatomical extension");

    Vec3 singular[3] {
        { 0.0f, 0.0f, 0.0f },
        { 0.32f, 0.0f, 0.0f },
        { 0.60f, 0.0f, 0.0f }
    };
    const Vec3 singularTarget { 0.45f, 0.0f, 0.0f };
    const auto singularResult = fnvxr::ik::solveFabrik(
        singular,
        3,
        armLengths,
        singularTarget,
        { 0.0f, 0.0f, -1.0f });
    if (!singularResult.solved || singular[1].z >= 0.0f)
        return fail("straight-chain singularity should seed an elbow bend toward the pole");
    if (!segmentLengthsPreserved(singular, armLengths, 3))
        return fail("singularity recovery must preserve arm lengths");

    Vec3 leftPole[3] {
        { 0.0f, 0.0f, 0.0f },
        { 0.30f, 0.10f, 0.0f },
        { 0.55f, 0.0f, 0.0f }
    };
    Vec3 rightPole[3] { leftPole[0], leftPole[1], leftPole[2] };
    const Vec3 sharedTarget { 0.45f, 0.0f, 0.0f };
    fnvxr::ik::solveFabrik(leftPole, 3, armLengths, sharedTarget, { 0.0f, 1.0f, 0.0f });
    fnvxr::ik::solveFabrik(rightPole, 3, armLengths, sharedTarget, { 0.0f, -1.0f, 0.0f });
    if (leftPole[1].y <= 0.0f || rightPole[1].y >= 0.0f)
        return fail("opposite pole targets should choose opposite elbow branches");

    std::cout << "FABRIK arm math ok\n";
    return 0;
}
