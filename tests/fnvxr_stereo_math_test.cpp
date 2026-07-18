#include "fnvxr_stereo_math.h"

#include <cmath>
#include <iostream>
#include <limits>

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

fnvxr::stereo::Quaternion axisAngle(float x, float y, float z, float radians)
{
    const float half = radians * 0.5f;
    const float sine = std::sin(half);
    return fnvxr::stereo::normalized({ x * sine, y * sine, z * sine, std::cos(half) });
}

bool vectorNear(
    const fnvxr::stereo::Vector3& value,
    float x,
    float y,
    float z,
    float epsilon = 0.0001f)
{
    return std::fabs(value.x - x) < epsilon
        && std::fabs(value.y - y) < epsilon
        && std::fabs(value.z - z) < epsilon;
}

bool matrixNear(
    const fnvxr::stereo::Matrix4& lhs,
    const fnvxr::stereo::Matrix4& rhs,
    float epsilon = 0.0001f)
{
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            if (!std::isfinite(lhs.m[row][column])
                || !std::isfinite(rhs.m[row][column])
                || std::fabs(lhs.m[row][column] - rhs.m[row][column]) >= epsilon)
                return false;
        }
    }
    return true;
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

    if (!nearlyEqual(fnvxr::stereo::DefaultGameUnitsPerMeter * 0.10f, 7.0f)
        || !nearlyEqual(fnvxr::stereo::DefaultIpdGameUnits, 4.48f))
    {
        return fail("FNV physical scale must be 70 units/m and 64 mm IPD must be 4.48 units");
    }
    if (fnvxr::stereo::certifiedStereoTextureMayBeSampled(true, false, true, true)
        || !fnvxr::stereo::certifiedStereoTextureMayBeSampled(true, true, true, true))
    {
        return fail("mono fullscreen fallback must never sample a rejected or stale per-eye texture");
    }

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

    const auto symmetricProjection = fnvxr::stereo::projectionFromTangents(
        projection, -1.0f, 1.0f, 1.0f, -1.0f);
    if (!nearlyEqual(symmetricProjection.m[0][0], 1.0f)
        || !nearlyEqual(symmetricProjection.m[1][1], 1.0f)
        || !nearlyEqual(symmetricProjection.m[2][0], 0.0f)
        || !nearlyEqual(symmetricProjection.m[2][1], 0.0f)
        || !nearlyEqual(symmetricProjection.m[2][2], projection.m[2][2]))
    {
        return fail("symmetric OpenXR tangents should preserve a centered D3D projection and its depth terms");
    }

    const auto asymmetricProjection = fnvxr::stereo::projectionFromTangents(
        projection, -1.2f, 0.8f, 1.1f, -0.9f);
    if (!nearlyEqual(asymmetricProjection.m[0][0], 1.0f)
        || !nearlyEqual(asymmetricProjection.m[1][1], 1.0f)
        || !nearlyEqual(asymmetricProjection.m[2][0], 0.2f)
        || !nearlyEqual(asymmetricProjection.m[2][1], -0.1f))
    {
        return fail("asymmetric OpenXR tangents should map to the D3D off-center projection terms");
    }

    if (!fnvxr::stereo::isFinite(eyes.leftView) || !fnvxr::stereo::isFinite(eyes.rightView))
        return fail("eye matrices should stay finite");

    // The retail shader path receives exact combined NiCamera world-to-clip
    // matrices. Prove both shader conventions replace only that factor while
    // preserving an arbitrary draw-local model matrix.
    fnvxr::stereo::Matrix4 centerVp {};
    fnvxr::stereo::Matrix4 inverseCenterVp {};
    fnvxr::stereo::Matrix4 eyeVp {};
    fnvxr::stereo::Matrix4 model {};
    centerVp.m[0][0] = 2.0f;
    centerVp.m[1][1] = 3.0f;
    centerVp.m[2][2] = 4.0f;
    centerVp.m[3][3] = 1.0f;
    inverseCenterVp.m[0][0] = 0.5f;
    inverseCenterVp.m[1][1] = 1.0f / 3.0f;
    inverseCenterVp.m[2][2] = 0.25f;
    inverseCenterVp.m[3][3] = 1.0f;
    eyeVp = centerVp;
    eyeVp.m[0][3] = 0.125f;
    eyeVp.m[1][3] = -0.25f;
    model.m[0][0] = 0.5f;
    model.m[1][1] = 0.75f;
    model.m[2][2] = 1.25f;
    model.m[3][3] = 1.0f;
    model.m[0][3] = 7.0f;
    model.m[1][3] = -4.0f;
    model.m[2][3] = 2.0f;

    const auto columnOriginal = fnvxr::stereo::multiply(centerVp, model);
    const auto columnExpected = fnvxr::stereo::multiply(eyeVp, model);
    const auto columnDelta = fnvxr::stereo::makeViewProjectionDelta(centerVp, eyeVp, true);
    if (!columnDelta.valid)
        return fail("column-vector view-projection delta must pass its numerical residual gate");
    const auto columnPatched = fnvxr::stereo::applyViewProjectionDelta(
        columnOriginal, columnDelta.matrix, true);
    const auto columnPatchValidation = fnvxr::stereo::validateAppliedViewProjectionDelta(
        columnOriginal, columnPatched, columnDelta, true);
    if (!matrixNear(columnPatched, columnExpected) || !columnPatchValidation.valid)
        return fail("column-vector shader MVP delta must replace the exact center NiCamera world-to-clip factor");

    const auto rowOriginal = fnvxr::stereo::multiply(model, centerVp);
    const auto rowExpected = fnvxr::stereo::multiply(model, eyeVp);
    const auto rowDelta = fnvxr::stereo::makeViewProjectionDelta(centerVp, eyeVp, false);
    if (!rowDelta.valid)
        return fail("row-vector view-projection delta must pass its numerical residual gate");
    const auto rowPatched = fnvxr::stereo::applyViewProjectionDelta(
        rowOriginal, rowDelta.matrix, false);
    const auto rowPatchValidation = fnvxr::stereo::validateAppliedViewProjectionDelta(
        rowOriginal, rowPatched, rowDelta, false);
    if (!matrixNear(rowPatched, rowExpected) || !rowPatchValidation.valid)
        return fail("row-vector shader MVP delta must replace the exact center NiCamera world-to-clip factor");

    // Captured retail NiCamera center world-to-clip matrix. Its condition
    // number is about 1.77e9, so the previous float Gauss-Jordan inverse could
    // corrupt even E=C by several clip-space units. The new double-precision
    // delta builder has an exact identity path and a reconstruction gate.
    const fnvxr::stereo::Matrix4 retailCenter {{
        { -0.456312f, -0.565370f, 0.0f, -33734.3f },
        { 0.150332f, -0.121333f, 0.835479f, 3832.55f },
        { 0.778176f, -0.628068f, 0.0f, 55550.0f },
        { 0.778165f, -0.628060f, 0.0f, 55554.2f }
    }};
    const auto retailIdentityDelta =
        fnvxr::stereo::makeViewProjectionDelta(retailCenter, retailCenter, true);
    if (!retailIdentityDelta.valid
        || retailIdentityDelta.reconstructionResidual != 0.0
        || !matrixNear(retailIdentityDelta.matrix, projection))
    {
        return fail("captured ill-conditioned retail matrix must produce an exact identity delta when E equals C");
    }
    const auto retailOriginal = fnvxr::stereo::multiply(retailCenter, model);
    const auto retailIdentityPatched = fnvxr::stereo::applyViewProjectionDelta(
        retailOriginal,
        retailIdentityDelta.matrix,
        true);
    if (!matrixNear(retailIdentityPatched, retailOriginal))
        return fail("identity eye delta must leave a retail-scale shader WVP unchanged");

    fnvxr::stereo::Matrix4 syntheticEyeDelta = projection;
    syntheticEyeDelta.m[0][2] = 0.00025f;
    syntheticEyeDelta.m[1][2] = -0.00015f;
    const auto retailEye = fnvxr::stereo::multiply(syntheticEyeDelta, retailCenter);
    const auto recoveredRetailDelta =
        fnvxr::stereo::makeViewProjectionDelta(retailCenter, retailEye, true);
    if (!recoveredRetailDelta.valid
        || recoveredRetailDelta.reconstructionResidual > 0.01
        || recoveredRetailDelta.normalizedReconstructionResidual > 0.000001)
    {
        return fail("captured retail matrix eye delta must pass double-precision reconstruction gates");
    }
    const auto retailExpected = fnvxr::stereo::multiply(retailEye, model);
    const auto retailPatched = fnvxr::stereo::applyViewProjectionDelta(
        retailOriginal,
        recoveredRetailDelta.matrix,
        true);
    const auto retailPatchValidation = fnvxr::stereo::validateAppliedViewProjectionDelta(
        retailOriginal,
        retailPatched,
        recoveredRetailDelta,
        true,
        0.02,
        0.000001);
    if (!matrixNear(retailPatched, retailExpected, 0.02f) || !retailPatchValidation.valid)
        return fail("captured retail matrix delta must reproduce the eye WVP within the gated float tolerance");
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    const unsigned int savedMxcsr = _mm_getcsr();
    const unsigned int forcedFlushMxcsr = savedMxcsr | 0x0040u | 0x8000u;
    _mm_setcsr(forcedFlushMxcsr);
    const auto flushModeDelta = fnvxr::stereo::makeViewProjectionDelta(retailCenter, retailEye, true);
    const auto flushModePatched = fnvxr::stereo::applyViewProjectionDelta(
        retailOriginal, flushModeDelta.matrix, true);
    const auto flushModeValidation = fnvxr::stereo::validateAppliedViewProjectionDelta(
        retailOriginal, flushModePatched, flushModeDelta, true, 0.02, 0.000001);
    const bool callerMxcsrRestored = _mm_getcsr() == forcedFlushMxcsr;
    _mm_setcsr(savedMxcsr);
    if (!flushModeDelta.valid || !flushModeValidation.valid || !callerMxcsrRestored)
    {
        return fail("WVP certification must temporarily disable FTZ/DAZ and restore the caller MXCSR");
    }
