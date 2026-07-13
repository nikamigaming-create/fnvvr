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
    if (fnvxr::shared::runtimeLoadingMenuBlocksInput(true, true)
        || !fnvxr::shared::runtimeLoadingMenuBlocksInput(true, false)
        || fnvxr::shared::runtimeLoadingMenuBlocksInput(false, false))
    {
        return fail("actionable retail menus must outrank a stale LoadingMenu lifecycle record");
    }
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

    if (sizeof(fnvxr::shared::SharedXInputState) != 32
        || fnvxr::shared::XInputSharedVersion != 2
        || offsetof(fnvxr::shared::SharedXInputState, sequence) != 8
        || offsetof(fnvxr::shared::SharedXInputState, packet) != 12
        || offsetof(fnvxr::shared::SharedXInputState, buttons) != 16
        || offsetof(fnvxr::shared::SharedXInputState, leftTrigger) != 18
        || offsetof(fnvxr::shared::SharedXInputState, leftThumbX) != 20
        || offsetof(fnvxr::shared::SharedXInputState, rightThumbX) != 24
        || offsetof(fnvxr::shared::SharedXInputState, connected) != 28
        || offsetof(fnvxr::shared::SharedXInputState, reserved) != 29)
    {
        return fail("SharedXInputState sequenced layout mismatch");
    }

    if (sizeof(fnvxr::shared::SharedDInputState) != 100
        || fnvxr::shared::DInputSharedVersion != 9
        || offsetof(fnvxr::shared::SharedDInputState, sequence) != 8
        || offsetof(fnvxr::shared::SharedDInputState, frame) != 12
        || offsetof(fnvxr::shared::SharedDInputState, mouseClickPacket) != 28
        || offsetof(fnvxr::shared::SharedDInputState, keyboardAcceptPacket) != 32
        || offsetof(fnvxr::shared::SharedDInputState, gameplayFlags) != 92
        || offsetof(fnvxr::shared::SharedDInputState, aimTrigger) != 96)
    {
        return fail("SharedDInputState sequenced layout mismatch");
    }

    const std::int32_t wrappedStart = INT32_MAX - 3;
    const std::int32_t wrappedEnd = fnvxr::shared::addWrappedInt32(wrappedStart, 9);
    if (fnvxr::shared::wrappedInt32Delta(wrappedEnd, wrappedStart) != 9)
        return fail("wrapped cumulative DInput delta mismatch");

    fnvxr::shared::SharedXInputState xinputState {};
    xinputState.magic = fnvxr::shared::XInputSharedMagic;
    xinputState.version = fnvxr::shared::XInputSharedVersion;
    if (!fnvxr::shared::beginSequencedSharedWrite(xinputState.sequence))
        return fail("SharedXInputState sequence write did not begin");
    if (fnvxr::shared::beginSequencedSharedWrite(xinputState.sequence))
        return fail("SharedXInputState admitted a second simultaneous writer");
    xinputState.packet = 17;
    xinputState.buttons = 0x1000;
    fnvxr::shared::SharedXInputState xinputSnapshot {};
    if (fnvxr::shared::readSequencedSharedSnapshot(&xinputState, xinputSnapshot, 1))
        return fail("SharedXInputState reader accepted an in-progress frame");
    fnvxr::shared::endSequencedSharedWrite(xinputState.sequence);
    if (!fnvxr::shared::readSequencedSharedSnapshot(&xinputState, xinputSnapshot)
        || xinputSnapshot.packet != 17
        || xinputSnapshot.buttons != 0x1000)
    {
        return fail("SharedXInputState stable snapshot mismatch");
    }

    fnvxr::shared::SharedDInputState dinputState {};
    dinputState.magic = fnvxr::shared::DInputSharedMagic;
    dinputState.version = fnvxr::shared::DInputSharedVersion;
    dinputState.keyboardAcceptPacket = 23;
    if (!fnvxr::shared::beginSequencedSharedWrite(dinputState.sequence))
        return fail("SharedDInputState sequence write did not begin");
    dinputState.frame = 7;
    dinputState.gameplayControlsActive = 1;
    fnvxr::shared::endSequencedSharedWrite(dinputState.sequence);
    fnvxr::shared::SharedDInputState dinputSnapshot {};
    if (!fnvxr::shared::readSequencedSharedSnapshot(&dinputState, dinputSnapshot)
        || dinputSnapshot.keyboardAcceptPacket != 23)
    {
        return fail("SharedDInputState host publish did not preserve plugin accept packet");
    }

    if (sizeof(fnvxr::shared::SharedVrPoseState) != 272)
        return fail("SharedVrPoseState size mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, predictedDisplayTime) != 24)
        return fail("SharedVrPoseState predictedDisplayTime offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, hmdRot) != 32)
        return fail("SharedVrPoseState hmdRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, leftAimRot) != 204)
        return fail("SharedVrPoseState leftAimRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, leftAimPos) != 220)
        return fail("SharedVrPoseState leftAimPos offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, rightAimRot) != 232)
        return fail("SharedVrPoseState rightAimRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, rightAimPos) != 248)
        return fail("SharedVrPoseState rightAimPos offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, trackingFlags) != 260)
        return fail("SharedVrPoseState trackingFlags offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, referenceSpaceGeneration) != 264)
        return fail("SharedVrPoseState reference generation offset mismatch");

    if (fnvxr::shared::VrPoseSharedVersion != 7)
        return fail("SharedVrPoseState reference-generation contract version mismatch");

    if (sizeof(fnvxr::shared::SharedVrOriginState) != 144
        || fnvxr::shared::VrOriginSharedVersion != 5
        || offsetof(fnvxr::shared::SharedVrOriginState, sequence) != 8
        || offsetof(fnvxr::shared::SharedVrOriginState, active) != 12
        || offsetof(fnvxr::shared::SharedVrOriginState, generation) != 16
        || offsetof(fnvxr::shared::SharedVrOriginState, poseSequence) != 20
        || offsetof(fnvxr::shared::SharedVrOriginState, poseFrame) != 24
        || offsetof(fnvxr::shared::SharedVrOriginState, originRot) != 32
        || offsetof(fnvxr::shared::SharedVrOriginState, originPos) != 48
        || offsetof(fnvxr::shared::SharedVrOriginState, renderPoseSequence) != 64
        || offsetof(fnvxr::shared::SharedVrOriginState, renderPoseFrame) != 72
        || offsetof(fnvxr::shared::SharedVrOriginState, renderedDisplayTime) != 80
        || offsetof(fnvxr::shared::SharedVrOriginState, renderCameraAddress) != 88
        || offsetof(fnvxr::shared::SharedVrOriginState, renderCameraWorldRot) != 96
        || offsetof(fnvxr::shared::SharedVrOriginState, renderCameraWorldPos) != 132)
    {
        return fail("SharedVrOriginState authoritative recenter layout mismatch");
    }

    if (fnvxr::shared::StereoProducerDrawReplay != 1
        || fnvxr::shared::StereoProducerNativeSameFrame != 2
        || fnvxr::shared::StereoProducerSingleTraversal != 3)
    {
        return fail("shared stereo producer provenance values mismatch");
    }

    const std::uint32_t aimTrackingBits =
        fnvxr::shared::VrPoseTrackingLeftAimActive
        | fnvxr::shared::VrPoseTrackingRightAimActive
        | fnvxr::shared::VrPoseTrackingLeftAimCurrent
        | fnvxr::shared::VrPoseTrackingRightAimCurrent;
    if (aimTrackingBits != 0x1e0u)
        return fail("SharedVrPoseState aim tracking bits mismatch");

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

    if (!fnvxr::shared::runtimeGameplayPhase(
            fnvxr::shared::RuntimePhaseGameplay,
            fnvxr::shared::RuntimeMenuModeBit,
            0))
    {
        return fail("diagnostic menu-mode bit alone must not block proven retail gameplay");
    }

    if (fnvxr::shared::runtimeUiActive(
            fnvxr::shared::RuntimePhaseGameplay,
            fnvxr::shared::RuntimeMenuModeBit,
            0))
    {
        return fail("diagnostic menu-mode bit alone must not activate flat UI");
    }

    if (!fnvxr::shared::runtimeUiActive(
            fnvxr::shared::RuntimePhaseMenu,
            fnvxr::shared::RuntimeGenericMenuBit,
            0))
    {
        return fail("visible generic retail menu must activate flat UI");
    }

    if (!fnvxr::shared::runtimeUiInputAllowed(fnvxr::shared::RuntimeGenericMenuBit))
        return fail("visible generic retail menu must allow UI input");

    if (fnvxr::shared::runtimeUiInputAllowed(fnvxr::shared::RuntimeMenuModeBit))
        return fail("diagnostic menu-mode bit alone must not enable UI input");

    if (fnvxr::shared::runtimeUiInputAllowed(
            fnvxr::shared::RuntimeGenericMenuBit | fnvxr::shared::RuntimeLoadingMenuBit))
    {
        return fail("loading state must suppress UI input");
    }

    if (!fnvxr::shared::runtimeGameplayPhase(fnvxr::shared::RuntimePhaseGameplay, 0, 0))
        return fail("menu-free retail gameplay must remain stereo eligible");

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

    if (fnvxr::shared::D3D9StereoFrameSharedVersion != 3
        || sizeof(fnvxr::shared::SharedD3D9StereoFrameHeader) != 184)
        return fail("SharedD3D9StereoFrameHeader size mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, writing) != 12
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, sequence) != 16)
        return fail("SharedD3D9StereoFrameHeader transaction offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, poseValid) != 48)
        return fail("SharedD3D9StereoFrameHeader poseValid offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, renderedDisplayTime) != 56)
        return fail("SharedD3D9StereoFrameHeader renderedDisplayTime offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, leftEyeRot) != 64)
        return fail("SharedD3D9StereoFrameHeader leftEyeRot offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, producerMode) != 152)
        return fail("SharedD3D9StereoFrameHeader producerMode offset mismatch");

    if (offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, renderPairSequence) != 156
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, leftPayloadOffset) != 160
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, totalMappingBytes) != 168)
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
