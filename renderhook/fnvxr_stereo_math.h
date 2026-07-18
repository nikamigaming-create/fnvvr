#pragma once

#include "../protocol/fnvxr_shared_state.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <xmmintrin.h>
#endif

namespace fnvxr::stereo
{
struct ScopedIeeeSubnormalMode
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    unsigned int saved = 0;
    ScopedIeeeSubnormalMode()
        : saved(_mm_getcsr())
    {
        // Clear DAZ (bit 6) and FTZ (bit 15). The outward interval proof is
        // valid for IEEE subnormal arithmetic, not process-global flush mode.
        _mm_setcsr(saved & ~(0x0040u | 0x8000u));
    }
    ~ScopedIeeeSubnormalMode()
    {
        _mm_setcsr(saved);
    }
#else
    ScopedIeeeSubnormalMode() = default;
#endif
};

constexpr float DefaultIpdMeters = 0.064f;
// Fallout 3/New Vegas GECK geometry uses approximately 7 units per 10 cm
// (64 units per yard), not one unit per inch.
constexpr float DefaultGameUnitsPerMeter = 70.0f;
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

struct EyeCullFrustum
{
    // Eye pose expressed in the same body-local right/up/back frame as the
    // center camera. Rotation maps eye-local vectors into that body frame.
    Matrix3 rotation {};
    Vector3 position {};
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
};

struct PerspectiveCullFrustum
{
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    float nearDistance = 0.0f;
    float farDistance = 0.0f;
    bool valid = false;
};

struct ViewProjectionDelta
{
    Matrix4 matrix {};
    double referenceMatrix[4][4] {};
    double centerMatrix[4][4] {};
    double eyeMatrix[4][4] {};
    double inverseCenterMatrix[4][4] {};
    double inverseResidual = std::numeric_limits<double>::infinity();
    double inverseResidualNorm = std::numeric_limits<double>::infinity();
    double reconstructionResidual = std::numeric_limits<double>::infinity();
    double normalizedReconstructionResidual = std::numeric_limits<double>::infinity();
    bool exactIdentity = false;
    bool valid = false;
};

struct ViewProjectionPatchValidation
{
    double maximumAbsoluteError = std::numeric_limits<double>::infinity();
    double normalizedError = std::numeric_limits<double>::infinity();
    bool valid = false;
};

