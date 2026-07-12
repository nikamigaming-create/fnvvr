#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace fnvxr::ik
{
struct Vec3
{
    float x {};
    float y {};
    float z {};
};

struct SolveOptions
{
    int maxIterations { 12 };
    float tolerance { 0.01f };
    float poleWeight { 1.0f };
};

struct SolveResult
{
    bool solved {};
    bool reachable {};
    int iterations {};
    float error {};
};

inline Vec3 operator+(Vec3 lhs, Vec3 rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

inline Vec3 operator-(Vec3 lhs, Vec3 rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

inline Vec3 operator*(Vec3 value, float scale)
{
    return { value.x * scale, value.y * scale, value.z * scale };
}

inline float dot(Vec3 lhs, Vec3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vec3 cross(Vec3 lhs, Vec3 rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

inline float lengthSquared(Vec3 value)
{
    return dot(value, value);
}

inline float length(Vec3 value)
{
    return std::sqrt(lengthSquared(value));
}

inline float distance(Vec3 lhs, Vec3 rhs)
{
    return length(lhs - rhs);
}

inline bool finite(Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline Vec3 normalized(Vec3 value, Vec3 fallback = { 1.0f, 0.0f, 0.0f })
{
    const float magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude < 0.000001f)
        return fallback;
    return value * (1.0f / magnitude);
}

inline Vec3 rotateAroundAxis(Vec3 value, Vec3 axis, float angle)
{
    axis = normalized(axis, { 0.0f, 0.0f, 1.0f });
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return value * c + cross(axis, value) * s + axis * (dot(axis, value) * (1.0f - c));
}

inline void applyPole(Vec3* joints, std::size_t jointCount, Vec3 pole, float poleWeight)
{
    if (!joints || jointCount < 3 || poleWeight <= 0.0f || !finite(pole))
        return;

    poleWeight = std::clamp(poleWeight, 0.0f, 1.0f);
    for (std::size_t index = 1; index + 1 < jointCount; ++index)
    {
        const Vec3 lineOrigin = joints[index - 1];
        const Vec3 lineAxis = normalized(joints[index + 1] - lineOrigin);
        const Vec3 jointFromOrigin = joints[index] - lineOrigin;
        const Vec3 poleFromOrigin = pole - lineOrigin;
        const Vec3 projectedJoint = jointFromOrigin - lineAxis * dot(jointFromOrigin, lineAxis);
        const Vec3 projectedPole = poleFromOrigin - lineAxis * dot(poleFromOrigin, lineAxis);
        if (lengthSquared(projectedJoint) < 0.000001f || lengthSquared(projectedPole) < 0.000001f)
            continue;

        const Vec3 jointDirection = normalized(projectedJoint);
        const Vec3 poleDirection = normalized(projectedPole);
        const float cosine = std::clamp(dot(jointDirection, poleDirection), -1.0f, 1.0f);
        const float sine = dot(lineAxis, cross(jointDirection, poleDirection));
        const float angle = std::atan2(sine, cosine) * poleWeight;
        joints[index] = lineOrigin + rotateAroundAxis(jointFromOrigin, lineAxis, angle);
    }
}

// Solves a fixed-root chain in place. segmentLengths[index] is the distance
// from joints[index] to joints[index + 1]. A pole target controls elbow/knee
// bend without changing the segment lengths established by FABRIK.
inline SolveResult solveFabrik(
    Vec3* joints,
    std::size_t jointCount,
    const float* segmentLengths,
    Vec3 target,
    Vec3 pole,
    SolveOptions options = {})
{
    SolveResult result {};
    if (!joints || !segmentLengths || jointCount < 2 || !finite(target))
        return result;

    float totalLength = 0.0f;
    for (std::size_t index = 0; index + 1 < jointCount; ++index)
    {
        if (!std::isfinite(segmentLengths[index]) || segmentLengths[index] <= 0.000001f)
            return result;
        totalLength += segmentLengths[index];
    }

    const Vec3 root = joints[0];
    result.reachable = distance(root, target) <= totalLength;
    if (!result.reachable)
    {
        const Vec3 direction = normalized(target - root);
        joints[0] = root;
        for (std::size_t index = 0; index + 1 < jointCount; ++index)
            joints[index + 1] = joints[index] + direction * segmentLengths[index];
        result.iterations = 1;
        result.error = distance(joints[jointCount - 1], target);
        result.solved = finite(joints[jointCount - 1]);
        return result;
    }

    // A perfectly straight chain aimed at a nearer target is a FABRIK
    // singularity: forward/backward passes can keep returning to the same
    // fully extended line. Seed a tiny bend toward the pole so the iterative
    // solve can choose the intended elbow branch.
    const Vec3 rootToTarget = target - root;
    const float rootTargetDistance = length(rootToTarget);
    if (jointCount > 2 && rootTargetDistance < totalLength - 0.0001f)
    {
        const Vec3 axis = normalized(rootToTarget);
        const Vec3 poleFromRoot = pole - root;
        const Vec3 poleProjected = poleFromRoot - axis * dot(poleFromRoot, axis);
        if (lengthSquared(poleProjected) > 0.000001f)
        {
            const Vec3 bendDirection = normalized(poleProjected);
            for (std::size_t index = 1; index + 1 < jointCount; ++index)
            {
                const Vec3 jointFromRoot = joints[index] - root;
                const Vec3 jointProjected = jointFromRoot - axis * dot(jointFromRoot, axis);
                if (lengthSquared(jointProjected) < 0.000001f)
                {
                    const float seed = (std::min)(segmentLengths[index - 1], segmentLengths[index]) * 0.05f;
                    joints[index] = joints[index] + bendDirection * seed;
                }
            }
        }
    }

    const int iterations = std::clamp(options.maxIterations, 1, 64);
    const float tolerance = (std::max)(options.tolerance, 0.000001f);
    for (int iteration = 0; iteration < iterations; ++iteration)
    {
        joints[jointCount - 1] = target;
        for (std::size_t reverse = jointCount - 1; reverse > 0; --reverse)
        {
            const std::size_t parent = reverse - 1;
            const Vec3 direction = normalized(joints[parent] - joints[reverse]);
            joints[parent] = joints[reverse] + direction * segmentLengths[parent];
        }

        joints[0] = root;
        for (std::size_t index = 0; index + 1 < jointCount; ++index)
        {
            const Vec3 direction = normalized(joints[index + 1] - joints[index]);
            joints[index + 1] = joints[index] + direction * segmentLengths[index];
        }

        applyPole(joints, jointCount, pole, options.poleWeight);
        result.iterations = iteration + 1;
        result.error = distance(joints[jointCount - 1], target);
        if (result.error <= tolerance)
            break;
    }

    result.solved = finite(joints[jointCount - 1]) && result.error <= tolerance;
    return result;
}
}
