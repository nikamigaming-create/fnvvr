#pragma once

#include <cmath>

namespace fnvxr::stereo
{
constexpr float DefaultIpdMeters = 0.064f;
constexpr float DefaultGameUnitsPerMeter = 39.3701f;
constexpr float DefaultIpdGameUnits = DefaultIpdMeters * DefaultGameUnitsPerMeter;

enum class Eye
{
    Left,
    Right,
};

struct Matrix4
{
    float m[4][4] {};
};

struct Matrix3
{
    float m[3][3] {};
};

struct Quaternion
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Vector3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct EyeMatrices
{
    Matrix4 leftView {};
    Matrix4 rightView {};
    Matrix4 leftProjection {};
    Matrix4 rightProjection {};
};

inline bool isFinite(const Matrix4& matrix)
{
    for (const auto& row : matrix.m)
    {
        for (const float value : row)
        {
            if (!std::isfinite(value))
                return false;
        }
    }
    return true;
}

inline Quaternion normalized(Quaternion value)
{
    const float lengthSquared = value.x * value.x
        + value.y * value.y
        + value.z * value.z
        + value.w * value.w;
    if (!std::isfinite(lengthSquared) || lengthSquared < 0.00000001f)
        return {};
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    value.w *= inverseLength;
    return value;
}

inline Quaternion conjugated(const Quaternion& value)
{
    return { -value.x, -value.y, -value.z, value.w };
}

inline Quaternion multiply(const Quaternion& lhs, const Quaternion& rhs)
{
    return normalized({
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
    });
}

// A gameplay/body origin may remove heading, but it must remain aligned with
// OpenXR gravity. Using the recenter pose's pitch/roll as the origin tilts the
// room basis: later world-up yaw becomes mixed pitch/roll and horizontal room
// translation acquires vertical motion.
inline Quaternion gravityAlignedYawOrientation(const Quaternion& input)
{
    const Quaternion q = normalized(input);
    const float yaw = std::atan2(
        2.0f * (q.x * q.z + q.w * q.y),
        1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float halfYaw = yaw * 0.5f;
    return { 0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw) };
}

// OpenXR poses are absolute in the active reference space. Native FNV camera
// rotation needs the current orientation expressed in the gravity-aligned
// gameplay body frame.
inline Quaternion relativeOrientation(const Quaternion& origin, const Quaternion& current)
{
    return multiply(conjugated(normalized(origin)), normalized(current));
}

inline Matrix3 columnRotationFromQuaternion(const Quaternion& input)
{
    const Quaternion q = normalized(input);
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;
    return {{
        { 1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz), 2.0f * (xz + wy) },
        { 2.0f * (xy + wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx) },
        { 2.0f * (xz - wy), 2.0f * (yz + wx), 1.0f - 2.0f * (xx + yy) }
    }};
}

inline Matrix3 xrRotationToGamebryoColumn(const Matrix3& xr)
{
    Matrix3 result {};
    // Coordinate basis change with determinant +1:
    // OpenXR (+X right, +Y up, -Z forward) ->
    // Gamebryo (+X right, +Y forward, +Z up).
    const int xrAxisForGameAxis[3] { 0, 2, 1 };
    const float signForGameAxis[3] { 1.0f, -1.0f, 1.0f };
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            result.m[row][column] = signForGameAxis[row]
                * signForGameAxis[column]
                * xr.m[xrAxisForGameAxis[row]][xrAxisForGameAxis[column]];
        }
    }
    return result;
}

inline Matrix3 gamebryoHeadRotation(const Quaternion& relativeXrOrientation)
{
    return xrRotationToGamebryoColumn(columnRotationFromQuaternion(relativeXrOrientation));
}

inline Matrix3 multiply(const Matrix3& lhs, const Matrix3& rhs)
{
    Matrix3 result {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            for (int inner = 0; inner < 3; ++inner)
                result.m[row][column] += lhs.m[row][inner] * rhs.m[inner][column];
        }
    }
    return result;
}

inline Vector3 transform(const Matrix3& matrix, const Vector3& value)
{
    return {
        matrix.m[0][0] * value.x + matrix.m[0][1] * value.y + matrix.m[0][2] * value.z,
        matrix.m[1][0] * value.x + matrix.m[1][1] * value.y + matrix.m[1][2] * value.z,
        matrix.m[2][0] * value.x + matrix.m[2][1] * value.y + matrix.m[2][2] * value.z
    };
}

inline Vector3 vectorInOriginFrame(const Quaternion& originOrientation, const Vector3& worldVector)
{
    return transform(
        columnRotationFromQuaternion(conjugated(normalized(originOrientation))),
        worldVector);
}

inline Vector3 positionInOriginFrame(
    const Quaternion& originOrientation,
    const Vector3& originPosition,
    const Vector3& currentPosition)
{
    return vectorInOriginFrame(originOrientation, {
        currentPosition.x - originPosition.x,
        currentPosition.y - originPosition.y,
        currentPosition.z - originPosition.z
    });
}

inline Vector3 xrVectorToGamebryo(const Vector3& xr)
{
    return { xr.x, -xr.z, xr.y };
}

// The recentered HMD orientation is body-local.  Compose it on the local side
// of the engine-authored camera/body basis.  World-premultiplying this delta
// rotates the body basis itself and couples head axes to the player's heading.
inline Matrix3 composeBodyAndHead(const Matrix3& bodyWorld, const Matrix3& headLocal)
{
    return multiply(bodyWorld, headLocal);
}

inline float eyeViewTranslationX(Eye eye, float ipdGameUnits)
{
    const float halfIpd = ipdGameUnits * 0.5f;
    return eye == Eye::Left ? halfIpd : -halfIpd;
}

inline Matrix4 makeEyeViewMatrix(const Matrix4& baseView, Eye eye, float ipdGameUnits)
{
    Matrix4 result = baseView;
    result.m[3][0] += eyeViewTranslationX(eye, ipdGameUnits);
    return result;
}

inline EyeMatrices makeEyeMatrices(const Matrix4& baseView, const Matrix4& baseProjection, float ipdGameUnits)
{
    EyeMatrices result {};
    result.leftView = makeEyeViewMatrix(baseView, Eye::Left, ipdGameUnits);
    result.rightView = makeEyeViewMatrix(baseView, Eye::Right, ipdGameUnits);
    result.leftProjection = baseProjection;
    result.rightProjection = baseProjection;
    return result;
}

// Preserve the engine projection's depth convention while replacing its X/Y
// frustum with the asymmetric OpenXR eye tangents. D3D9 uses row vectors, so
// the off-center terms live in m[2][0] and m[2][1].
inline Matrix4 projectionFromTangents(
    const Matrix4& baseProjection,
    float left,
    float right,
    float top,
    float bottom)
{
    Matrix4 result = baseProjection;
    const float width = right - left;
    const float height = top - bottom;
    if (!std::isfinite(width)
        || !std::isfinite(height)
        || std::fabs(width) < 0.000001f
        || std::fabs(height) < 0.000001f)
    {
        return result;
    }

    result.m[0][0] = 2.0f / width;
    result.m[1][1] = 2.0f / height;
    result.m[2][0] = (left + right) / (left - right);
    result.m[2][1] = (top + bottom) / (bottom - top);
    return result;
}
}