struct EyeBaselineValidation
{
    Vector3 headLocalMeters {};
    float lengthMeters = 0.0f;
    float midpointDistanceMeters = 0.0f;
    bool valid = false;
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

inline bool certifiedStereoTextureMayBeSampled(
    bool drawFullscreen,
    bool useCertifiedStereoTextures,
    bool hasStereoFrame,
    bool stereoFrameSeparated)
{
    return drawFullscreen
        && useCertifiedStereoTextures
        && hasStereoFrame
        && stereoFrameSeparated;
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

inline float xrHeadingYawRadians(const Quaternion& input)
{
    const Quaternion q = normalized(input);
    return std::atan2(
        2.0f * (q.x * q.z + q.w * q.y),
        1.0f - 2.0f * (q.x * q.x + q.y * q.y));
}

// A gameplay/body origin may remove heading, but it must remain aligned with
// OpenXR gravity. Using the recenter pose's pitch/roll as the origin tilts the
// room basis: later world-up yaw becomes mixed pitch/roll and horizontal room
// translation acquires vertical motion.
inline Quaternion gravityAlignedYawOrientation(
    const Quaternion& input,
    const Quaternion& fallback = {})
{
    const Quaternion q = normalized(input);
    // Headset heading is undefined when its forward vector is vertical. Do
    // not let atan2 noise choose an arbitrary room yaw at that singularity;
    // retain the last gravity-aligned heading (identity on first capture).
    const float forwardX = -2.0f * (q.x * q.z + q.w * q.y);
    const float forwardZ = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float horizontalForwardSquared = forwardX * forwardX + forwardZ * forwardZ;
    if (!std::isfinite(horizontalForwardSquared) || horizontalForwardSquared < 0.000001f)
    {
        const Quaternion safeFallback = normalized(fallback);
        const float fallbackYaw = xrHeadingYawRadians(safeFallback);
        const float fallbackHalfYaw = fallbackYaw * 0.5f;
        return { 0.0f, std::sin(fallbackHalfYaw), 0.0f, std::cos(fallbackHalfYaw) };
    }
    const float yaw = xrHeadingYawRadians(q);
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

// NiCamera's local frame is already the OpenXR camera convention: +X is
// right, +Y is up, and -Z is forward.  It is not the Gamebryo actor frame
// (+X right, +Y forward, +Z up).  Applying the actor-basis permutation to a
// camera-local delta swaps headset yaw and pitch.
inline Matrix3 cameraLocalHeadRotation(const Quaternion& relativeXrOrientation)
{
    return columnRotationFromQuaternion(relativeXrOrientation);
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

inline Matrix3 transposed(const Matrix3& matrix)
{
    Matrix3 result {};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            result.m[row][column] = matrix.m[column][row];
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

// Preserve the retail NiCamera column layout (right, up, back) while removing
// engine-authored pitch/roll from the body frame.  A generic Z-up yaw matrix
// has columns (right, forward, up); feeding that to NiCamera puts world-up in
// its back/right pipeline and turns the rendered scene sideways.
inline Matrix3 gravityLevelCameraWorldRotation(const Matrix3& cameraWorld)
{
    float rightX = cameraWorld.m[0][0];
    float rightY = cameraWorld.m[1][0];
    float horizontalLength = std::sqrt(rightX * rightX + rightY * rightY);
    if (!std::isfinite(horizontalLength) || horizontalLength < 0.000001f)
    {
        // A valid camera normally has a horizontal right column.  If it does
        // not, recover it from the projected +Z/back column without changing
        // the handedness: right x up = back.
        float backX = cameraWorld.m[0][2];
        float backY = cameraWorld.m[1][2];
        horizontalLength = std::sqrt(backX * backX + backY * backY);
        if (!std::isfinite(horizontalLength) || horizontalLength < 0.000001f)
            return cameraWorld;
        backX /= horizontalLength;
        backY /= horizontalLength;
        rightX = -backY;
        rightY = backX;
    }
    else
    {
        rightX /= horizontalLength;
        rightY /= horizontalLength;
    }

    Matrix3 result {};
    // Column 0: camera right. Column 1: camera up. Column 2: camera back.
    result.m[0][0] = rightX;
    result.m[1][0] = rightY;
    result.m[2][0] = 0.0f;
    result.m[0][1] = 0.0f;
    result.m[1][1] = 0.0f;
    result.m[2][1] = 1.0f;
    result.m[0][2] = rightY;
    result.m[1][2] = -rightX;
    result.m[2][2] = 0.0f;
    return result;
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

inline EyeBaselineValidation validateEyeBaseline(
    const Quaternion& currentHeadOrientation,
    const Vector3& leftEyePosition,
    const Vector3& rightEyePosition,
    const Vector3& headPosition = {},
    float minimumMeters = 0.03f,
    float maximumMeters = 0.12f,
    float minimumLateralFraction = 0.8f,
    float maximumMidpointDistanceMeters = 0.25f)
{
    EyeBaselineValidation result {};
    result.headLocalMeters = vectorInOriginFrame(currentHeadOrientation, {
        rightEyePosition.x - leftEyePosition.x,
        rightEyePosition.y - leftEyePosition.y,
        rightEyePosition.z - leftEyePosition.z
    });
    result.lengthMeters = std::sqrt(
        result.headLocalMeters.x * result.headLocalMeters.x
        + result.headLocalMeters.y * result.headLocalMeters.y
        + result.headLocalMeters.z * result.headLocalMeters.z);
    const Vector3 midpointOffset {
        (leftEyePosition.x + rightEyePosition.x) * 0.5f - headPosition.x,
        (leftEyePosition.y + rightEyePosition.y) * 0.5f - headPosition.y,
        (leftEyePosition.z + rightEyePosition.z) * 0.5f - headPosition.z
    };
    result.midpointDistanceMeters = std::sqrt(
        midpointOffset.x * midpointOffset.x
        + midpointOffset.y * midpointOffset.y
        + midpointOffset.z * midpointOffset.z);
    result.valid = std::isfinite(result.lengthMeters)
        && std::isfinite(result.midpointDistanceMeters)
        && result.lengthMeters >= minimumMeters
        && result.lengthMeters <= maximumMeters
        && result.midpointDistanceMeters <= maximumMidpointDistanceMeters
        && result.headLocalMeters.x > 0.0f
        && result.headLocalMeters.x >= result.lengthMeters * minimumLateralFraction;
    return result;
}

inline bool openXrFovAnglesUsable(const float fov[4])
{
    if (!fov)
        return false;
    constexpr float HalfPi = 1.57079632679489661923f;
    constexpr float Margin = 0.001f;
    for (int index = 0; index < 4; ++index)
    {
        if (!std::isfinite(fov[index])
            || fov[index] <= -HalfPi + Margin
            || fov[index] >= HalfPi - Margin)
        {
            return false;
        }
    }
    if (!(fov[0] < -0.01f
            && fov[1] > 0.01f
            && fov[2] > 0.01f
            && fov[3] < -0.01f))
    {
        return false;
    }
    const float left = std::tan(fov[0]);
    const float right = std::tan(fov[1]);
    const float top = std::tan(fov[2]);
    const float bottom = std::tan(fov[3]);
    return std::isfinite(left)
        && std::isfinite(right)
        && std::isfinite(top)
        && std::isfinite(bottom)
        && left < right
        && bottom < top;
}

inline Vector3 xrVectorToGamebryo(const Vector3& xr)
{
    return { xr.x, -xr.z, xr.y };
}

// NiCamera local axes already match OpenXR pose axes:
//   +X right, +Y up, +Z back (-Z forward).
// Do not apply the actor-basis permutation (right, forward, up) before a
// vector is multiplied by a NiCamera right/up/back world rotation. Doing so
// turns physical forward motion into camera-up and height into camera-back.
inline Vector3 xrVectorToNiCameraLocal(const Vector3& xr)
{
    return xr;
}

// Bound both displaced, possibly canted OpenXR eye frusta with one center
// NiCamera frustum for the conservative Gamebryo traversal. Comparing only
// raw eye tangents is not a union: with half-IPD h and near distance n, the
// horizontal bound already grows by h/n even when both eyes are parallel.
// Transforming all near/far corner rays into the center frame also covers eye
// cant and forward offsets. The ratio of affine coordinates along each edge is
// monotone while forward depth stays positive, so extrema occur at endpoints.
inline PerspectiveCullFrustum conservativeCenterCullFrustum(
    const Matrix3& centerRotation,
    const Vector3& centerPosition,
    const EyeCullFrustum eyes[2],
    float nearDistance,
    float farDistance)
{
    PerspectiveCullFrustum result {};
    constexpr double MaximumInputMagnitude = 1000000.0;
    if (!eyes
        || !std::isfinite(nearDistance)
        || !std::isfinite(farDistance)
        || nearDistance < 0.01f
        || farDistance <= nearDistance
        || farDistance > MaximumInputMagnitude)
    {
        return result;
    }

    const auto finiteBoundedVector = [=](const Vector3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z)
            && std::fabs(static_cast<double>(value.x)) <= MaximumInputMagnitude
            && std::fabs(static_cast<double>(value.y)) <= MaximumInputMagnitude
            && std::fabs(static_cast<double>(value.z)) <= MaximumInputMagnitude;
    };
    const auto properRotation = [](const Matrix3& value) {
        for (const auto& row : value.m)
            for (float entry : row)
                if (!std::isfinite(entry) || std::fabs(entry) > 1.0001f)
                    return false;
        constexpr double OrthonormalTolerance = 0.002;
        for (int row = 0; row < 3; ++row)
        {
            for (int other = 0; other < 3; ++other)
            {
                double dot = 0.0;
                for (int column = 0; column < 3; ++column)
                    dot += static_cast<double>(value.m[row][column])
                        * static_cast<double>(value.m[other][column]);
                const double expected = row == other ? 1.0 : 0.0;
                if (std::fabs(dot - expected) > OrthonormalTolerance)
                    return false;
            }
        }
        const double determinant =
            static_cast<double>(value.m[0][0])
                * (static_cast<double>(value.m[1][1]) * value.m[2][2]
                    - static_cast<double>(value.m[1][2]) * value.m[2][1])
            - static_cast<double>(value.m[0][1])
                * (static_cast<double>(value.m[1][0]) * value.m[2][2]
                    - static_cast<double>(value.m[1][2]) * value.m[2][0])
            + static_cast<double>(value.m[0][2])
                * (static_cast<double>(value.m[1][0]) * value.m[2][1]
                    - static_cast<double>(value.m[1][1]) * value.m[2][0]);
        return std::isfinite(determinant)
            && std::fabs(determinant - 1.0) <= 0.004;
    };
    if (!properRotation(centerRotation) || !finiteBoundedVector(centerPosition))
        return result;

    double left = std::numeric_limits<double>::infinity();
    double right = -std::numeric_limits<double>::infinity();
    double top = -std::numeric_limits<double>::infinity();
    double bottom = std::numeric_limits<double>::infinity();
    double minimumForward = std::numeric_limits<double>::infinity();
    double maximumForward = 0.0;
    double bodyToCenter[3][3] {};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            bodyToCenter[row][column] = static_cast<double>(centerRotation.m[column][row]);

    for (int eyeIndex = 0; eyeIndex < 2; ++eyeIndex)
    {
        const EyeCullFrustum& eye = eyes[eyeIndex];
        if (!properRotation(eye.rotation)
            || !finiteBoundedVector(eye.position)
            || !std::isfinite(eye.left)
            || !std::isfinite(eye.right)
            || !std::isfinite(eye.top)
            || !std::isfinite(eye.bottom)
            || eye.right <= eye.left
            || eye.top <= eye.bottom
            || std::fabs(eye.left) > 10.0f
            || std::fabs(eye.right) > 10.0f
            || std::fabs(eye.top) > 10.0f
            || std::fabs(eye.bottom) > 10.0f
            || std::fabs(static_cast<double>(eye.position.x) - centerPosition.x) > 1000.0
            || std::fabs(static_cast<double>(eye.position.y) - centerPosition.y) > 1000.0
            || std::fabs(static_cast<double>(eye.position.z) - centerPosition.z) > 1000.0)
        {
            return result;
        }

        const double positionDelta[3] {
            static_cast<double>(eye.position.x) - static_cast<double>(centerPosition.x),
            static_cast<double>(eye.position.y) - static_cast<double>(centerPosition.y),
            static_cast<double>(eye.position.z) - static_cast<double>(centerPosition.z)
        };
        double originInCenter[3] {};
        double eyeToCenter[3][3] {};
        for (int row = 0; row < 3; ++row)
        {
            for (int inner = 0; inner < 3; ++inner)
                originInCenter[row] += bodyToCenter[row][inner] * positionDelta[inner];
            for (int column = 0; column < 3; ++column)
            {
                for (int inner = 0; inner < 3; ++inner)
                {
                    eyeToCenter[row][column] += bodyToCenter[row][inner]
                        * static_cast<double>(eye.rotation.m[inner][column]);
                }
            }
        }
        const float horizontal[2] { eye.left, eye.right };
        const float vertical[2] { eye.bottom, eye.top };
        const float depths[2] { nearDistance, farDistance };
        for (const float depth : depths)
        {
            for (const float tangentX : horizontal)
            {
                for (const float tangentY : vertical)
                {
                    const double local[3] {
                        static_cast<double>(tangentX) * static_cast<double>(depth),
                        static_cast<double>(tangentY) * static_cast<double>(depth),
                        -static_cast<double>(depth)
                    };
                    double rotated[3] {};
                    for (int row = 0; row < 3; ++row)
                        for (int inner = 0; inner < 3; ++inner)
                            rotated[row] += eyeToCenter[row][inner] * local[inner];
                    const double x = originInCenter[0] + rotated[0];
                    const double y = originInCenter[1] + rotated[1];
                    const double forward = -(originInCenter[2] + rotated[2]);
                    // A corner whose forward component is arbitrarily close
                    // to zero has an unbounded, ill-conditioned perspective
                    // slope.  Real headset frusta stay well inside this 0.01
                    // cosine floor; rejecting the singular fringe supplies a
                    // denominator bound for the outward-rounding proof below.
                    const double minimumConditionedForward = (std::max)(
                        0.000001,
                        std::fabs(static_cast<double>(depth)) * 0.01);
                    if (!std::isfinite(x)
                        || !std::isfinite(y)
                        || !std::isfinite(forward)
                        || forward <= minimumConditionedForward)
                    {
                        return result;
                    }
                    const double slopeX = x / forward;
                    const double slopeY = y / forward;
                    left = (std::min)(left, slopeX);
                    right = (std::max)(right, slopeX);
                    bottom = (std::min)(bottom, slopeY);
                    top = (std::max)(top, slopeY);
                    minimumForward = (std::min)(minimumForward, forward);
                    maximumForward = (std::max)(maximumForward, forward);
                }
            }
        }
    }

    if (!std::isfinite(left)
        || !std::isfinite(right)
        || !std::isfinite(top)
        || !std::isfinite(bottom)
        || !std::isfinite(minimumForward)
        || !std::isfinite(maximumForward)
        || right <= left
        || top <= bottom
        || minimumForward <= 0.0
        || maximumForward <= minimumForward)
    {
        return result;
    }

    constexpr float negativeInfinity = -std::numeric_limits<float>::infinity();
    constexpr float positiveInfinity = std::numeric_limits<float>::infinity();
    // Double-precision dot products can suffer cancellation even though every
    // source value is a float. Expand by a bounded relative envelope before
    // the final outward float rounding. The input magnitude limits above make
    // this envelope far larger than the worst accumulated roundoff of these
    // fixed-size (three-term) products while remaining visually negligible.
    const double maximumSlopeMagnitude = (std::max)(
        (std::max)(std::fabs(left), std::fabs(right)),
        (std::max)(std::fabs(top), std::fabs(bottom)));
    const double slopeEnvelope = 0.00001 * (1.0 + maximumSlopeMagnitude);
    left -= slopeEnvelope;
    right += slopeEnvelope;
    bottom -= slopeEnvelope;
    top += slopeEnvelope;
    // Round every float boundary outward. Round-to-nearest can move a bound
    // inward by one ULP and invalidate the word "conservative."
    result.left = std::nextafter(static_cast<float>(left), negativeInfinity);
    result.right = std::nextafter(static_cast<float>(right), positiveInfinity);
    result.top = std::nextafter(static_cast<float>(top), positiveInfinity);
    result.bottom = std::nextafter(static_cast<float>(bottom), negativeInfinity);
    result.nearDistance = std::nextafter(static_cast<float>(minimumForward), 0.0f);
    result.farDistance = std::nextafter(static_cast<float>(maximumForward), positiveInfinity);
    result.valid = std::isfinite(result.left)
        && std::isfinite(result.right)
        && std::isfinite(result.top)
        && std::isfinite(result.bottom)
        && std::isfinite(result.nearDistance)
        && std::isfinite(result.farDistance)
        && result.left < result.right
        && result.bottom < result.top
        && result.nearDistance > 0.0f
        && result.nearDistance < result.farDistance;
    return result;
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

inline Matrix4 multiply(const Matrix4& lhs, const Matrix4& rhs)
{
    Matrix4 result {};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int inner = 0; inner < 4; ++inner)
                result.m[row][column] += lhs.m[row][inner] * rhs.m[inner][column];
        }
    }
    return result;
}

inline ViewProjectionDelta makeViewProjectionDelta(
    const Matrix4& centerViewProjection,
    const Matrix4& eyeViewProjection,
    bool columnVector)
{
    const ScopedIeeeSubnormalMode ieeeSubnormalMode {};
    ViewProjectionDelta result {};
    bool identical = true;
    for (int row = 0; row < 4; ++row)
    {
        result.matrix.m[row][row] = 1.0f;
        result.referenceMatrix[row][row] = 1.0;
        for (int column = 0; column < 4; ++column)
        {
            if (!std::isfinite(centerViewProjection.m[row][column])
                || !std::isfinite(eyeViewProjection.m[row][column]))
            {
                return result;
            }
            result.centerMatrix[row][column] =
                static_cast<double>(centerViewProjection.m[row][column]);
            result.eyeMatrix[row][column] =
                static_cast<double>(eyeViewProjection.m[row][column]);
            identical = identical
                && centerViewProjection.m[row][column] == eyeViewProjection.m[row][column];
        }
    }
    if (identical)
    {
        result.exactIdentity = true;
        result.inverseResidual = 0.0;
        result.inverseResidualNorm = 0.0;
        result.reconstructionResidual = 0.0;
        result.normalizedReconstructionResidual = 0.0;
        result.valid = true;
        return result;
    }

    double work[4][8] {};
    double centerScale = 0.0;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            work[row][column] = static_cast<double>(centerViewProjection.m[row][column]);
            centerScale = (std::max)(centerScale, std::fabs(work[row][column]));
        }
        work[row][row + 4] = 1.0;
    }
    if (!std::isfinite(centerScale) || centerScale <= 0.0)
        return result;

    for (int column = 0; column < 4; ++column)
    {
        int pivot = column;
        double pivotAbs = std::fabs(work[pivot][column]);
        for (int row = column + 1; row < 4; ++row)
        {
            const double candidateAbs = std::fabs(work[row][column]);
            if (candidateAbs > pivotAbs)
            {
                pivot = row;
                pivotAbs = candidateAbs;
            }
        }
        if (!std::isfinite(pivotAbs)
            || pivotAbs <= centerScale * 32.0 * std::numeric_limits<double>::epsilon())
        {
            return result;
        }
        if (pivot != column)
        {
            for (int index = 0; index < 8; ++index)
                std::swap(work[column][index], work[pivot][index]);
        }

        const double pivotValue = work[column][column];
        for (int index = 0; index < 8; ++index)
            work[column][index] /= pivotValue;
        for (int row = 0; row < 4; ++row)
        {
            if (row == column)
                continue;
            const double factor = work[row][column];
            for (int index = 0; index < 8; ++index)
                work[row][index] -= factor * work[column][index];
        }
    }

    double inverse[4][4] {};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
        {
            inverse[row][column] = work[row][column + 4];
            result.inverseCenterMatrix[row][column] = inverse[row][column];
        }

    double inverseResidual = 0.0;
    for (int order = 0; order < 2; ++order)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                double value = 0.0;
                for (int inner = 0; inner < 4; ++inner)
                {
                    value += order == 0
                        ? static_cast<double>(centerViewProjection.m[row][inner]) * inverse[inner][column]
                        : inverse[row][inner] * static_cast<double>(centerViewProjection.m[inner][column]);
                }
                const double expected = row == column ? 1.0 : 0.0;
                inverseResidual = (std::max)(inverseResidual, std::fabs(value - expected));
            }
        }
    }

    double delta[4][4] {};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int inner = 0; inner < 4; ++inner)
            {
                delta[row][column] += columnVector
                    ? static_cast<double>(eyeViewProjection.m[row][inner]) * inverse[inner][column]
                    : inverse[row][inner] * static_cast<double>(eyeViewProjection.m[inner][column]);
            }
            if (!std::isfinite(delta[row][column]))
                return result;
            result.referenceMatrix[row][column] = delta[row][column];
            result.matrix.m[row][column] = static_cast<float>(delta[row][column]);
        }
    }

    double reconstructionResidual = 0.0;
    double eyeScale = 0.0;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            double reconstructed = 0.0;
            for (int inner = 0; inner < 4; ++inner)
            {
                reconstructed += columnVector
                    ? static_cast<double>(result.matrix.m[row][inner])
                        * static_cast<double>(centerViewProjection.m[inner][column])
                    : static_cast<double>(centerViewProjection.m[row][inner])
                        * static_cast<double>(result.matrix.m[inner][column]);
            }
            const double expected = static_cast<double>(eyeViewProjection.m[row][column]);
            reconstructionResidual = (std::max)(
                reconstructionResidual,
                std::fabs(reconstructed - expected));
            eyeScale = (std::max)(eyeScale, std::fabs(expected));
        }
    }

    result.inverseResidual = inverseResidual;
    result.inverseResidualNorm = inverseResidual * 4.0;
    result.reconstructionResidual = reconstructionResidual;
    result.normalizedReconstructionResidual = reconstructionResidual / (std::max)(1.0, eyeScale);
    result.valid = std::isfinite(result.inverseResidual)
        && std::isfinite(result.reconstructionResidual)
        && std::isfinite(result.inverseResidualNorm)
        && std::isfinite(result.normalizedReconstructionResidual)
        && result.inverseResidual <= 0.00001
        && result.inverseResidualNorm < 1.0
        && result.reconstructionResidual <= 0.01
        && result.normalizedReconstructionResidual <= 0.000001;
    return result;
}

