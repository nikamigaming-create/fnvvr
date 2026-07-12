#include "fnvxr_stereo_math.h"

#include <cmath>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << "\n";
    return 1;
}

bool nearlyEqual(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.00001f;
}
}

int main()
{
    fnvxr::stereo::Matrix4 view {};
    fnvxr::stereo::Matrix4 projection {};
    for (int i = 0; i < 4; ++i)
    {
        view.m[i][i] = 1.0f;
        projection.m[i][i] = 1.0f;
    }
    view.m[3][0] = 12.0f;
    view.m[3][1] = -3.0f;
    view.m[3][2] = 7.5f;

    const auto eyes = fnvxr::stereo::makeEyeMatrices(view, projection, fnvxr::stereo::DefaultIpdGameUnits);
    const float halfIpd = fnvxr::stereo::DefaultIpdGameUnits * 0.5f;

    if (!nearlyEqual(eyes.leftView.m[3][0], 12.0f + halfIpd))
        return fail("left eye view X should move by positive half game-unit IPD in FNV view space");

    if (!nearlyEqual(eyes.rightView.m[3][0], 12.0f - halfIpd))
        return fail("right eye view X should move by negative half game-unit IPD in FNV view space");

    if (!nearlyEqual(eyes.leftView.m[3][1], view.m[3][1]) || !nearlyEqual(eyes.rightView.m[3][2], view.m[3][2]))
        return fail("eye split should not disturb non-X view translation");

    if (!nearlyEqual(eyes.leftProjection.m[0][0], projection.m[0][0])
        || !nearlyEqual(eyes.rightProjection.m[3][3], projection.m[3][3]))
    {
        return fail("projection should pass through until asymmetric OpenXR frusta are wired");
    }

    if (!fnvxr::stereo::isFinite(eyes.leftView) || !fnvxr::stereo::isFinite(eyes.rightView))
        return fail("eye matrices should stay finite");

    std::cout << "stereo math ok\n";
    return 0;
}
