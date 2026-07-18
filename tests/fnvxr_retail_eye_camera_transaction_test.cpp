#include "fnvxr_retail_eye_camera_transaction.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>

namespace
{
using namespace fnvxr;
using namespace fnvxr::engine;

[[noreturn]] void fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const char* message)
{
    if (!condition)
        fail(message);
}

bool nearlyEqual(float actual, float expected, float tolerance = 0.0001f)
{
    return std::fabs(actual - expected) <= tolerance;
}

void writePointer32(
    abi::RetailNiCameraLayout& camera,
    std::size_t offset,
    abi::RetailPointer32 value)
{
    std::memcpy(
        reinterpret_cast<std::uint8_t*>(&camera) + offset,
        &value,
        sizeof(value));
}

RetailNiTransformLayout identityTransform(
    float x = 0.0f,
    float y = 0.0f,
    float z = 0.0f)
{
    RetailNiTransformLayout result {};
    result.rotation[0] = 1.0f;
    result.rotation[4] = 1.0f;
    result.rotation[8] = 1.0f;
    result.translation[0] = x;
    result.translation[1] = y;
    result.translation[2] = z;
    result.scale = 1.0f;
    return result;
}

void initializeCamera(
    abi::RetailNiCameraLayout& camera,
    float x = 0.0f,
    float y = 0.0f,
    float z = 0.0f)
{
    camera = {};
    writePointer32(camera, 0u, 0x0109CB9Cu);
    const RetailNiTransformLayout transform = identityTransform(x, y, z);
    detail::retailCameraWriteTransform(
        &camera,
        RetailNiAvObjectLocalTransformOffset,
        transform);
    detail::retailCameraWriteTransform(
        &camera,
        RetailNiAvObjectWorldTransformOffset,
        transform);
    camera.frustum.left = -1.0f;
    camera.frustum.right = 1.0f;
    camera.frustum.top = 1.0f;
    camera.frustum.bottom = -1.0f;
    camera.frustum.nearDistance = 5.0f;
    camera.frustum.farDistance = 1000.0f;
    camera.minimumNearPlane = 1.0f;
    camera.maximumFarNearRatio = 10000.0f;
    camera.viewport.left = 0.0f;
    camera.viewport.right = 1.0f;
    camera.viewport.top = 1.0f;
    camera.viewport.bottom = 0.0f;
    camera.lodAdjust = 1.0f;
}

RetailTrackedFrame validFrame()
{
    RetailTrackedFrame frame {};
    frame.pose.magic = shared::VrPoseSharedMagic;
    frame.pose.version = shared::VrPoseSharedVersion;
    frame.pose.frame = 10u;
    frame.pose.predictedDisplayTime = 100;
    frame.pose.hmdRot[3] = 1.0f;
    frame.pose.leftEyeRot[3] = 1.0f;
    frame.pose.rightEyeRot[3] = 1.0f;
    frame.pose.leftEyePos[0] = -0.032f;
    frame.pose.rightEyePos[0] = 0.032f;
    const float fov[4] { -0.8f, 0.8f, 0.75f, -0.75f };
    for (std::size_t index = 0u; index < 4u; ++index)
    {
        frame.pose.leftFov[index] = fov[index];
        frame.pose.rightFov[index] = fov[index];
    }
    frame.pose.trackingFlags = shared::VrPoseTrackingHmd;
    frame.pose.referenceSpaceGeneration = 2u;
    frame.pose.producerEpoch = 3u;
    frame.runtime.magic = shared::RuntimeSharedMagic;
    frame.runtime.version = shared::RuntimeSharedVersion;
    frame.runtime.frame = 10u;
    frame.runtime.phase = shared::RuntimePhaseGameplay;
    frame.runtime.cameraActive = 1u;
    frame.poseSequence = 2;
    frame.runtimeSequence = 2;
    return frame;
}

#if defined(_MSC_VER) && defined(_M_IX86)
#define FNVXR_TEST_CDECL __cdecl
#define FNVXR_TEST_THISCALL_IMPL __fastcall
#define FNVXR_TEST_TRAILING_EDX , void*
#else
#define FNVXR_TEST_CDECL
#define FNVXR_TEST_THISCALL_IMPL
#define FNVXR_TEST_TRAILING_EDX
#endif