// A shader MVP already contains its draw-local model transform. Replace only
// the camera's exact world-to-clip factor, so the model cancels algebraically:
//   column vectors: eyeVP * inverse(centerVP) * centerVP * model
//   row vectors:    model * centerVP * inverse(centerVP) * eyeVP
inline Matrix4 applyViewProjectionDelta(
    const Matrix4& originalMvp,
    const Matrix4& viewProjectionDelta,
    bool columnVector)
{
    const ScopedIeeeSubnormalMode ieeeSubnormalMode {};
    if (columnVector)
        return multiply(viewProjectionDelta, originalMvp);
    return multiply(originalMvp, viewProjectionDelta);
}

// The D*C ~= E gate above proves the camera factor, but a large draw-local
// model matrix can amplify float error when the actual D*O (or O*D) upload is
// formed. Recover the draw-local model factor, reconstruct it through C, and
// carry the inverse-residual uncertainty through E. Comparing only against
// the same approximate inverse would launder its error.
inline ViewProjectionPatchValidation validateAppliedViewProjectionDelta(
    const Matrix4& originalMvp,
    const Matrix4& patchedMvp,
    const ViewProjectionDelta& delta,
    bool columnVector,
    double maximumAbsoluteErrorAllowed = 0.01,
    double normalizedErrorAllowed = 0.0000001)
{
    const ScopedIeeeSubnormalMode ieeeSubnormalMode {};
    ViewProjectionPatchValidation result {};
    if (!delta.valid
        || !std::isfinite(maximumAbsoluteErrorAllowed)
        || !std::isfinite(normalizedErrorAllowed)
        || maximumAbsoluteErrorAllowed < 0.0
        || normalizedErrorAllowed < 0.0)
    {
        return result;
    }

    // Certify the exact semantic target without comparing against the same
    // approximate inverse used to form the patch. Every primitive operation
    // below is enclosed with nextafter, so the interval contains the exact
    // real operation on the binary input values under IEEE round-to-nearest.
    struct OutwardInterval
    {
        double lower = 0.0;
        double upper = 0.0;
    };
    const double negativeInfinity = -std::numeric_limits<double>::infinity();
    const double positiveInfinity = std::numeric_limits<double>::infinity();
    const auto downward = [negativeInfinity](double value) {
        return std::isfinite(value) ? std::nextafter(value, negativeInfinity) : value;
    };
    const auto upward = [positiveInfinity](double value) {
        return std::isfinite(value) ? std::nextafter(value, positiveInfinity) : value;
    };
    const auto exactInterval = [](double value) {
        return OutwardInterval { value, value };
    };
    const auto addIntervals = [&](OutwardInterval lhs, OutwardInterval rhs) {
        return OutwardInterval {
            downward(lhs.lower + rhs.lower),
            upward(lhs.upper + rhs.upper)
        };
    };
    const auto subtractIntervals = [&](OutwardInterval lhs, OutwardInterval rhs) {
        return OutwardInterval {
            downward(lhs.lower - rhs.upper),
            upward(lhs.upper - rhs.lower)
        };
    };
    const auto multiplyIntervals = [&](OutwardInterval lhs, OutwardInterval rhs) {
        const double products[4] {
            lhs.lower * rhs.lower,
            lhs.lower * rhs.upper,
            lhs.upper * rhs.lower,
            lhs.upper * rhs.upper
        };
        double lower = positiveInfinity;
        double upper = negativeInfinity;
        for (double product : products)
        {
            if (!std::isfinite(product))
                return OutwardInterval { negativeInfinity, positiveInfinity };
            lower = (std::min)(lower, downward(product));
            upper = (std::max)(upper, upward(product));
        }
        return OutwardInterval { lower, upper };
    };
    const auto divideIntervals = [&](OutwardInterval numerator, OutwardInterval denominator) {
        if (denominator.lower <= 0.0 && denominator.upper >= 0.0)
            return OutwardInterval { negativeInfinity, positiveInfinity };
        const OutwardInterval reciprocal {
            downward(1.0 / denominator.upper),
            upward(1.0 / denominator.lower)
        };
        return multiplyIntervals(numerator, reciprocal);
    };
    const auto multiplyIntervalMatrices = [&](
        const OutwardInterval lhs[4][4],
        const OutwardInterval rhs[4][4],
        OutwardInterval output[4][4]) {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                OutwardInterval sum = exactInterval(0.0);
                for (int inner = 0; inner < 4; ++inner)
                    sum = addIntervals(sum, multiplyIntervals(lhs[row][inner], rhs[inner][column]));
                output[row][column] = sum;
            }
        }
    };
    const auto intervalInfinityNormUpper = [&](const OutwardInterval matrix[4][4]) {
        double maximum = 0.0;
        for (int row = 0; row < 4; ++row)
        {
            OutwardInterval rowSum = exactInterval(0.0);
            for (int column = 0; column < 4; ++column)
            {
                const double magnitude = (std::max)(
                    std::fabs(matrix[row][column].lower),
                    std::fabs(matrix[row][column].upper));
                rowSum = addIntervals(rowSum, exactInterval(magnitude));
            }
            maximum = (std::max)(maximum, rowSum.upper);
        }
        return maximum;
    };

    OutwardInterval expectedMatrix[4][4] {};
    if (delta.exactIdentity)
    {
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                expectedMatrix[row][column] = exactInterval(originalMvp.m[row][column]);
    }
    else
    {
        OutwardInterval center[4][4] {};
        OutwardInterval eye[4][4] {};
        OutwardInterval inverse[4][4] {};
        OutwardInterval original[4][4] {};
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                center[row][column] = exactInterval(delta.centerMatrix[row][column]);
                eye[row][column] = exactInterval(delta.eyeMatrix[row][column]);
                inverse[row][column] = exactInterval(delta.inverseCenterMatrix[row][column]);
                original[row][column] = exactInterval(originalMvp.m[row][column]);
            }
        }

        OutwardInterval inverseResidual[4][4] {};
        if (columnVector)
            multiplyIntervalMatrices(center, inverse, inverseResidual);
        else
            multiplyIntervalMatrices(inverse, center, inverseResidual);
        for (int axis = 0; axis < 4; ++axis)
        {
            inverseResidual[axis][axis] = subtractIntervals(
                inverseResidual[axis][axis], exactInterval(1.0));
        }

        OutwardInterval cameraDelta[4][4] {};
        if (columnVector)
            multiplyIntervalMatrices(eye, inverse, cameraDelta);
        else
            multiplyIntervalMatrices(inverse, eye, cameraDelta);

        OutwardInterval reconstructionResidual[4][4] {};
        if (columnVector)
            multiplyIntervalMatrices(inverseResidual, original, reconstructionResidual);
        else
            multiplyIntervalMatrices(original, inverseResidual, reconstructionResidual);

        OutwardInterval approximateExpected[4][4] {};
        OutwardInterval firstCorrection[4][4] {};
        if (columnVector)
        {
            multiplyIntervalMatrices(cameraDelta, original, approximateExpected);
            multiplyIntervalMatrices(cameraDelta, reconstructionResidual, firstCorrection);
        }
        else
        {
            multiplyIntervalMatrices(original, cameraDelta, approximateExpected);
            multiplyIntervalMatrices(reconstructionResidual, cameraDelta, firstCorrection);
        }

        const double residualNorm = intervalInfinityNormUpper(inverseResidual);
        const double cameraDeltaNorm = intervalInfinityNormUpper(cameraDelta);
        const double reconstructionNorm = intervalInfinityNormUpper(reconstructionResidual);
        if (!std::isfinite(residualNorm)
            || !std::isfinite(cameraDeltaNorm)
            || !std::isfinite(reconstructionNorm)
            || residualNorm >= 1.0)
        {
            return result;
        }
        const OutwardInterval neumannRatio = divideIntervals(
            exactInterval(residualNorm),
            subtractIntervals(exactInterval(1.0), exactInterval(residualNorm)));
        const OutwardInterval remainderInterval = multiplyIntervals(
            multiplyIntervals(exactInterval(cameraDeltaNorm), neumannRatio),
            exactInterval(reconstructionNorm));
        const double remainder = remainderInterval.upper;
        if (!std::isfinite(remainder) || remainder < 0.0)
            return result;

        const OutwardInterval remainderWidth { -remainder, remainder };
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                expectedMatrix[row][column] = addIntervals(
                    subtractIntervals(approximateExpected[row][column], firstCorrection[row][column]),
                    remainderWidth);
            }
        }
    }

    double maximumError = 0.0;
    double expectedScaleLowerBound = 0.0;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            const double actual = static_cast<double>(patchedMvp.m[row][column]);
            const OutwardInterval expected = expectedMatrix[row][column];
            if (!std::isfinite(expected.lower)
                || !std::isfinite(expected.upper)
                || !std::isfinite(actual)
                || expected.lower > expected.upper)
            {
                return result;
            }
            const double errorUpper = upward((std::max)(
                std::fabs(actual - expected.lower),
                std::fabs(actual - expected.upper)));
            maximumError = (std::max)(
                maximumError,
                errorUpper);
            const double entryMagnitudeLowerBound = expected.lower > 0.0
                ? expected.lower
                : (expected.upper < 0.0 ? -expected.upper : 0.0);
            expectedScaleLowerBound = (std::max)(
                expectedScaleLowerBound,
                entryMagnitudeLowerBound);
        }
    }

    result.maximumAbsoluteError = maximumError;
    result.normalizedError = upward(
        maximumError / (std::max)(1.0, expectedScaleLowerBound));
    result.valid = std::isfinite(result.maximumAbsoluteError)
        && std::isfinite(result.normalizedError)
        && result.maximumAbsoluteError <= maximumAbsoluteErrorAllowed
        && result.normalizedError <= normalizedErrorAllowed;
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