#endif

    fnvxr::stereo::Matrix4 amplifiedRetailWvp {};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            amplifiedRetailWvp.m[row][column] = static_cast<float>((row + 1) * (column + 3)) * 100000000.0f;
    const auto amplifiedRetailPatched = fnvxr::stereo::applyViewProjectionDelta(
        amplifiedRetailWvp,
        recoveredRetailDelta.matrix,
        true);
    const auto amplifiedPatchValidation = fnvxr::stereo::validateAppliedViewProjectionDelta(
        amplifiedRetailWvp,
        amplifiedRetailPatched,
        recoveredRetailDelta,
        true);
    if (amplifiedPatchValidation.valid
        || amplifiedPatchValidation.maximumAbsoluteError <= 0.01)
    {
        return fail("draw-local amplification above the final patched-WVP precision budget must fail closed");
    }

    const fnvxr::stereo::Matrix4 inverseUncertaintyWitness {{
        { -2.5619666e4f, -45.183163f, 158.06393f, 9.8419256f },
        { -4.3579812f, 9.2319419e5f, 23.570318f, -9.1408789e4f },
        { 471.71097f, 6.9220777f, -3.2614128e8f, 1.2684435e6f },
        { -36.367538f, 9.2181453e8f, -1.5004458e6f, -2.0689106e7f }
    }};
    const auto inverseUncertaintyPatched = fnvxr::stereo::applyViewProjectionDelta(
        inverseUncertaintyWitness,
        recoveredRetailDelta.matrix,
        true);
    const auto inverseUncertaintyValidation =
        fnvxr::stereo::validateAppliedViewProjectionDelta(
            inverseUncertaintyWitness,
            inverseUncertaintyPatched,
            recoveredRetailDelta,
            true);
    if (inverseUncertaintyValidation.valid
        || inverseUncertaintyValidation.maximumAbsoluteError <= 0.01)
    {
        return fail("inverse uncertainty amplified by a draw-local WVP must fail the exact semantic budget");
    }
    if (fnvxr::stereo::validateAppliedViewProjectionDelta(
            retailOriginal,
            retailPatched,
            recoveredRetailDelta,
            true,
            std::numeric_limits<double>::infinity(),
            0.000001).valid)
    {
        return fail("nonfinite patched-WVP tolerance must fail closed");
    }

    constexpr float Pi = 3.14159265358979323846f;
    const auto origin = fnvxr::stereo::multiply(
        axisAngle(0.0f, 1.0f, 0.0f, 0.7f),
        fnvxr::stereo::multiply(
            axisAngle(1.0f, 0.0f, 0.0f, -0.3f),
            axisAngle(0.0f, 0.0f, 1.0f, 0.2f)));
    const auto bodyOrigin = fnvxr::stereo::gravityAlignedYawOrientation(origin);
    if (!nearlyEqual(bodyOrigin.x, 0.0f) || !nearlyEqual(bodyOrigin.z, 0.0f))
    {
        return fail("gameplay recenter origin must remain aligned with OpenXR gravity");
    }
    const auto physicalTiltAtRecenter = fnvxr::stereo::relativeOrientation(bodyOrigin, origin);
    if (std::fabs(physicalTiltAtRecenter.x) + std::fabs(physicalTiltAtRecenter.z) < 0.01f)
    {
        return fail("gravity-aligned recenter must not erase physical headset pitch and roll");
    }
    const auto retainedYaw = axisAngle(0.0f, 1.0f, 0.0f, 0.65f);
    const auto verticalLook = axisAngle(1.0f, 0.0f, 0.0f, Pi * 0.5f);
    const auto verticalRecenter =
        fnvxr::stereo::gravityAlignedYawOrientation(verticalLook, retainedYaw);
    if (std::fabs(verticalRecenter.y - retainedYaw.y) > 0.0001f
        || std::fabs(verticalRecenter.w - retainedYaw.w) > 0.0001f)
    {
        return fail("vertical-look recenter must retain the previous yaw instead of latching atan2 noise");
    }
    const auto validEyeBaseline = fnvxr::stereo::validateEyeBaseline(
        {},
        { -0.032f, 0.0f, 0.0f },
        { 0.032f, 0.0f, 0.0f });
    if (!validEyeBaseline.valid || !nearlyEqual(validEyeBaseline.lengthMeters, 0.064f))
        return fail("signed 64 mm right-minus-left eye baseline must validate in head-local space");
    const auto swappedEyeBaseline = fnvxr::stereo::validateEyeBaseline(
        {},
        { 0.032f, 0.0f, 0.0f },
        { -0.032f, 0.0f, 0.0f });
    const auto verticalEyeBaseline = fnvxr::stereo::validateEyeBaseline(
        {},
        { 0.0f, -0.032f, 0.0f },
        { 0.0f, 0.032f, 0.0f });
    if (swappedEyeBaseline.valid || verticalEyeBaseline.valid)
        return fail("swapped or non-lateral eye baselines must fail closed");
    const auto displacedEyeBaseline = fnvxr::stereo::validateEyeBaseline(
        {},
        { 49.968f, 0.0f, 0.0f },
        { 50.032f, 0.0f, 0.0f },
        {});
    if (displacedEyeBaseline.valid)
        return fail("eye midpoint must remain close to the current HMD position");
    const float validFov[4] { -0.9f, 1.0f, 0.95f, -1.05f };
    const float wrappedFov[4] { -3.0f, 3.0f, 1.0f, -1.0f };
    if (!fnvxr::stereo::openXrFovAnglesUsable(validFov)
        || fnvxr::stereo::openXrFovAnglesUsable(wrappedFov))
    {
        return fail("OpenXR FOV validation must reject angles whose tangents wrap or reverse bounds");
    }

    const fnvxr::stereo::Vector3 originPosition { 4.0f, -2.0f, 7.0f };
    const fnvxr::stereo::Vector3 expectedLocalPosition { 0.25f, -0.10f, -0.40f };
    const auto originRotation = fnvxr::stereo::columnRotationFromQuaternion(bodyOrigin);
    const auto worldPositionDelta = fnvxr::stereo::transform(originRotation, expectedLocalPosition);
    const fnvxr::stereo::Vector3 currentPosition {
        originPosition.x + worldPositionDelta.x,
        originPosition.y + worldPositionDelta.y,
        originPosition.z + worldPositionDelta.z
    };
    const auto actualLocalPosition = fnvxr::stereo::positionInOriginFrame(
        bodyOrigin,
        originPosition,
        currentPosition);
    if (!vectorNear(
            actualLocalPosition,
            expectedLocalPosition.x,
            expectedLocalPosition.y,
            expectedLocalPosition.z))
    {
        return fail("head translation must use the same gravity-aligned recenter frame as head rotation");
    }

    const auto worldYawDelta = axisAngle(0.0f, 1.0f, 0.0f, 20.0f * Pi / 180.0f);
    const auto headAtOrigin = fnvxr::stereo::relativeOrientation(bodyOrigin, origin);
    const auto headAfterWorldYaw = fnvxr::stereo::relativeOrientation(
        bodyOrigin,
        fnvxr::stereo::multiply(worldYawDelta, origin));
    const auto measuredWorldYaw = fnvxr::stereo::multiply(
        headAfterWorldYaw,
        fnvxr::stereo::conjugated(headAtOrigin));
    if (std::fabs(measuredWorldYaw.x) > 0.0001f
        || std::fabs(measuredWorldYaw.z) > 0.0001f)
    {
        return fail("tilted recenter must not mix world-up yaw into pitch or roll");
    }

    const auto horizontalRoomMove = fnvxr::stereo::positionInOriginFrame(
        bodyOrigin,
        originPosition,
        { originPosition.x + 0.10f, originPosition.y, originPosition.z });
    if (std::fabs(horizontalRoomMove.y) > 0.0001f)
        return fail("tilted recenter must not turn horizontal room translation into vertical motion");

    const auto gamePosition = fnvxr::stereo::xrVectorToGamebryo(expectedLocalPosition);
    if (!vectorNear(gamePosition, 0.25f, 0.40f, -0.10f))
        return fail("OpenXR position must map to Gamebryo right/forward/up axes");

    const auto cameraLocalPosition =
        fnvxr::stereo::xrVectorToNiCameraLocal(expectedLocalPosition);
    if (!vectorNear(cameraLocalPosition, 0.25f, -0.10f, -0.40f))
        return fail("OpenXR position must remain right/up/back in NiCamera-local space");

    const fnvxr::stereo::Vector3 gameForward { 0.0f, 1.0f, 0.0f };
    const fnvxr::stereo::Vector3 gameRight { 1.0f, 0.0f, 0.0f };
    const fnvxr::stereo::Vector3 gameUp { 0.0f, 0.0f, 1.0f };

    const auto yawLeft = fnvxr::stereo::gamebryoHeadRotation(
        axisAngle(0.0f, 1.0f, 0.0f, Pi * 0.5f));
    if (!vectorNear(fnvxr::stereo::transform(yawLeft, gameForward), -1.0f, 0.0f, 0.0f)
        || !vectorNear(fnvxr::stereo::transform(yawLeft, gameUp), 0.0f, 0.0f, 1.0f))
    {
        return fail("OpenXR yaw must turn Gamebryo forward horizontally without changing up");
    }

    const auto pitchUp = fnvxr::stereo::gamebryoHeadRotation(
        axisAngle(1.0f, 0.0f, 0.0f, Pi * 0.5f));
    if (!vectorNear(fnvxr::stereo::transform(pitchUp, gameForward), 0.0f, 0.0f, 1.0f)
        || !vectorNear(fnvxr::stereo::transform(pitchUp, gameRight), 1.0f, 0.0f, 0.0f))
    {
        return fail("OpenXR pitch must rotate Gamebryo forward toward up around camera right");
    }

    const auto roll = fnvxr::stereo::gamebryoHeadRotation(
        axisAngle(0.0f, 0.0f, 1.0f, Pi * 0.5f));
    if (!vectorNear(fnvxr::stereo::transform(roll, gameForward), 0.0f, 1.0f, 0.0f)
        || !vectorNear(fnvxr::stereo::transform(roll, gameRight), 0.0f, 0.0f, 1.0f))
    {
        return fail("OpenXR roll must rotate around Gamebryo forward without steering the view");
    }

    const auto bodyYaw = fnvxr::stereo::gamebryoHeadRotation(
        axisAngle(0.0f, 1.0f, 0.0f, -0.65f));
    const auto combined = fnvxr::stereo::composeBodyAndHead(bodyYaw, yawLeft);
    const auto expectedCombinedForward = fnvxr::stereo::transform(
        bodyYaw,
        fnvxr::stereo::transform(yawLeft, gameForward));
    const auto actualCombinedForward = fnvxr::stereo::transform(combined, gameForward);
    if (!vectorNear(
            actualCombinedForward,
            expectedCombinedForward.x,
            expectedCombinedForward.y,
            expectedCombinedForward.z))
    {
        return fail("head rotation must remain body-local at arbitrary player headings");
    }

    // Captured from the retail NiCamera immediately before the sideways
    // regression.  Its columns are camera right, up, and back.  Leveling must
    // retain that layout instead of replacing it with an actor/world yaw
    // matrix whose third column is world-up.
    const fnvxr::stereo::Matrix3 capturedCameraWorld {{
        { 0.010f, 0.250f, -0.968f },
        { -1.000f, 0.003f, -0.010f },
        { -0.000f, 0.968f, 0.250f }
    }};
    const auto levelCamera = fnvxr::stereo::gravityLevelCameraWorldRotation(capturedCameraWorld);
    const fnvxr::stereo::Vector3 cameraLocalRight { 1.0f, 0.0f, 0.0f };
    const fnvxr::stereo::Vector3 cameraLocalUp { 0.0f, 1.0f, 0.0f };
    const fnvxr::stereo::Vector3 cameraLocalForward { 0.0f, 0.0f, -1.0f };
    const auto levelRight = fnvxr::stereo::transform(levelCamera, cameraLocalRight);
    const auto levelUp = fnvxr::stereo::transform(levelCamera, cameraLocalUp);
    const auto levelForward = fnvxr::stereo::transform(levelCamera, cameraLocalForward);
    if (std::fabs(levelRight.z) > 0.0001f
        || !vectorNear(levelUp, 0.0f, 0.0f, 1.0f)
        || std::fabs(levelForward.z) > 0.0001f)
    {
        return fail("leveled NiCamera must keep right/forward horizontal and camera up on world +Z");
    }
    const float capturedRightLength = std::sqrt(
        capturedCameraWorld.m[0][0] * capturedCameraWorld.m[0][0]
        + capturedCameraWorld.m[1][0] * capturedCameraWorld.m[1][0]);
    if (!vectorNear(
            levelRight,
            capturedCameraWorld.m[0][0] / capturedRightLength,
            capturedCameraWorld.m[1][0] / capturedRightLength,
            0.0f))
    {
        return fail("leveled NiCamera must preserve the engine-authored horizontal heading");
    }

    // Regression witness for the native 6DoF translation bug: NiCamera's
    // columns are right/up/back. A 10 cm OpenXR forward move is local -Z and
    // must follow the leveled camera-forward column without gaining height.
    // The actor mapping would turn it into +Y, which is camera-up here.
    const auto nativeForwardLocal = fnvxr::stereo::xrVectorToNiCameraLocal({ 0.0f, 0.0f, -0.10f });
    const auto nativeForwardWorld = fnvxr::stereo::transform(levelCamera, nativeForwardLocal);
    if (!vectorNear(
            nativeForwardWorld,
            levelForward.x * 0.10f,
            levelForward.y * 0.10f,
            levelForward.z * 0.10f))
    {
        return fail("native OpenXR forward translation must follow NiCamera forward, not camera up");
    }
    const auto nativeUpLocal = fnvxr::stereo::xrVectorToNiCameraLocal({ 0.0f, 0.10f, 0.0f });
    const auto nativeUpWorld = fnvxr::stereo::transform(levelCamera, nativeUpLocal);
    if (!vectorNear(nativeUpWorld, 0.0f, 0.0f, 0.10f))
        return fail("native OpenXR height must follow NiCamera up, not camera back");

    fnvxr::stereo::Matrix3 identity3 {{
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    }};
    constexpr float eyeHalfSeparation = 2.24f;
    fnvxr::stereo::EyeCullFrustum cullEyes[2] {};
    for (int eye = 0; eye < 2; ++eye)
    {
        cullEyes[eye].rotation = identity3;
        cullEyes[eye].position = {
            eye == 0 ? -eyeHalfSeparation : eyeHalfSeparation,
            0.0f,
            0.0f
        };
        cullEyes[eye].left = -1.0f;
        cullEyes[eye].right = 1.0f;
        cullEyes[eye].top = 1.0f;
        cullEyes[eye].bottom = -1.0f;
    }
    const auto centerCull = fnvxr::stereo::conservativeCenterCullFrustum(
        identity3,
        {},
        cullEyes,
        5.0f,
        1000.0f);
    const float expectedHorizontalCull = 1.0f + eyeHalfSeparation / 5.0f;
    if (!centerCull.valid
        || centerCull.left > -expectedHorizontalCull
        || centerCull.right < expectedHorizontalCull
        || centerCull.top < 1.0f
        || centerCull.bottom > -1.0f
        || centerCull.nearDistance > 5.0f
        || centerCull.farDistance < 1000.0f
        || std::fabs(centerCull.left + expectedHorizontalCull) > 0.0001f
        || std::fabs(centerCull.right - expectedHorizontalCull) > 0.0001f
        || std::fabs(centerCull.nearDistance - 5.0f) > 0.0001f
        || std::fabs(centerCull.farDistance - 1000.0f) > 0.001f)
    {
        return fail("center traversal cull must include displaced eye origins, not only raw FOV tangents");
    }
    if (fnvxr::stereo::conservativeCenterCullFrustum(
            identity3, {}, nullptr, 5.0f, 1000.0f).valid)
    {
        return fail("null cull-eye input must fail closed");
    }
    const auto unboundedCull = fnvxr::stereo::conservativeCenterCullFrustum(
        identity3,
        {},
        cullEyes,
        5.0f,
        std::numeric_limits<float>::max());
    if (unboundedCull.valid)
        return fail("unbounded cull distance must not round to an infinite published frustum");

    fnvxr::stereo::Matrix3 shearedCenter {{
        { 1.0f, 0.0f, 2.0f },
        { 0.0f, 1.0f, 2.0f },
        { 0.0f, 0.0f, 2.0f }
    }};
    fnvxr::stereo::EyeCullFrustum cancellationEyes[2] {};
    for (auto& eye : cancellationEyes)
    {
        eye.rotation = {{
            { 1.9368603229522705f, -1.9368603229522705f, 0.00011821657244581729f },
            { 1.6135408878326416f, -1.6135408878326416f, 0.000098482720204629f },
            { 1.662406325340271f, -1.662406325340271f, 0.00010146522981813177f }
        }};
        eye.left = 998.9999389648438f;
        eye.right = 999.0f;
        eye.bottom = 998.9999389648438f;
        eye.top = 999.0f;
    }
    if (fnvxr::stereo::conservativeCenterCullFrustum(
            shearedCenter,
            {},
            cancellationEyes,
            838520.125f,
            838520.1875f).valid)
    {
        return fail("non-rotation matrices must not exploit a cancelled forward denominator");
    }

    fnvxr::stereo::EyeCullFrustum grazingEyes[2] {};
    const fnvxr::stereo::Matrix3 grazingYaw {{
        { 0.9070694446563721f, 0.0f, 0.42098110914230347f },
        { 0.0f, 1.0f, 0.0f },
        { -0.42098110914230347f, 0.0f, 0.9070694446563721f }
    }};
    for (int eye = 0; eye < 2; ++eye)
    {
        grazingEyes[eye].rotation = grazingYaw;
        grazingEyes[eye].position = {
            eye == 0 ? -1.2365703582763672f : 1.2365703582763672f,
            0.0f,
            0.0f
        };
        grazingEyes[eye].left = -2.154655933380127f;
        grazingEyes[eye].right = 0.5463024973869324f;
        grazingEyes[eye].bottom = -0.5463024973869324f;
        grazingEyes[eye].top = 0.5463024973869324f;
    }
    if (fnvxr::stereo::conservativeCenterCullFrustum(
            identity3,
            {},
            grazingEyes,
            999999.9375f,
            1000000.0f).valid)
    {
        return fail("near-grazing eye corners must fail the perspective conditioning floor");
    }

    // Independently recompute long-range canted-eye sample points in double.
    // This catches a subtle failure where production once rounded the matrix
    // composition/corner transform in float and then expanded the final bound
    // by only one ULP, which was not actually conservative at 100k units.
    std::uint32_t frustumRandomState = 0x6d0f2026u;
    auto frustumRandom = [&frustumRandomState]() {
        frustumRandomState = frustumRandomState * 1664525u + 1013904223u;
        return static_cast<float>((frustumRandomState >> 8) * (1.0 / 16777216.0));
    };
    for (int trial = 0; trial < 600; ++trial)
    {
        const float nearDistance = 4.0f + 6.0f * frustumRandom();
        const float farDistance = 5000.0f + 95000.0f * frustumRandom();
        const float halfIpd = 1.9f + 0.7f * frustumRandom();
        fnvxr::stereo::EyeCullFrustum randomizedEyes[2] {};
        for (int eye = 0; eye < 2; ++eye)
        {
            const float sign = eye == 0 ? -1.0f : 1.0f;
            const float cant = sign * (0.01f + 0.09f * frustumRandom());
            const float cosine = std::cos(cant);
            const float sine = std::sin(cant);
            randomizedEyes[eye].rotation = {{
                { cosine, 0.0f, sine },
                { 0.0f, 1.0f, 0.0f },
                { -sine, 0.0f, cosine }
            }};
            randomizedEyes[eye].position = {
                sign * halfIpd,
                (frustumRandom() - 0.5f) * 0.1f,
                (frustumRandom() - 0.5f) * 0.1f
            };
            randomizedEyes[eye].left = -1.4f + 0.5f * frustumRandom();
            randomizedEyes[eye].right = 0.9f + 0.5f * frustumRandom();
            randomizedEyes[eye].bottom = -1.3f + 0.4f * frustumRandom();
            randomizedEyes[eye].top = 0.9f + 0.4f * frustumRandom();
        }
        const auto randomizedCull = fnvxr::stereo::conservativeCenterCullFrustum(
            identity3,
            {},
            randomizedEyes,
            nearDistance,
            farDistance);
        if (!randomizedCull.valid)
            return fail("randomized translated/canted long-range cull must remain constructible");

        for (int eye = 0; eye < 2; ++eye)
        {
            const auto& sampleEye = randomizedEyes[eye];
            const double horizontal[3] {
                sampleEye.left,
                (static_cast<double>(sampleEye.left) + sampleEye.right) * 0.5,
                sampleEye.right
            };
            const double vertical[3] {
                sampleEye.bottom,
                (static_cast<double>(sampleEye.bottom) + sampleEye.top) * 0.5,
                sampleEye.top
            };
            const double depthSamples[3] {
                nearDistance,
                std::sqrt(static_cast<double>(nearDistance) * farDistance),
                farDistance
            };
            for (const double depth : depthSamples)
            {
                for (const double tangentX : horizontal)
                {
                    for (const double tangentY : vertical)
                    {
                        const double local[3] { tangentX * depth, tangentY * depth, -depth };
                        double center[3] {
                            sampleEye.position.x,
                            sampleEye.position.y,
                            sampleEye.position.z
                        };
                        for (int row = 0; row < 3; ++row)
                            for (int inner = 0; inner < 3; ++inner)
                                center[row] += static_cast<double>(sampleEye.rotation.m[row][inner]) * local[inner];
                        const double forward = -center[2];
                        const double slopeX = center[0] / forward;
                        const double slopeY = center[1] / forward;
                        if (forward < randomizedCull.nearDistance
                            || forward > randomizedCull.farDistance
                            || slopeX < randomizedCull.left
                            || slopeX > randomizedCull.right
                            || slopeY < randomizedCull.bottom
                            || slopeY > randomizedCull.top)
                        {
                            return fail("outward-rounded float cull must contain independent double long-range samples");
                        }
                    }
                }
            }
        }
    }

    const auto cameraYaw = fnvxr::stereo::composeBodyAndHead(
        levelCamera,
        fnvxr::stereo::cameraLocalHeadRotation(axisAngle(0.0f, 1.0f, 0.0f, Pi * 0.25f)));
    const auto yawedCameraForward = fnvxr::stereo::transform(cameraYaw, cameraLocalForward);
    if (std::fabs(yawedCameraForward.z) > 0.0001f)
        return fail("OpenXR camera-local yaw must stay horizontal instead of becoming pitch");

    const auto cameraPitch = fnvxr::stereo::composeBodyAndHead(
        levelCamera,
        fnvxr::stereo::cameraLocalHeadRotation(axisAngle(1.0f, 0.0f, 0.0f, Pi * 0.25f)));
    const auto pitchedCameraForward = fnvxr::stereo::transform(cameraPitch, cameraLocalForward);
    if (pitchedCameraForward.z < 0.70f)
        return fail("OpenXR camera-local positive pitch must lift the view toward world-up");

    const auto cameraRoll = fnvxr::stereo::composeBodyAndHead(
        levelCamera,
        fnvxr::stereo::cameraLocalHeadRotation(axisAngle(0.0f, 0.0f, 1.0f, Pi * 0.25f)));
    const auto rolledCameraForward = fnvxr::stereo::transform(cameraRoll, cameraLocalForward);
    if (!vectorNear(
            rolledCameraForward,
            levelForward.x,
            levelForward.y,
            levelForward.z))
    {
        return fail("OpenXR camera-local roll must not steer the viewing direction");
    }

    // Head and controller are independent tracked poses in the same recenter
    // frame.  The controller-to-gun lane must not contain the current HMD pose.
    const fnvxr::stereo::Vector3 bodyAnchor { 120.0f, -45.0f, 900.0f };
    const fnvxr::stereo::Vector3 controllerRaw { 4.25f, -2.30f, 6.55f };
    const auto controllerLocalXr = fnvxr::stereo::positionInOriginFrame(
        bodyOrigin,
        originPosition,
        controllerRaw);
    const auto controllerLocalGame = fnvxr::stereo::xrVectorToGamebryo(controllerLocalXr);
    const auto controllerBodyOffset = fnvxr::stereo::transform(bodyYaw, {
        controllerLocalGame.x * fnvxr::stereo::DefaultGameUnitsPerMeter,
        controllerLocalGame.y * fnvxr::stereo::DefaultGameUnitsPerMeter,
        controllerLocalGame.z * fnvxr::stereo::DefaultGameUnitsPerMeter
    });
    const fnvxr::stereo::Vector3 gunWorldA {
        bodyAnchor.x + controllerBodyOffset.x,
        bodyAnchor.y + controllerBodyOffset.y,
        bodyAnchor.z + controllerBodyOffset.z
    };

    // A completely different current HMD pose must not alter gunWorldA.  It
    // is intentionally evaluated only through the camera lane.
    const auto movedHead = fnvxr::stereo::multiply(
        bodyOrigin,
        axisAngle(0.0f, 1.0f, 0.0f, 0.9f));
    const auto movedHeadLocal = fnvxr::stereo::relativeOrientation(bodyOrigin, movedHead);
    const auto movedCamera = fnvxr::stereo::composeBodyAndHead(
        bodyYaw,
        fnvxr::stereo::gamebryoHeadRotation(movedHeadLocal));
    (void)movedCamera;
    const fnvxr::stereo::Vector3 gunWorldAfterHeadOnly {
        bodyAnchor.x + controllerBodyOffset.x,
        bodyAnchor.y + controllerBodyOffset.y,
        bodyAnchor.z + controllerBodyOffset.z
    };
    if (!vectorNear(
            gunWorldAfterHeadOnly,
            gunWorldA.x,
            gunWorldA.y,
            gunWorldA.z))
    {
        return fail("head-only movement must not move the controller-driven gun target");
    }

    const fnvxr::stereo::Vector3 movedControllerRaw {
        controllerRaw.x + 0.10f,
        controllerRaw.y,
        controllerRaw.z
    };
    const auto movedControllerLocalXr = fnvxr::stereo::positionInOriginFrame(
        bodyOrigin,
        originPosition,
        movedControllerRaw);
    const auto movedControllerLocalGame = fnvxr::stereo::xrVectorToGamebryo(movedControllerLocalXr);
    const auto movedControllerBodyOffset = fnvxr::stereo::transform(bodyYaw, {
        movedControllerLocalGame.x * fnvxr::stereo::DefaultGameUnitsPerMeter,
        movedControllerLocalGame.y * fnvxr::stereo::DefaultGameUnitsPerMeter,
        movedControllerLocalGame.z * fnvxr::stereo::DefaultGameUnitsPerMeter
    });
    const fnvxr::stereo::Vector3 gunWorldAfterControllerMove {
        bodyAnchor.x + movedControllerBodyOffset.x,
        bodyAnchor.y + movedControllerBodyOffset.y,
        bodyAnchor.z + movedControllerBodyOffset.z
    };
    const float gunMotion = std::sqrt(
        (gunWorldAfterControllerMove.x - gunWorldA.x) * (gunWorldAfterControllerMove.x - gunWorldA.x)
        + (gunWorldAfterControllerMove.y - gunWorldA.y) * (gunWorldAfterControllerMove.y - gunWorldA.y)
        + (gunWorldAfterControllerMove.z - gunWorldA.z) * (gunWorldAfterControllerMove.z - gunWorldA.z));
    if (!nearlyEqual(gunMotion, 0.10f * fnvxr::stereo::DefaultGameUnitsPerMeter))
        return fail("controller-only movement must move the gun target at the configured meter-to-unit scale");

    const bool eligibleWorldBasis = fnvxr::stereo::programmableWorldDrawBasis(
        true, false, true, true, false, false, false, false, false);
    // The fifth argument means proven screen-space, not merely a stale
    // fixed-function projection. Production passes false until strong shader
    // semantics exist, so programmable draws cannot escape the denominator.
    if (!fnvxr::stereo::programmableWorldDrawCandidate(eligibleWorldBasis, true))
    {
        return fail("eligible indexed world geometry on the eye target must enter shader coverage");
    }
    if (fnvxr::stereo::programmableWorldDrawCandidate(eligibleWorldBasis, false))
    {
        return fail("auxiliary render-target geometry must stay outside eye-image coverage");
    }
    if (!fnvxr::stereo::strictEyeTargetDrawLedgerComplete(9, 9)
        || fnvxr::stereo::strictEyeTargetDrawLedgerComplete(9, 8)
        || fnvxr::stereo::strictEyeTargetDrawLedgerComplete(0, 0))
    {
        return fail("every strict eye-target draw must have a proven successful write to both eyes");
    }
    if (!fnvxr::stereo::strictEyeTargetOptionalWriteLedgerComplete(0, 0)
        || !fnvxr::stereo::strictEyeTargetOptionalWriteLedgerComplete(3, 3)
        || fnvxr::stereo::strictEyeTargetOptionalWriteLedgerComplete(3, 2))
    {
        return fail("every observed strict eye-target clear/copy must succeed on both eyes");
    }
    if (!fnvxr::stereo::programmableWorldDrawBasis(
            true, false, true, true, false, false, false, false, false))
    {
        return fail("eligible indexed world geometry must enter shader coverage");
    }
    if (fnvxr::stereo::programmableWorldDrawBasis(
            true, false, true, true, false, false, false, true, false))
    {
        return fail("unsupported immediate UI primitives must not poison world coverage");
    }
    if (!fnvxr::stereo::programmableWorldDrawBasis(
            true, false, true, true, false, false, false, true, true))
    {
        return fail("immediate primitives must enter coverage when replay is explicitly enabled");
    }
    if (fnvxr::stereo::programmableWorldDrawBasis(
            true, false, true, true, true, false, false, false, false)
        || fnvxr::stereo::programmableWorldDrawBasis(
            true, false, true, true, false, true, false, false, false)
        || fnvxr::stereo::programmableWorldDrawBasis(
            true, false, true, true, false, false, true, false, false))
    {
        return fail("screen/composite/configured-skip draws must stay outside world coverage");
    }

    if (!fnvxr::stereo::singleTraversalPublishAllowed(false, false)
        || !fnvxr::stereo::singleTraversalPublishAllowed(true, true)
        || fnvxr::stereo::singleTraversalPublishAllowed(true, false))
    {
        return fail("single-traversal mode must never downgrade an incomplete pair to generic replay");
    }

    constexpr std::uint64_t producerEpoch = 0x1122334455667788ull;
    constexpr std::uint64_t rendererProducerEpoch = 0x8877665544332211ull;
    constexpr std::uint32_t referenceGeneration = 7u;
    constexpr std::uint32_t producerProcessId = 4242u;
    if (fnvxr::stereo::nextStereoStableFrameCount(
            0, 0, 1, 0, producerEpoch,
            0, rendererProducerEpoch,
            0, referenceGeneration, 0, producerProcessId) != 1
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, 10, 11, producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId) != 6
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, 10, 3, producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId) != 1
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, 10, 11, producerEpoch, producerEpoch + 1,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId) != 1
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, 10, 11, producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch + 1,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId) != 1
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, 10, 11, producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration + 1,
            producerProcessId, producerProcessId) != 1
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, 10, 11, producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId + 1) != 1
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, -2, 1, producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId) != 6
        || fnvxr::stereo::nextStereoStableFrameCount(
            5, 10, 0, producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId) != 0
        || fnvxr::stereo::nextStereoStableFrameCount(
            (std::numeric_limits<std::uint64_t>::max)(), 10, 11,
            producerEpoch, producerEpoch,
            rendererProducerEpoch, rendererProducerEpoch,
            referenceGeneration, referenceGeneration,
            producerProcessId, producerProcessId)
            != (std::numeric_limits<std::uint64_t>::max)())
    {
        return fail("stereo warm-up must reset on sequence or producer identity discontinuities");
    }

    constexpr std::int64_t displayTime = 1000000000LL;
    constexpr std::int64_t maximumAge = 25000000LL;
    constexpr std::int64_t futureTolerance = 5000000LL;
    std::int64_t sourceAge = 0;
    if (!fnvxr::stereo::sourcePoseAgeWithinBudget(
            displayTime, displayTime - 20000000LL, maximumAge, futureTolerance, &sourceAge)
        || sourceAge != 20000000LL)
    {
        return fail("a 20 ms source pose must pass the 25 ms projection-layer age budget");
    }
    if (fnvxr::stereo::sourcePoseAgeWithinBudget(
            displayTime, displayTime - 100000000LL, maximumAge, futureTolerance))
    {
        return fail("a 100 ms source pose must fail the projection-layer age budget");
    }
    if (fnvxr::stereo::sourcePoseAgeWithinBudget(
            displayTime, displayTime + 10000000LL, maximumAge, futureTolerance))
    {
        return fail("a source pose 10 ms in the future must exceed the 5 ms tolerance");
    }
    if (!fnvxr::stereo::sourcePoseAgeWithinBudget(
            displayTime, displayTime - maximumAge, maximumAge, futureTolerance)
        || fnvxr::stereo::sourcePoseAgeWithinBudget(
            displayTime, displayTime - maximumAge - 1, maximumAge, futureTolerance)
        || !fnvxr::stereo::sourcePoseAgeWithinBudget(
            displayTime, displayTime + futureTolerance, maximumAge, futureTolerance)
        || fnvxr::stereo::sourcePoseAgeWithinBudget(
            displayTime, displayTime + futureTolerance + 1, maximumAge, futureTolerance))
    {
        return fail("source-pose age and future-tolerance boundaries must be inclusive and fail one tick outside");
    }
    if (fnvxr::stereo::sourcePoseAgeWithinBudget(0, 1, maximumAge, futureTolerance)
        || fnvxr::stereo::sourcePoseAgeWithinBudget(1, 0, maximumAge, futureTolerance)
        || fnvxr::stereo::sourcePoseAgeWithinBudget(1, 1, -1, futureTolerance)
        || fnvxr::stereo::sourcePoseAgeWithinBudget(1, 1, maximumAge, -1)
        || fnvxr::stereo::sourcePoseAgeWithinBudget(
            std::numeric_limits<std::int64_t>::max(), 1, maximumAge, futureTolerance)
        || fnvxr::stereo::sourcePoseAgeWithinBudget(
            1, std::numeric_limits<std::int64_t>::max(), maximumAge, futureTolerance))
    {
        return fail("invalid and extreme source-pose timestamps must fail without overflow");
    }

    std::cout << "stereo math ok\n";
    return 0;
}