int gFactoryCalls = 0;
int gFreeCalls = 0;
int gNullFactoryCall = 0;

abi::RetailNiCameraLayout* FNVXR_TEST_CDECL fakeCameraCreate()
{
    ++gFactoryCalls;
    if (gNullFactoryCall == gFactoryCalls)
        return nullptr;
    auto* camera = new (std::nothrow) abi::RetailNiCameraLayout {};
    if (camera)
        initializeCamera(*camera);
    return camera;
}

void FNVXR_TEST_THISCALL_IMPL fakeCameraFree(
    abi::RetailNiAccumulatorLayout* instance
    FNVXR_TEST_TRAILING_EDX)
{
    ++gFreeCalls;
    delete reinterpret_cast<abi::RetailNiCameraLayout*>(instance);
}

template <typename Target, typename Source>
Target engineFunction(Source source) noexcept
{
    return reinterpret_cast<Target>(source);
}

void testWorldToCameraContract()
{
    RetailNiTransformLayout world {};
    // Columns: right=(0,1,0), up=(0,0,1), back=(1,0,0).
    world.rotation[1] = 0.0f;
    world.rotation[2] = 1.0f;
    world.rotation[3] = 1.0f;
    world.rotation[7] = 1.0f;
    world.translation[0] = 10.0f;
    world.translation[1] = 20.0f;
    world.translation[2] = 30.0f;
    world.scale = 1.0f;
    abi::RetailNiFrustumLayout frustum {};
    frustum.left = -2.0f;
    frustum.right = 1.0f;
    frustum.top = 1.5f;
    frustum.bottom = -0.5f;
    frustum.nearDistance = 5.0f;
    frustum.farDistance = 100.0f;
    float matrix[16] {};
    require(
        detail::buildRetailWorldToCamera(world, frustum, matrix),
        "known retail right/up/back camera did not produce a matrix");
    require(
        nearlyEqual(matrix[0], -1.0f / 3.0f)
            && nearlyEqual(matrix[1], 2.0f / 3.0f)
            && nearlyEqual(matrix[2], 0.0f)
            && nearlyEqual(matrix[3], -10.0f)
            && nearlyEqual(matrix[4], 0.5f)
            && nearlyEqual(matrix[5], 0.0f)
            && nearlyEqual(matrix[6], 1.0f)
            && nearlyEqual(matrix[7], -35.0f)
            && nearlyEqual(matrix[8], -100.0f / 95.0f)
            && nearlyEqual(matrix[11], 5.0f / 95.0f * 100.0f)
            && nearlyEqual(matrix[12], -1.0f)
            && nearlyEqual(matrix[15], 10.0f),
        "world-to-clip signs or off-center projection terms changed");
}

void testCapturedRetailWorldToCamera()
{
    // One loaded retail transaction logged these camera/frustum/matrix values
    // together. The rotation columns below are the normalized right/up/back
    // axes present in that same matrix; the position and frustum are the
    // independently logged NiCamera fields.
    RetailNiTransformLayout world {};
    world.rotation[0] = 0.29196088f;
    world.rotation[1] = 0.0f;
    world.rotation[2] = -0.95643002f;
    world.rotation[3] = -0.95642990f;
    world.rotation[4] = 0.0f;
    world.rotation[5] = -0.29196101f;
    world.rotation[6] = 0.0f;
    world.rotation[7] = 1.0f;
    world.rotation[8] = 0.0f;
    world.translation[0] = -72393.6484f;
    world.translation[1] = -1240.1996f;
    world.translation[2] = 8258.8574f;
    world.scale = 1.0f;
    abi::RetailNiFrustumLayout frustum {};
    frustum.left = -1.37638187f;
    frustum.right = 1.37638187f;
    frustum.top = 0.965688765f;
    frustum.bottom = -1.42814791f;
    frustum.nearDistance = 5.0f;
    frustum.farDistance = 353840.0f;
    float actual[16] {};
    require(
        detail::buildRetailWorldToCamera(world, frustum, actual),
        "captured retail camera did not pass the matrix contract");
    const float captured[16] {
        0.212122f, -0.694887f, 0.0f, 14494.5f,
        0.184770f, 0.0564031f, 0.835479f, 6546.05f,
        0.956444f, 0.291965f, 0.0f, 69597.6f,
        0.956430f, 0.291961f, 0.0f, 69601.6f,
    };
    for (std::size_t index = 0u; index < 16u; ++index)
    {
        const float tolerance = index % 4u == 3u ? 0.1f : 0.00005f;
        require(
            nearlyEqual(actual[index], captured[index], tolerance),
            "direct camera matrix diverged from loaded retail capture");
    }
}