// The shader-coverage denominator must describe draws the stereo producer is
// actually permitted to replay. Fallout uses Draw*PrimitiveUP for immediate
// UI geometry; counting those as world candidates while the replay policy
// rejects them makes 100% coverage mathematically unreachable.
inline bool programmableWorldDrawBasis(
    bool nativeCenterTraversal,
    bool uiSuppressed,
    bool haveVertexShader,
    bool haveProjection,
    bool provenScreenSpaceDraw,
    bool samplesStereoTwin,
    bool configuredSkip,
    bool userPrimitiveDraw,
    bool replayUserPrimitiveDraws)
{
    return nativeCenterTraversal
        && !uiSuppressed
        && haveVertexShader
        && haveProjection
        && !provenScreenSpaceDraw
        && !samplesStereoTwin
        && !configuredSkip
        && (!userPrimitiveDraw || replayUserPrimitiveDraws);
}

// Only the full-resolution scene target contributes pixels to the stereo eye
// images. Auxiliary world passes (shadow maps, reflections, render textures)
// may use the same world shaders, but the strict target gate deliberately
// leaves them mono and they must not make eye-image coverage unreachable.
inline bool programmableWorldDrawCandidate(bool drawBasis, bool stereoTargetEligible)
{
    return drawBasis && stereoTargetEligible;
}

