#include "fnvxr_retail_tracked_frame.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

fnvxr::engine::RetailTrackedFrame validFrame()
{
    using namespace fnvxr;
    engine::RetailTrackedFrame frame {};
    frame.pose.magic = shared::VrPoseSharedMagic;
    frame.pose.version = shared::VrPoseSharedVersion;
    frame.pose.frame = 10u;
    frame.pose.predictedDisplayTime = 99;
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
}

int main()
{
    using namespace fnvxr::engine;
    RetailTrackedFrame frame = validFrame();
    require(
        validateRetailTrackedGameplayFrame(frame).complete(),
        "complete tracked gameplay frame rejected");

    frame.poseSequence = 3;
    require(
        validateRetailTrackedGameplayFrame(frame).failure
            == RetailTrackedFrameFailure::PosePublicationInvalid,
        "odd pose publication accepted");
    frame = validFrame();
    frame.pose.leftEyePos[0] =
        (std::numeric_limits<float>::quiet_NaN)();
    require(
        validateRetailTrackedGameplayFrame(frame).failure
            == RetailTrackedFrameFailure::PoseNumbersInvalid,
        "non-finite eye pose accepted");
    frame = validFrame();
    frame.pose.rightFov[0] = frame.pose.rightFov[1];
    require(
        validateRetailTrackedGameplayFrame(frame).failure
            == RetailTrackedFrameFailure::EyeFovInvalid,
        "collapsed eye FOV accepted");
    frame = validFrame();
    frame.runtime.menuBits = fnvxr::shared::RuntimeDialogMenuBit;
    require(
        validateRetailTrackedGameplayFrame(frame).failure
            == RetailTrackedFrameFailure::RuntimeNotGameplay,
        "dialogue runtime admitted as binocular gameplay");
    require(
        validateRetailTrackedUiFrame(frame).complete(),
        "dialogue runtime rejected as confirmed UI");
    frame = validFrame();
    require(
        validateRetailTrackedUiFrame(frame).failure
            == RetailTrackedFrameFailure::RuntimeNotUi,
        "gameplay runtime admitted as UI capture");
    frame = validFrame();
    frame.runtime.phase = fnvxr::shared::RuntimePhaseLoading;
    require(
        validateRetailTrackedUiFrame(frame).complete(),
        "confirmed loading UI rejected");
    frame = validFrame();
    frame.runtime.menuBits = fnvxr::shared::RuntimePipBoyMenuBit;
    require(
        validateRetailTrackedUiFrame(frame).complete(),
        "confirmed Pip-Boy rejected as UI quad");

    std::cout << "retail tracked frame validation passed\n";
    return EXIT_SUCCESS;
}