void testOriginAndRig()
{
    RetailTrackedFrame frame = validFrame();
    const RetailVrOriginCandidate first = prepareRetailVrOriginCandidate(
        {},
        frame);
    require(
        first.complete() && first.relatched
            && nearlyEqual(first.origin.position.x, 0.0f)
            && nearlyEqual(first.origin.orientation.w, 1.0f),
        "first gameplay pose did not latch one rigid eye-midpoint/yaw origin");

    abi::RetailNiCameraLayout stock {};
    initializeCamera(stock, 100.0f, 200.0f, 300.0f);
    const abi::RetailNiCameraLayout stockBefore = stock;
    const RetailDerivedEyeCameraRig rig = deriveRetailEyeCameraRig(
        &stock,
        frame,
        first.origin,
        70.0f);
    require(rig.complete(), "valid tracked frame did not derive three cameras");
    require(
        nearlyEqual(rig.center.world.translation[0], 100.0f)
            && nearlyEqual(rig.left.world.translation[0], 97.76f, 0.001f)
            && nearlyEqual(rig.right.world.translation[0], 102.24f, 0.001f),
        "exact OpenXR eye positions did not become signed private-camera IPD");
    require(
        rig.center.frustum.left < rig.left.frustum.left
            && rig.center.frustum.right > rig.right.frustum.right,
        "center cull did not conservatively include translated eye frusta");
    require(
        std::memcmp(&stock, &stockBefore, sizeof(stock)) == 0,
        "pure rig derivation mutated the stock camera");

    abi::RetailNiCameraLayout center {};
    abi::RetailNiCameraLayout left {};
    abi::RetailNiCameraLayout right {};
    initializeCamera(center, 1.0f, 2.0f, 3.0f);
    initializeCamera(left, 4.0f, 5.0f, 6.0f);
    initializeCamera(right, 7.0f, 8.0f, 9.0f);
    const abi::RetailNiCameraLayout centerBefore = center;
    const abi::RetailNiCameraLayout leftBefore = left;
    const abi::RetailNiCameraLayout rightBefore = right;
    {
        RetailScopedEyeCameraTransaction transaction;
        require(
            transaction.begin(
                &stock,
                { &center, &left, &right },
                rig),
            "three detached private cameras rejected a valid transaction");
        require(
            nearlyEqual(detail::retailCameraReadTransform(
                    &left,
                    RetailNiAvObjectWorldTransformOffset).translation[0],
                97.76f,
                0.001f)
                && nearlyEqual(detail::retailCameraReadTransform(
                    &right,
                    RetailNiAvObjectWorldTransformOffset).translation[0],
                102.24f,
                0.001f),
            "scoped transaction did not install distinct eye cameras");
        require(
            std::memcmp(&stock, &stockBefore, sizeof(stock)) == 0,
            "private camera transaction touched the stock camera");
    }
    require(
        std::memcmp(&center, &centerBefore, sizeof(center)) == 0
            && std::memcmp(&left, &leftBefore, sizeof(left)) == 0
            && std::memcmp(&right, &rightBefore, sizeof(right)) == 0,
        "scoped transaction did not roll back every private camera field");

    frame.pose.frame += 1u;
    frame.runtime.frame += 1u;
    frame.poseSequence += 2;
    frame.runtimeSequence += 2;
    frame.pose.hmdPos[2] = -0.10f;
    frame.pose.leftEyePos[2] = -0.10f;
    frame.pose.rightEyePos[2] = -0.10f;
    const RetailVrOriginCandidate moved = prepareRetailVrOriginCandidate(
        first.origin,
        frame);
    require(
        moved.complete() && !moved.relatched
            && nearlyEqual(moved.origin.position.z, 0.0f),
        "ordinary head motion incorrectly moved the retained origin");
    const RetailDerivedEyeCameraRig movedRig = deriveRetailEyeCameraRig(
        &stock,
        frame,
        moved.origin,
        70.0f);
    require(
        movedRig.complete()
            && nearlyEqual(movedRig.center.world.translation[1], 207.0f, 0.001f),
        "OpenXR forward motion leaked away from NiCamera forward");

    frame.pose.recenterRequestId = 1u;
    frame.pose.frame += 1u;
    frame.runtime.frame += 1u;
    frame.poseSequence += 2;
    frame.runtimeSequence += 2;
    const RetailVrOriginCandidate recentered = prepareRetailVrOriginCandidate(
        moved.origin,
        frame);
    const RetailDerivedEyeCameraRig recenteredRig = deriveRetailEyeCameraRig(
        &stock,
        frame,
        recentered.origin,
        70.0f);
    require(
        recentered.complete() && recentered.relatched
            && recenteredRig.complete()
            && nearlyEqual(recenteredRig.center.world.translation[0], 100.0f)
            && nearlyEqual(recenteredRig.center.world.translation[1], 200.0f)
            && nearlyEqual(recenteredRig.center.world.translation[2], 300.0f),
        "recenter did not relatch position and yaw as one rigid frame");

    RetailTrackedFrame badBaseline = validFrame();
    badBaseline.pose.rightEyePos[0] = -0.064f;
    require(
        deriveRetailEyeCameraRig(
            &stock,
            badBaseline,
            first.origin,
            70.0f).failure == RetailEyeCameraFailure::EyeBaselineRejected,
        "reversed OpenXR eye baseline was admitted");
    require(
        deriveRetailEyeCameraRig(
            &stock,
            validFrame(),
            first.origin,
            0.0f).failure == RetailEyeCameraFailure::ScaleRejected,
        "unconfigured meter-to-game scale was admitted");

    writePointer32(left, RetailNiAvObjectParentOffset, 0x12345678u);
    RetailScopedEyeCameraTransaction parented;
    require(
        !parented.begin(&stock, { &center, &left, &right }, rig)
            && parented.failure()
                == RetailEyeCameraFailure::PrivateCameraRejected,
        "parented private camera was admitted as local-equals-world");
}