inline bool strictEyeTargetDrawLedgerComplete(long totalDraws, long provenBothEyeDraws)
{
    return totalDraws > 0 && provenBothEyeDraws == totalDraws;
}

inline bool strictEyeTargetOptionalWriteLedgerComplete(long totalWrites, long provenBothEyeWrites)
{
    return totalWrites >= 0 && provenBothEyeWrites == totalWrites;
}

// A configured single-traversal producer owns the headset transaction. If its
// exact camera/pose/contract proof fails, generic draw replay must not silently
// replace it with pixels that have weaker pose ownership. The caller can retain
// the last coherent transaction while the next complete pair is produced.
inline bool singleTraversalPublishAllowed(bool singleTraversalEnabled, bool completePair)
{
    return !singleTraversalEnabled || completePair;
}

// Count only one uninterrupted producer stream toward the stereo handoff.
// A restarted producer can reuse a sequence value or jump backwards, and a
// reference-space reset changes the meaning of every cached eye pose.  The
// first coherent frame after either event starts a new run at one; it must not
// inherit warm-up credit from the previous coordinate system.
inline std::uint64_t nextStereoStableFrameCount(
    std::uint64_t previousCount,
    std::int32_t previousSequence,
    std::int32_t currentSequence,
    std::uint64_t previousProducerEpoch,
    std::uint64_t currentProducerEpoch,
    std::uint64_t previousRendererProducerEpoch,
    std::uint64_t currentRendererProducerEpoch,
    std::uint32_t previousReferenceGeneration,
    std::uint32_t currentReferenceGeneration,
    std::uint32_t previousProducerProcessId,
    std::uint32_t currentProducerProcessId)
{
    if (fnvxr::shared::sequencedValueBits(currentSequence) == 0u
        || currentProducerEpoch == 0
        || currentRendererProducerEpoch == 0
        || currentReferenceGeneration == 0
        || currentProducerProcessId == 0)
    {
        return 0;
    }

    const bool havePreviousIdentity = previousProducerEpoch != 0
        && previousRendererProducerEpoch != 0
        && previousReferenceGeneration != 0
        && previousProducerProcessId != 0;
    const bool identityChanged = havePreviousIdentity
        && (previousProducerEpoch != currentProducerEpoch
            || previousRendererProducerEpoch != currentRendererProducerEpoch
            || previousReferenceGeneration != currentReferenceGeneration
            || previousProducerProcessId != currentProducerProcessId);
    const bool havePreviousSequence =
        fnvxr::shared::sequencedValueBits(previousSequence) != 0u;
    if (!havePreviousSequence
        || !havePreviousIdentity
        || identityChanged
        || !fnvxr::shared::nonzeroSharedCounterAdvanced(
            currentSequence, previousSequence))
    {
        return 1;
    }

    return previousCount == (std::numeric_limits<std::uint64_t>::max)()
        ? previousCount
        : previousCount + 1;
}

// A projection layer without depth can only use the compositor's rotational
// reprojection exactly. Keep translation error bounded by rejecting pixels
// whose source pose is too old (or implausibly far in the future).
inline bool sourcePoseAgeWithinBudget(
    std::int64_t currentPredictedDisplayTime,
    std::int64_t sourceRenderedDisplayTime,
    std::int64_t maximumAgeNanoseconds,
    std::int64_t futureToleranceNanoseconds,
    std::int64_t* ageNanoseconds = nullptr)
{
    if (currentPredictedDisplayTime <= 0
        || sourceRenderedDisplayTime <= 0
        || maximumAgeNanoseconds < 0
        || futureToleranceNanoseconds < 0)
    {
        if (ageNanoseconds)
            *ageNanoseconds = 0;
        return false;
    }

    const std::int64_t age = currentPredictedDisplayTime - sourceRenderedDisplayTime;
    if (ageNanoseconds)
        *ageNanoseconds = age;
    return age <= maximumAgeNanoseconds && age >= -futureToleranceNanoseconds;
}
}
