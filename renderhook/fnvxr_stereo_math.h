#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>

namespace fnvxr::stereo
{
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
    double inverseResidual = std::numeric_limits<double>::infinity();
    double reconstructionResidual = std::numeric_limits<double>::infinity();
    double normalizedReconstructionResidual = std::numeric_limits<double>::infinity();
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
    float minimumMeters = 0.03f,
    float maximumMeters = 0.12f,
    float minimumLateralFraction = 0.8f)
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
    result.valid = std::isfinite(result.lengthMeters)
        && result.lengthMeters >= minimumMeters
        && result.lengthMeters <= maximumMeters
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
    if (!std::isfinite(nearDistance)
        || !std::isfinite(farDistance)
        || nearDistance <= 0.0f
        || farDistance <= nearDistance)
    {
        return result;
    }

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
        if (!std::isfinite(eye.left)
            || !std::isfinite(eye.right)
            || !std::isfinite(eye.top)
            || !std::isfinite(eye.bottom)
            || eye.right <= eye.left
            || eye.top <= eye.bottom)
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
                    if (!std::isfinite(x)
                        || !std::isfinite(y)
                        || !std::isfinite(forward)
                        || forward <= 0.000001)
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

    const float negativeInfinity = -std::numeric_limits<float>::infinity();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    // Round every float boundary outward. Round-to-nearest can move a bound
    // inward by one ULP and invalidate the word "conservative."
    result.left = std::nextafter(static_cast<float>(left), negativeInfinity);
    result.right = std::nextafter(static_cast<float>(right), positiveInfinity);
    result.top = std::nextafter(static_cast<float>(top), positiveInfinity);
    result.bottom = std::nextafter(static_cast<float>(bottom), negativeInfinity);
    result.nearDistance = std::nextafter(static_cast<float>(minimumForward), 0.0f);
    result.farDistance = std::nextafter(static_cast<float>(maximumForward), positiveInfinity);
    result.valid = true;
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
            identical = identical
                && centerViewProjection.m[row][column] == eyeViewProjection.m[row][column];
        }
    }
    if (identical)
    {
        result.inverseResidual = 0.0;
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
            inverse[row][column] = work[row][column + 4];

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
    result.reconstructionResidual = reconstructionResidual;
    result.normalizedReconstructionResidual = reconstructionResidual / (std::max)(1.0, eyeScale);
    result.valid = std::isfinite(result.inverseResidual)
        && std::isfinite(result.reconstructionResidual)
        && std::isfinite(result.normalizedReconstructionResidual)
        && result.inverseResidual <= 0.00001
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
    if (columnVector)
        return multiply(viewProjectionDelta, originalMvp);
    return multiply(originalMvp, viewProjectionDelta);
}

// The D*C ~= E gate above proves the camera factor, but a large draw-local
// model matrix can amplify float error when the actual D*O (or O*D) upload is
// formed. Compare that final float upload to the same operation using the
// retained double-precision delta and reject before touching shader state.
inline ViewProjectionPatchValidation validateAppliedViewProjectionDelta(
    const Matrix4& originalMvp,
    const Matrix4& patchedMvp,
    const ViewProjectionDelta& delta,
    bool columnVector,
    double maximumAbsoluteErrorAllowed = 0.01,
    double normalizedErrorAllowed = 0.0000001)
{
    ViewProjectionPatchValidation result {};
    if (!delta.valid
        || maximumAbsoluteErrorAllowed < 0.0
        || normalizedErrorAllowed < 0.0)
    {
        return result;
    }

    double maximumError = 0.0;
    double expectedScale = 0.0;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            double expected = 0.0;
            for (int inner = 0; inner < 4; ++inner)
            {
                expected += columnVector
                    ? delta.referenceMatrix[row][inner]
                        * static_cast<double>(originalMvp.m[inner][column])
                    : static_cast<double>(originalMvp.m[row][inner])
                        * delta.referenceMatrix[inner][column];
            }
            const double actual = static_cast<double>(patchedMvp.m[row][column]);
            if (!std::isfinite(expected) || !std::isfinite(actual))
                return result;
            maximumError = (std::max)(maximumError, std::fabs(actual - expected));
            expectedScale = (std::max)(expectedScale, std::fabs(expected));
        }
    }

    result.maximumAbsoluteError = maximumError;
    result.normalizedError = maximumError / (std::max)(1.0, expectedScale);
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