void testPrivateCameraOwnership()
{
    abi::RetailNiCameraLayout center {};
    initializeCamera(center);
    RetailEngineCalls calls {};
    calls.niCameraCreate = &fakeCameraCreate;
    calls.niRefObjectFree = engineFunction<abi::NiRefObjectFreeFunction>(
        &fakeCameraFree);

    gFactoryCalls = 0;
    gFreeCalls = 0;
    gNullFactoryCall = 0;
    {
        RetailPrivateEyeCameraPair pair;
        require(
            pair.initialize(calls, &center)
                && pair.valid()
                && pair.left() != pair.right(),
            "verified NiCamera factory did not create two owned eye cameras");
    }
    require(
        gFactoryCalls == 2 && gFreeCalls == 2,
        "owned eye cameras were not released exactly once");

    gFactoryCalls = 0;
    gFreeCalls = 0;
    gNullFactoryCall = 2;
    {
        RetailPrivateEyeCameraPair pair;
        require(
            !pair.initialize(calls, &center)
                && pair.failure()
                    == RetailEyeCameraFailure::RightFactoryReturnedNull,
            "second factory failure was not exact");
    }
    require(
        gFactoryCalls == 2 && gFreeCalls == 1,
        "partial eye-camera acquisition did not roll back the first owner");
}
}

int main()
{
    static_assert(sizeof(RetailNiTransformLayout) == 0x34u);
    static_assert(sizeof(RetailNiCameraSpatialEvidenceLayout) == 0x114u);
    testWorldToCameraContract();
    testCapturedRetailWorldToCamera();
    testOriginAndRig();
    testPrivateCameraOwnership();
    std::cout << "retail distinct-eye camera transaction passed\n";
    return EXIT_SUCCESS;
}

#undef FNVXR_TEST_CDECL
#undef FNVXR_TEST_THISCALL_IMPL
#undef FNVXR_TEST_TRAILING_EDX
