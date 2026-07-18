#include "fnvxr_gpu_color_route.h"

#include <cstdlib>
#include <iostream>

namespace
{
using namespace fnvxr::host::gpu_color;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

ConsumerFrame completeFrame(
    fnvxr::gpu::color_v5::PresentationMode mode =
        fnvxr::gpu::color_v5::PresentationMode::BinocularWorld)
{
    ConsumerFrame frame {};
    frame.presentationMode = mode;
    frame.format = fnvxr::gpu::GpuInteropFormat::R8G8B8A8Unorm;
    frame.width = 2064u;
    frame.height = 2208u;
    frame.resourceSetId = 1u;
    frame.producerEpoch = 2u;
    frame.producerProcessId = 3u;
    frame.adapterLuid = 4u;
    frame.transactionId = 5u;
    frame.sourceFrame = 6u;
    frame.poseSequence = 7u;
    frame.runtimeStateSample = 8u;
    frame.renderedDisplayTime = 9;
    frame.readySequence = 1u;
    frame.releaseSequence = 2u;
    return frame;
}
}

int main()
{
    const ConsumerFrame world = completeFrame();
    for (const ConsumeDisposition disposition : {
            ConsumeDisposition::FrameReady,
            ConsumeDisposition::NoNewFrame })
    {
        const RoutedFrame routed = routeConsumedFrame(
            { disposition, ConsumerFailure::None, world },
            true,
            true);
        require(routed.content == RoutedContent::BinocularWorld
                && routed.bindLeftEyeView
                && routed.bindRightEyeView,
            "an accepted binocular Consumer frame did not bind both eyes");
        require(routed.retainedConsumerFrame
                == (disposition == ConsumeDisposition::NoNewFrame),
            "NoNewFrame retention was not derived from Consumer disposition");
    }

    require(routeConsumedFrame(
                { ConsumeDisposition::Rejected,
                    ConsumerFailure::UnstablePublication,
                    world },
                true,
                true).content == RoutedContent::SafetyBlank,
        "Rejected retained an earlier-looking binocular frame");
    require(routeConsumedFrame(
                { ConsumeDisposition::FrameReady,
                    ConsumerFailure::None,
                    world },
                false,
                true).content == RoutedContent::SafetyBlank,
        "binocular world admitted a missing left GPU view");

    ConsumerFrame invalid = world;
    invalid.releaseSequence = invalid.readySequence + 3u;
    require(routeConsumedFrame(
                { ConsumeDisposition::NoNewFrame,
                    ConsumerFailure::None,
                    invalid },
                true,
                true).content == RoutedContent::SafetyBlank,
        "NoNewFrame admitted an invalid Consumer fence interval");

    invalid = world;
    invalid.format = static_cast<fnvxr::gpu::GpuInteropFormat>(0xFFFFFFFFu);
    require(routeConsumedFrame(
                { ConsumeDisposition::FrameReady,
                    ConsumerFailure::None,
                    invalid },
                true,
                true).content == RoutedContent::SafetyBlank,
        "route admitted an unknown GPU color format");

    const RoutedFrame ui = routeConsumedFrame(
        { ConsumeDisposition::FrameReady,
            ConsumerFailure::None,
            completeFrame(
                fnvxr::gpu::color_v5::PresentationMode::MonoUiQuad) },
        true,
        true);
    require(ui.content == RoutedContent::MonoUiQuad
            && !ui.bindLeftEyeView
            && !ui.bindRightEyeView
            && ui.bindMonoUiView,
        "MonoUiQuad was relabeled as binocular world");

    require(routeConsumedFrame(
                { ConsumeDisposition::FrameReady,
                    ConsumerFailure::None,
                    completeFrame(
                        fnvxr::gpu::color_v5::PresentationMode::MonoUiQuad) },
                true,
                false).content == RoutedContent::SafetyBlank,
        "MonoUiQuad admitted an incomplete GPU resource pair");

    const SourcePoseLineage lineage {
        world.poseSequence,
        world.renderedDisplayTime,
        0x900u,
        4u,
        true,
        true,
    };
    require(exactSourcePoseLineage(world, lineage, 0x900u, 4u),
        "exact v5/shared-pose lineage was rejected");
    SourcePoseLineage wrongLineage = lineage;
    ++wrongLineage.poseSequence;
    require(!exactSourcePoseLineage(world, wrongLineage, 0x900u, 4u),
        "pose-sequence mismatch admitted as exact lineage");
    wrongLineage = lineage;
    ++wrongLineage.predictedDisplayTime;
    require(!exactSourcePoseLineage(world, wrongLineage, 0x900u, 4u),
        "display-time mismatch admitted as exact lineage");
    wrongLineage = lineage;
    wrongLineage.completeEyePosesAndFov = false;
    require(!exactSourcePoseLineage(world, wrongLineage, 0x900u, 4u),
        "incomplete eye pose/FOV admitted as exact lineage");
    wrongLineage = lineage;
    wrongLineage.distinctBinocularViews = false;
    require(!exactSourcePoseLineage(world, wrongLineage, 0x900u, 4u),
        "non-binocular views admitted as exact lineage");
    require(!exactSourcePoseLineage(world, lineage, 0x901u, 4u)
            && !exactSourcePoseLineage(world, lineage, 0x900u, 5u),
        "host lifetime or reference-space mismatch admitted as exact lineage");

    RuntimeEvidence runtime {};
    runtime.sample = 8u;
    runtime.phase = fnvxr::shared::RuntimePhaseMenu;
    runtime.menuBits = fnvxr::shared::RuntimePipBoyMenuBit;
    runtime.cameraActive = false;
    runtime.fresh = true;
    const fnvxr::product::PresentationInput uiInput =
        makePresentationInput(runtime, ui, false, true);
    require(uiInput.ui.completeForUiQuad()
            && !uiInput.stereo.completeForWorldStereo(),
        "MonoUiQuad generated anything other than a complete UI proof");
    fnvxr::product::PresentationController controller;
    const fnvxr::product::PresentationDecision uiDecision =
        controller.advance(uiInput);
    require(uiDecision.mode == fnvxr::product::PresentationMode::UiQuad
            && uiDecision.pointerEnabled
            && uiDecision.presentedUiSourceFrame == ui.frame.sourceFrame,
        "v5 MonoUiQuad did not pass through PresentationController");
    const ProductCompositionBindings uiBindings =
        selectProductComposition(uiDecision, false, true);
    require(uiBindings.content == RoutedContent::MonoUiQuad
            && uiBindings.bindMonoUiView
            && uiBindings.pointerEnabled
            && !uiBindings.bindLeftEyeView
            && !uiBindings.bindRightEyeView,
        "controller UI decision did not exclusively select the UI binding");
    require(selectProductComposition(
                uiDecision,
                false,
                false).content == RoutedContent::SafetyBlank,
        "controller UI decision bypassed missing retained UI resources");

    ConsumerFrame newerWorld = world;
    newerWorld.transactionId = 10u;
    newerWorld.sourceFrame = 11u;
    newerWorld.poseSequence = 12u;
    newerWorld.renderedDisplayTime = 13;
    const RoutedFrame routedWorld = routeConsumedFrame(
        { ConsumeDisposition::FrameReady,
            ConsumerFailure::None,
            newerWorld },
        true,
        true);
    runtime.phase = fnvxr::shared::RuntimePhaseGameplay;
    runtime.menuBits = 0u;
    runtime.cameraActive = true;
    constexpr ProducerWorldEvidence completeProducerWorldEvidence {
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
    };
    require(!makePresentationInput(
                runtime,
                routedWorld,
                true,
                true).stereo.completeForWorldStereo(),
        "presentation mode alone fabricated producer-only world evidence");
    const fnvxr::product::PresentationInput completeWorldInput =
        makePresentationInput(
            runtime,
            routedWorld,
            true,
            true,
            completeProducerWorldEvidence);
    require(completeWorldInput.stereo.completeForWorldStereo()
            && !completeWorldInput.ui.completeForUiQuad(),
        "BinocularWorld generated anything other than a complete stereo proof");
    RuntimeEvidence advancedRuntime = runtime;
    ++advancedRuntime.sample;
    require(stableRuntimeLineage(runtime, advancedRuntime),
        "stable gameplay rejected a lagged source runtime sample");
    require(makePresentationInput(
                runtime,
                routedWorld,
                true,
                true,
                completeProducerWorldEvidence).stereo.completeForWorldStereo(),
        "historical exact source sample did not authorize its stereo proof");
    RuntimeEvidence menuTransition = advancedRuntime;
    menuTransition.phase = fnvxr::shared::RuntimePhaseMenu;
    menuTransition.menuBits = fnvxr::shared::RuntimePipBoyMenuBit;
    menuTransition.cameraActive = false;
    require(!stableRuntimeLineage(runtime, menuTransition),
        "gameplay source survived a current menu/camera transition");
    RuntimeEvidence cameraTransition = advancedRuntime;
    cameraTransition.cameraActive = false;
    require(!stableRuntimeLineage(runtime, cameraTransition),
        "gameplay source survived a current camera transition");
    RuntimeEvidence futureRuntime = runtime;
    --futureRuntime.sample;
    require(!stableRuntimeLineage(runtime, futureRuntime),
        "future source runtime sample was admitted");
    RuntimeEvidenceHistory<3u> runtimeHistory;
    runtimeHistory.record(runtime);
    RuntimeEvidence historicalRuntime {};
    require(runtimeHistory.findStableSource(
                runtime.sample,
                advancedRuntime,
                historicalRuntime)
            && historicalRuntime.sample == runtime.sample,
        "bounded runtime history did not resolve a stable lagged sample");
    require(!runtimeHistory.findStableSource(
                runtime.sample,
                menuTransition,
                historicalRuntime),
        "bounded runtime history admitted a state-transition mismatch");
    RuntimeEvidence conflictingRuntime = runtime;
    conflictingRuntime.cameraActive = false;
    runtimeHistory.record(conflictingRuntime);
    require(!runtimeHistory.findStableSource(
                runtime.sample,
                advancedRuntime,
                historicalRuntime),
        "conflicting states reused one runtime sample identity");
    runtimeHistory.reset();
    require(!runtimeHistory.findStableSource(
                runtime.sample,
                advancedRuntime,
                historicalRuntime),
        "runtime history reset retained a source sample");
    RuntimeEvidence wrongRuntime = runtime;
    ++wrongRuntime.sample;
    require(!makePresentationInput(
                wrongRuntime,
                routedWorld,
                true,
                true,
                completeProducerWorldEvidence).stereo.completeForWorldStereo(),
        "runtime-state mismatch admitted a world proof");
    require(!makePresentationInput(
                runtime,
                routedWorld,
                true,
                false,
                completeProducerWorldEvidence).stereo.completeForWorldStereo(),
        "stale source admitted a world proof");
    const fnvxr::product::PresentationDecision missingLineage =
        controller.advance(makePresentationInput(
            runtime,
            routedWorld,
            false,
            true,
            completeProducerWorldEvidence));
    require(missingLineage.mode == fnvxr::product::PresentationMode::UiQuad
            && missingLineage.transitionHold,
        "missing source lineage bypassed the controller transition hold");
    const fnvxr::product::PresentationDecision worldDecision =
        controller.advance(completeWorldInput);
    require(worldDecision.mode == fnvxr::product::PresentationMode::WorldStereo
            && worldDecision.gameplayVrAccepted
            && !worldDecision.hudVisible,
        "complete v5 binocular proof did not pass PresentationController");
    const ProductCompositionBindings worldBindings =
        selectProductComposition(worldDecision, true, false);
    require(worldBindings.content == RoutedContent::BinocularWorld
            && worldBindings.bindLeftEyeView
            && worldBindings.bindRightEyeView
            && !worldBindings.bindMonoUiView
            && !worldBindings.pointerEnabled,
        "controller world decision did not exclusively bind both eyes");
    require(selectProductComposition(
                worldDecision,
                false,
                false).content == RoutedContent::SafetyBlank,
        "controller world decision bypassed missing world resources");
    fnvxr::product::PresentationDecision forbiddenHud = worldDecision;
    forbiddenHud.hudVisible = true;
    require(selectProductComposition(
                forbiddenHud,
                true,
                true).content == RoutedContent::SafetyBlank,
        "product composition admitted a gameplay HUD decision");

    const RoutedFrame retainedWorld = routeConsumedFrame(
        { ConsumeDisposition::NoNewFrame,
            ConsumerFailure::None,
            newerWorld },
        true,
        true);
    require(retainedWorld.retainedConsumerFrame,
        "NoNewFrame did not preserve its Consumer-owned identity marker");
    const fnvxr::product::PresentationDecision repeated = controller.advance(
        makePresentationInput(
            runtime,
            retainedWorld,
            true,
            true,
            completeProducerWorldEvidence));
    require(repeated.mode == fnvxr::product::PresentationMode::SafetyBlank
            && !repeated.gameplayVrAccepted,
        "repeated NoNewFrame identity bypassed controller replay rejection");

    std::cout << "GPU color host route fail-closed tests passed\n";
    return EXIT_SUCCESS;
}
