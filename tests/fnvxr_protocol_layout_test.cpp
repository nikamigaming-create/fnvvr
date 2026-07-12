#include "fnvxr_protocol.h"
#include "fnvxr_shared_state.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << "\n";
    return 1;
}
}

int main()
{
    if (sizeof(fnvxr::PoseFrame) != 164)
        return fail("PoseFrame size mismatch");

    if (sizeof(fnvxr::GameFrame) != 96)
        return fail("GameFrame size mismatch");

    if (sizeof(fnvxr::shared::SharedPlayerState) != 160)
        return fail("SharedPlayerState size mismatch");

    if (sizeof(fnvxr::shared::SharedCommandState) != 216)
        return fail("SharedCommandState size mismatch");

    if (sizeof(fnvxr::shared::SharedInputEvent) != 32)
        return fail("SharedInputEvent size mismatch");

    if (sizeof(fnvxr::shared::SharedInputEventQueue) != 2088)
        return fail("SharedInputEventQueue size mismatch");

    if (sizeof(fnvxr::shared::SharedVrPoseState) != 208)
        return fail("SharedVrPoseState size mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, predictedDisplayTime) != 24)
        return fail("SharedVrPoseState predictedDisplayTime offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, hmdRot) != 32)
        return fail("SharedVrPoseState hmdRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, trackingFlags) != 204)
        return fail("SharedVrPoseState trackingFlags offset mismatch");

    if (fnvxr::shared::VrPoseSharedVersion != 4)
        return fail("SharedVrPoseState tracking contract version mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, sequence) != 8)
        return fail("SharedPlayerState sequence offset mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, frame) != 16)
        return fail("SharedPlayerState frame offset mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, currentCellFormId) != 24)
        return fail("SharedPlayerState currentCellFormId offset mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, playerWorldRot) != 40)
        return fail("SharedPlayerState playerWorldRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, playerWorldPos) != 76)
        return fail("SharedPlayerState playerWorldPos offset mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, cameraWorldRot) != 88)
        return fail("SharedPlayerState cameraWorldRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, cameraWorldPos) != 124)
        return fail("SharedPlayerState cameraWorldPos offset mismatch");

    if (offsetof(fnvxr::shared::SharedPlayerState, reserved) != 136)
        return fail("SharedPlayerState reserved offset mismatch");

    if (fnvxr::shared::PlayerSharedWeaponClassReservedIndex != 0
        || fnvxr::shared::PlayerSharedEquippedWeaponFormIdReservedIndex != 1
        || fnvxr::shared::PlayerSharedEquippedFavoriteSlotReservedIndex != 2)
    {
        return fail("SharedPlayerState weapon reserved index mismatch");
    }

    if (offsetof(fnvxr::shared::SharedCommandState, sequence) != 8)
        return fail("SharedCommandState sequence offset mismatch");

    if (offsetof(fnvxr::shared::SharedCommandState, requestId) != 12)
        return fail("SharedCommandState requestId offset mismatch");

    if (offsetof(fnvxr::shared::SharedCommandState, requestedFrame) != 32)
        return fail("SharedCommandState requestedFrame offset mismatch");

    if (offsetof(fnvxr::shared::SharedCommandState, saveName) != 52)
        return fail("SharedCommandState saveName offset mismatch");

    if (offsetof(fnvxr::shared::SharedCommandState, lastCommand) != 116)
        return fail("SharedCommandState lastCommand offset mismatch");

    if (offsetof(fnvxr::shared::SharedInputEventQueue, writeSequence) != 12)
        return fail("SharedInputEventQueue writeSequence offset mismatch");

    if (offsetof(fnvxr::shared::SharedInputEventQueue, events) != 40)
        return fail("SharedInputEventQueue events offset mismatch");

    if (sizeof(fnvxr::shared::SharedD3D9FrameHeader) != 28)
        return fail("SharedD3D9FrameHeader size mismatch");

    if (sizeof(fnvxr::shared::SharedD3D9StereoFrameHeader) != 152)
        return fail("SharedD3D9StereoFrameHeader size mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, poseValid) != 40)
        return fail("SharedD3D9StereoFrameHeader poseValid offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, renderedDisplayTime) != 48)
        return fail("SharedD3D9StereoFrameHeader renderedDisplayTime offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, leftEyeRot) != 56)
        return fail("SharedD3D9StereoFrameHeader leftEyeRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, producerMode) != 144)
        return fail("SharedD3D9StereoFrameHeader producerMode offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, renderPairSequence) != 148)
        return fail("SharedD3D9StereoFrameHeader renderPairSequence offset mismatch");

    fnvxr::PoseFrame pose {};
    if (!fnvxr::isValidPoseFrame(pose))
        return fail("default PoseFrame should be valid");

    pose.version = 999;
    if (fnvxr::isValidPoseFrame(pose))
        return fail("PoseFrame version validation failed");

    pose = {};
    pose.menuPointerX = NAN;
    if (fnvxr::isValidPoseFrame(pose))
        return fail("PoseFrame should reject NaN menuPointerX");

    pose = {};
    pose.leftGrip = INFINITY;
    if (fnvxr::isValidPoseFrame(pose))
        return fail("PoseFrame should reject infinite trigger/grip values");

    fnvxr::GameFrame game {};
    if (!fnvxr::isValidGameFrame(game))
        return fail("default GameFrame should be valid");

    game.byteSize = 0;
    if (fnvxr::isValidGameFrame(game))
        return fail("GameFrame byteSize validation failed");

    game = {};
    game.playerWorldPos.z = NAN;
    if (fnvxr::isValidGameFrame(game))
        return fail("GameFrame should reject NaN world position");

    game = {};
    game.playerBodyRot.w = INFINITY;
    if (fnvxr::isValidGameFrame(game))
        return fail("GameFrame should reject infinite body rotation");

    std::cout << "protocol layout ok\n";
    return 0;
}
