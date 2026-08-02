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

    if (sizeof(fnvxr::shared::SharedDesktopAssistState) != 240
        || fnvxr::shared::DesktopAssistSharedVersion != 2
        || offsetof(fnvxr::shared::SharedDesktopAssistState, sequence) != 8
        || offsetof(fnvxr::shared::SharedDesktopAssistState, flags) != 12
        || offsetof(fnvxr::shared::SharedDesktopAssistState, frame) != 16
        || offsetof(fnvxr::shared::SharedDesktopAssistState, cameraNodeAddress) != 24
        || offsetof(fnvxr::shared::SharedDesktopAssistState, poseSequence) != 28
        || offsetof(fnvxr::shared::SharedDesktopAssistState, poseProducerEpoch) != 32
        || offsetof(fnvxr::shared::SharedDesktopAssistState, playerWorldRot) != 40
        || offsetof(fnvxr::shared::SharedDesktopAssistState, playerWorldPos) != 76
        || offsetof(fnvxr::shared::SharedDesktopAssistState, cameraLocalRot) != 88
        || offsetof(fnvxr::shared::SharedDesktopAssistState, cameraLocalPos) != 124
        || offsetof(fnvxr::shared::SharedDesktopAssistState, cameraWorldRot) != 136
        || offsetof(fnvxr::shared::SharedDesktopAssistState, cameraWorldPos) != 172
        || offsetof(fnvxr::shared::SharedDesktopAssistState, bodyRootAddress) != 184
        || offsetof(fnvxr::shared::SharedDesktopAssistState, bodyRootWorldRot) != 192
        || offsetof(fnvxr::shared::SharedDesktopAssistState, bodyRootWorldPos) != 228)
    {
        return fail("SharedDesktopAssistState local-camera layout mismatch");
    }

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

    if (sizeof(fnvxr::shared::SharedVrPoseState) != 288)
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

    if (offsetof(fnvxr::shared::SharedVrPoseState, producerEpoch) != 272)
        return fail("SharedVrPoseState producer epoch offset mismatch");

    if (offsetof(fnvxr::shared::SharedVrPoseState, recenterRequestId) != 280)
        return fail("SharedVrPoseState recenter request offset mismatch");

    if (fnvxr::shared::VrPoseSharedVersion != 8)
        return fail("SharedVrPoseState event-mailbox contract version mismatch");

    if (sizeof(fnvxr::shared::SharedVrOriginState) != 216
        || fnvxr::shared::VrOriginSharedVersion != 6
        || offsetof(fnvxr::shared::SharedVrOriginState, sequence) != 8
        || offsetof(fnvxr::shared::SharedVrOriginState, active) != 12
        || offsetof(fnvxr::shared::SharedVrOriginState, generation) != 16
        || offsetof(fnvxr::shared::SharedVrOriginState, poseSequence) != 20
        || offsetof(fnvxr::shared::SharedVrOriginState, poseFrame) != 24
        || offsetof(fnvxr::shared::SharedVrOriginState, originRot) != 32
        || offsetof(fnvxr::shared::SharedVrOriginState, originPos) != 48
        || offsetof(fnvxr::shared::SharedVrOriginState, producerEpoch) != 64
        || offsetof(fnvxr::shared::SharedVrOriginState, renderPoseSequence) != 72
        || offsetof(fnvxr::shared::SharedVrOriginState, renderPoseFrame) != 80
        || offsetof(fnvxr::shared::SharedVrOriginState, renderedDisplayTime) != 88
        || offsetof(fnvxr::shared::SharedVrOriginState, renderCameraAddress) != 96
        || offsetof(fnvxr::shared::SharedVrOriginState, renderCameraWorldRot) != 104
        || offsetof(fnvxr::shared::SharedVrOriginState, renderCameraWorldPos) != 140
        || offsetof(fnvxr::shared::SharedVrOriginState, bodyRootAddress) != 152
        || offsetof(fnvxr::shared::SharedVrOriginState, bodyRootWorldRot) != 160
        || offsetof(fnvxr::shared::SharedVrOriginState, bodyRootWorldPos) != 196
        || offsetof(fnvxr::shared::SharedVrOriginState, bodyRootWorldScale) != 208)
    {
        return fail("SharedVrOriginState authoritative recenter layout mismatch");
    }

    if (fnvxr::shared::DesktopAssistUiQuadSharedVersion != 1
        || sizeof(fnvxr::shared::SharedDesktopAssistUiQuadHeader) != 96
        || offsetof(fnvxr::shared::SharedDesktopAssistUiQuadHeader, writing) != 12
        || offsetof(fnvxr::shared::SharedDesktopAssistUiQuadHeader, sequence) != 16
        || offsetof(fnvxr::shared::SharedDesktopAssistUiQuadHeader, runtimeStateSample) != 40
        || offsetof(fnvxr::shared::SharedDesktopAssistUiQuadHeader, poseProducerEpoch) != 80
        || offsetof(fnvxr::shared::SharedDesktopAssistUiQuadHeader, captureOrdinal) != 88)
    {
        return fail("SharedDesktopAssistUiQuadHeader lineage layout mismatch");
    }

    if (fnvxr::shared::D3D9StereoFrameSharedVersion != 8
        || sizeof(fnvxr::shared::SharedD3D9StereoFrameHeader) != 240
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, producerEpoch) != 176
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, rendererProducerEpoch) != 184
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, producerProcessId) != 192
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, publishedSlot) != 196
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, readerSlots) != 200
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, publicationGeneration) != 208
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, transactionId) != 216
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, sourceFrame) != 224
        || offsetof(fnvxr::shared::SharedD3D9StereoFrameHeader, runtimeStateSample) != 232
        || fnvxr::shared::D3D9StereoFrameReaderLaneCount != 2
        || fnvxr::shared::D3D9StereoFrameSlotCount != 4)
    {
        return fail("SharedD3D9StereoFrameHeader 64-bit producer identity mismatch");
    }
    if (std::strcmp(
            fnvxr::shared::D3D9StereoFrameSharedMappingName,
            "Local\\FNVXR_D3D9_StereoFrame_v8") != 0
        || std::strcmp(
            fnvxr::shared::D3D9StereoFrameProducerMutexName,
            "Local\\FNVXR_D3D9_Stereo_Producer_v8") != 0
        || std::strcmp(
            fnvxr::shared::D3D9StereoFrameHostReaderMutexName,
            "Local\\FNVXR_D3D9_Stereo_HostReader_v8") != 0
        || std::strcmp(
            fnvxr::shared::D3D9StereoFrameCaptureReaderMutexName,
            "Local\\FNVXR_D3D9_Stereo_CaptureReader_v8") != 0
        || fnvxr::shared::StereoProducerMonoUiQuad != 5u)
    {
        return fail("SharedD3D9StereoFrameHeader v8 names or mono UI producer mismatch");
    }

    LONG highSequence = 0;
    const std::uint32_t highEvenBits = 0x7ffffffeu;
    std::memcpy(&highSequence, &highEvenBits, sizeof(highSequence));
    if (!fnvxr::shared::beginSequencedSharedWrite(highSequence))
        return fail("high-bit sequence write did not begin");
    fnvxr::shared::endSequencedSharedWrite(highSequence);
    if (fnvxr::shared::sequencedValueBits(highSequence) != 0x80000000u
        || !fnvxr::shared::sequencedValueIsPublished(highSequence))
    {
        return fail("negative even sequence was not accepted modulo 2^32");
    }

    const std::uint32_t finalEvenBits = 0xfffffffeu;
    std::memcpy(&highSequence, &finalEvenBits, sizeof(highSequence));
    if (!fnvxr::shared::beginSequencedSharedWrite(highSequence))
        return fail("wrap sequence write did not begin");
    fnvxr::shared::endSequencedSharedWrite(highSequence);
    if (fnvxr::shared::sequencedValueBits(highSequence) != 2u
        || !fnvxr::shared::sequencedValueIsPublished(highSequence))
    {
        return fail("seqlock published the reserved zero sequence at wrap");
    }

    LONG publicationCounter = 0;
    const std::uint32_t publicationFinalBits = 0xffffffffu;
    std::memcpy(&publicationCounter, &publicationFinalBits, sizeof(publicationCounter));
    if (fnvxr::shared::sequencedValueBits(
            fnvxr::shared::incrementNonzeroSharedCounter(publicationCounter)) != 1u)
    {
        return fail("plain publication counter emitted zero at wrap");
    }
    LONG previousPublication = 0;
    std::memcpy(&previousPublication, &publicationFinalBits, sizeof(previousPublication));
    if (!fnvxr::shared::nonzeroSharedCounterAdvanced(publicationCounter, previousPublication))
        return fail("plain publication counter did not advance modularly across wrap");
    if (fnvxr::shared::nonzeroSharedCounterDistance(1u, 0xfffffff0u) != 16u
        || fnvxr::shared::nonzeroSharedCounterDistance(17u, 1u) != 16u
        || fnvxr::shared::nonzeroSharedCounterDistance(1u, 1u) != 0u
        || fnvxr::shared::nonzeroSharedCounterDistance(0u, 1u) != 0u)
    {
        return fail("nonzero publication distance mishandled wrap or sentinel zero");
    }

    LONG64 generationBeforeHighBit = 0;
    LONG64 generationAfterHighBit = 0;
    const std::uint64_t generationBeforeHighBitBits = 0x7fffffffffffffffull;
    const std::uint64_t generationAfterHighBitBits = 0x8000000000000000ull;
    std::memcpy(
        &generationBeforeHighBit,
        &generationBeforeHighBitBits,
        sizeof(generationBeforeHighBit));
    std::memcpy(
        &generationAfterHighBit,
        &generationAfterHighBitBits,
        sizeof(generationAfterHighBit));
    if (!fnvxr::shared::nonzeroSharedGenerationAdvanced(
            generationAfterHighBit,
            generationBeforeHighBit))
    {
        return fail("64-bit publication generation rejected a valid high-bit transition");
    }

    LONG64 generationBeforeWrap = 0;
    LONG64 generationAfterWrap = 1;
    const std::uint64_t generationBeforeWrapBits = 0xffffffffffffffffull;
    std::memcpy(
        &generationBeforeWrap,
        &generationBeforeWrapBits,
        sizeof(generationBeforeWrap));
    if (!fnvxr::shared::nonzeroSharedGenerationAdvanced(
            generationAfterWrap,
            generationBeforeWrap)
        || fnvxr::shared::nonzeroSharedGenerationAdvanced(
            generationBeforeWrap,
            generationAfterWrap)
        || fnvxr::shared::nonzeroSharedGenerationAdvanced(
            generationAfterWrap,
            0))
    {
        return fail("64-bit publication generation modular ordering mismatch");
    }

    if (fnvxr::shared::StereoProducerDrawReplay != 1
        || fnvxr::shared::StereoProducerNativeSameFrame != 2
        || fnvxr::shared::StereoProducerSingleTraversal != 3
        || fnvxr::shared::StereoProducerEngineCenter != 4)
    {
        return fail("shared stereo producer provenance values mismatch");
    }
    if (!fnvxr::shared::stereoProducerCarriesSameTransactionEyes(
            fnvxr::shared::StereoProducerNativeSameFrame)
        || !fnvxr::shared::stereoProducerCarriesSameTransactionEyes(
            fnvxr::shared::StereoProducerSingleTraversal)
        || !fnvxr::shared::stereoProducerCarriesSameTransactionEyes(
            fnvxr::shared::StereoProducerEngineCenter)
        || fnvxr::shared::stereoProducerCarriesSameTransactionEyes(
            fnvxr::shared::StereoProducerDrawReplay)
        || fnvxr::shared::stereoProducerCarriesSameTransactionEyes(
            fnvxr::shared::StereoProducerUnknown))
    {
        return fail("same-transaction stereo producer classification mismatch");
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

    if (fnvxr::shared::runtimeControllerMode(
            fnvxr::shared::RuntimePhaseMenu,
            fnvxr::shared::RuntimeGenericMenuBit,
            0u,
            false,
            true)
        != fnvxr::shared::RuntimeControllerMode::Ui)
    {
        return fail("an actionable menu did not select controller UI mode");
    }
    if (fnvxr::shared::runtimeControllerMode(
            fnvxr::shared::RuntimePhaseGameplay,
            0u,
            0u,
            true,
            true)
        != fnvxr::shared::RuntimeControllerMode::Gameplay)
    {
        return fail("a live menu-free camera did not select controller gameplay mode");
    }
    if (fnvxr::shared::runtimeControllerMode(
            fnvxr::shared::RuntimePhaseLoading,
            fnvxr::shared::RuntimeLoadingMenuBit,
            0u,
            false,
            true)
        != fnvxr::shared::RuntimeControllerMode::Unknown)
    {
        return fail("loading admitted controller mutation");
    }
    if (fnvxr::shared::runtimeControllerMode(
            fnvxr::shared::RuntimePhaseGameplay,
            0u,
            0u,
            false,
            true)
        != fnvxr::shared::RuntimeControllerMode::Unknown)
    {
        return fail("camera-less startup admitted gameplay controls");
    }
    if (fnvxr::shared::runtimeControllerMode(
            fnvxr::shared::RuntimePhaseMenu,
            fnvxr::shared::RuntimeGenericMenuBit,
            0u,
            false,
            false)
        != fnvxr::shared::RuntimeControllerMode::Unknown)
    {
        return fail("a stale menu sample admitted UI controls");
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

    if (fnvxr::shared::DesktopAssistUiQuadSharedMagic != 0x55585646u
        || fnvxr::shared::DesktopAssistUiQuadFlagLeaseCurrent == 0u
        || fnvxr::shared::DesktopAssistUiQuadFlagPresentHookInstalled == 0u
        || fnvxr::shared::DesktopAssistUiQuadFlagRuntimeUiConfirmed == 0u
        || fnvxr::shared::DesktopAssistUiQuadFlagPixelCopyComplete == 0u
        || fnvxr::shared::DesktopAssistUiQuadFlagPixelContentNonBlack == 0u
        || fnvxr::shared::DesktopAssistUiQuadFlagPoseEpochCurrent == 0u)
    {
        return fail("desktop assist UI quad protocol flags mismatch");
    }

    if (fnvxr::shared::D3D9StereoFrameSharedVersion != 8
        || sizeof(fnvxr::shared::SharedD3D9StereoFrameHeader) != 240)
        return fail("SharedD3D9StereoFrameHeader size mismatch");

    for (LONG publishedSlot = -1; publishedSlot < 4; ++publishedSlot)
    {
        for (LONG hostReaderSlot = -1; hostReaderSlot < 4; ++hostReaderSlot)
        {
            for (LONG captureReaderSlot = -1; captureReaderSlot < 4; ++captureReaderSlot)
            {
                const LONG writable = fnvxr::shared::selectWritableStereoFrameSlot(
                    publishedSlot, hostReaderSlot, captureReaderSlot);
                if (writable < 0
                    || writable >= 4
                    || writable == publishedSlot
                    || writable == hostReaderSlot
                    || writable == captureReaderSlot)
                {
                    return fail("four-slot stereo ring failed with two independent or stale readers");
                }
            }
        }
    }

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
