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
            if (std::fabs(lhs.m[row][column] - rhs.m[row][column]) >= epsilon)
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
    const auto columnPatched = fnvxr::stereo::applyViewProjectionDelta(
        columnOriginal, inverseCenterVp, eyeVp, true);
    if (!matrixNear(columnPatched, columnExpected))
        return fail("column-vector shader MVP delta must replace the exact center NiCamera world-to-clip factor");

    const auto rowOriginal = fnvxr::stereo::multiply(model, centerVp);
    const auto rowExpected = fnvxr::stereo::multiply(model, eyeVp);
    const auto rowPatched = fnvxr::stereo::applyViewProjectionDelta(
        rowOriginal, inverseCenterVp, eyeVp, false);
    if (!matrixNear(rowPatched, rowExpected))
        return fail("row-vector shader MVP delta must replace the exact center NiCamera world-to-clip factor");

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
    if (!fnvxr::stereo::programmableWorldDrawCandidate(eligibleWorldBasis, true))
    {
        return fail("eligible indexed world geometry on the eye target must enter shader coverage");
    }
    if (fnvxr::stereo::programmableWorldDrawCandidate(eligibleWorldBasis, false))
    {
        return fail("auxiliary render-target geometry must stay outside eye-image coverage");
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

    std::cout << "stereo math ok\n";
    return 0;
}
