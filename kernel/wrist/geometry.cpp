#include "geometry.h"

#include <algorithm>
#include <cmath>

namespace fnvxr::kernel::wrist
{
namespace
{
Quaternion normalized(const Quaternion& value) noexcept
{
    const float length = std::sqrt(value.x * value.x + value.y * value.y
        + value.z * value.z + value.w * value.w);
    if (!std::isfinite(length) || length <= 0.000001F)
        return {};
    return { value.x / length, value.y / length,
        value.z / length, value.w / length };
}

Quaternion multiply(const Quaternion& left, const Quaternion& right) noexcept
{
    return {
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
    };
}
}

bool finitePose(const Pose& pose) noexcept
{
    const Quaternion& q = pose.orientation;
    const float normSquared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return std::isfinite(pose.position.x)
        && std::isfinite(pose.position.y)
        && std::isfinite(pose.position.z)
        && std::isfinite(q.x) && std::isfinite(q.y)
        && std::isfinite(q.z) && std::isfinite(q.w)
        && std::isfinite(normSquared) && normSquared > 0.000001F;
}

Vec3 rotate(const Quaternion& rotation, const Vec3& value) noexcept
{
    const Quaternion q = normalized(rotation);
    const Quaternion vector { value.x, value.y, value.z, 0.0F };
    const Quaternion inverse { -q.x, -q.y, -q.z, q.w };
    const Quaternion result = multiply(multiply(q, vector), inverse);
    return { result.x, result.y, result.z };
}

Pose compose(const Pose& parent, const Pose& local) noexcept
{
    const Vec3 offset = rotate(parent.orientation, local.position);
    return {
        { parent.position.x + offset.x, parent.position.y + offset.y,
            parent.position.z + offset.z },
        normalized(multiply(normalized(parent.orientation),
            normalized(local.orientation))),
    };
}

float distance(const Vec3& left, const Vec3& right) noexcept
{
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z);
}

float angleDegrees(const Vec3& left, const Vec3& right) noexcept
{
    const float leftLength = std::sqrt(left.x * left.x + left.y * left.y
        + left.z * left.z);
    const float rightLength = std::sqrt(right.x * right.x + right.y * right.y
        + right.z * right.z);
    if (!std::isfinite(leftLength) || !std::isfinite(rightLength)
        || leftLength <= 0.000001F || rightLength <= 0.000001F)
    {
        return 180.0F;
    }
    const float cosine = std::clamp(
        (left.x * right.x + left.y * right.y + left.z * right.z)
            / (leftLength * rightLength),
        -1.0F, 1.0F);
    return std::acos(cosine) * 57.2957795131F;
}
}
