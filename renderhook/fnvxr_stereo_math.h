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
}
