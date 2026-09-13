#include "fnvxr_product_kernel_adapter.h"

namespace fnvxr::host
{
namespace kp = kernel::presentation;

ProductKernelAdapter::ProductKernelAdapter(
    const kernel::ValidatedRuntimeConfig& config)
    : kernel_(kernel::ProductKernel::fromValidated(
          config, { product::MaxUiToStereoHoldFrames, true }))
{
}

product::PresentationDecision ProductKernelAdapter::advance(
    const product::PresentationInput& input,
    const PresentationTransportProof& transport,
    bool trackedRigReady,
    bool wristContentReady,
    const PresentationTransportProof& uiTransport)
{
    return translate(
        kernel_.present(translate(
            input, transport, trackedRigReady, wristContentReady, uiTransport)),
        input);
}

void ProductKernelAdapter::reset() noexcept
{
    kernel_.resetPresentation();
}

kp::RuntimeSnapshot ProductKernelAdapter::runtimeSnapshot(
    const product::PresentationInput& input) noexcept
{
    kp::RuntimeSnapshot runtime {};
    const product::RetailState retailState = product::classifyRetailState(input);
    runtime.sample = input.runtimeStateSample;
    runtime.fresh = input.runtimeFresh;
    runtime.phase = retailState == product::RetailState::Unknown
        ? kp::RuntimePhase::Unknown
        : retailState == product::RetailState::Loading
            ? kp::RuntimePhase::Loading : kp::RuntimePhase::Running;
    if (retailState == product::RetailState::InteractiveUi)
        runtime.ui = (input.menuBits & shared::RuntimePipBoyMenuBit) != 0
            ? kp::UiClassification::BlockingWithPipBoy
            : kp::UiClassification::Blocking;
    else if (retailState == product::RetailState::Gameplay
        && (input.menuBits & shared::RuntimePipBoyMenuBit) != 0)
        runtime.ui = kp::UiClassification::PipBoySpatial;
    return runtime;
}

kp::PresentationInput ProductKernelAdapter::translate(
    const product::PresentationInput& input,
    const PresentationTransportProof& transport,
    bool trackedRigReady,
    bool wristContentReady,
    const PresentationTransportProof& uiTransport) noexcept
{
    kp::PresentationInput translated {};
    translated.runtime = runtimeSnapshot(input);

    const kp::SourceKey worldSource {
        transport.producerEpoch,
        input.stereo.sourceFrame,
        input.stereo.transactionId };
    translated.world.source = worldSource;
    translated.world.runtimeSample = input.stereo.runtimeStateSample;
    translated.world.stereoIdentity = {
        worldSource, worldSource, input.stereo.sameSimulationTick };
    translated.world.gpu = {
        worldSource,
        transport.producerOwned,
        transport.completionObserved,
        transport.consumerAcquired,
        transport.exclusiveInterval,
        transport.synchronized && input.stereo.gpuSynchronized };
    translated.world.retail = {
        input.stereo.renderLocalDepthPairComplete,
        input.stereo.conservativeVisibilityComplete,
        input.stereo.resourceGraphComplete,
        input.stereo.exactShaderSemantics,
        input.stereo.independentTranslational6Dof,
        input.stereo.independentRotational6Dof,
        input.stereo.authoritativeTrackedRetailWeapon,
        input.stereo.authoritativeMuzzleAlignment,
        input.stereo.gameplayHudExcluded,
        input.stereo.retailRenderTransactionComplete };
    translated.world.completeStereoPair = input.stereo.colorPairComplete;
    translated.world.distinctEyeViews = input.stereo.distinctBinocularViews;
    translated.world.runtimeLineageVerified =
        input.stereo.runtimeStateSample == input.runtimeStateSample
        || input.worldRuntimeLineageVerified;
    translated.world.verifiedRuntimeSample = input.worldRuntimeLineageVerified
        ? input.runtimeStateSample : 0;
    translated.world.fresh = input.stereo.fresh;
    translated.world.retainedForContinuity = input.retainedWorldForContinuity;
    translated.poseHistory = { worldSource, input.stereo.poseMatched };

    const auto& menuTransport = uiTransport.producerEpoch != 0 ? uiTransport : transport;
    const kp::SourceKey uiSource {
        menuTransport.producerEpoch,
        input.ui.sourceFrame,
        menuTransport.transaction != 0
            ? menuTransport.transaction : input.ui.sourceFrame };
    translated.ui.source = uiSource;
    translated.ui.runtimeSample = input.ui.runtimeStateSample;
    translated.ui.gpu = {
        uiSource,
        menuTransport.producerOwned && input.ui.retailOwned,
        menuTransport.completionObserved,
        menuTransport.consumerAcquired,
        menuTransport.exclusiveInterval,
        menuTransport.synchronized };
    translated.ui.completeRetailColor = input.ui.retailColorComplete;
    translated.ui.monoRetailView = input.ui.retailOwned;
    translated.ui.runtimeLineageVerified =
        input.ui.runtimeStateSample == input.runtimeStateSample
        || input.uiRuntimeLineageVerified;
    translated.ui.verifiedRuntimeSample = input.uiRuntimeLineageVerified
        ? input.runtimeStateSample : 0;
    translated.ui.fresh = input.ui.fresh;
    translated.trackedRigReady = trackedRigReady;
    translated.wristContentReady = wristContentReady;
    return translated;
}

product::PresentationDecision ProductKernelAdapter::translate(
    const kp::PresentationDecision& decision,
    const product::PresentationInput& input) noexcept
{
    product::PresentationDecision translated {};
    translated.hudVisible = false;
    if (decision.mode == kp::PresentationMode::WorldStereo)
    {
        translated.mode = product::PresentationMode::WorldStereo;
        translated.reason = product::DecisionReason::StereoWorldReady;
        translated.stereoPresentationReady = true;
        translated.gameplayVrAccepted = input.stereo.completeForWorldStereo();
        translated.spatialRigMayRender = decision.spatialRigMayRender;
        translated.wristScreenMayRender = decision.wristScreenMayRender;
        translated.presentedUiSourceFrame = decision.overlayUiSource.frame;
        translated.pointerEnabled = decision.pointerMayRender
            && shared::runtimeUiInputAllowed(input.menuBits);
        translated.presentedSourceEpoch = decision.selectedSource.epoch;
        translated.presentedSourceFrame = decision.selectedSource.frame;
        translated.presentedSourceTransaction =
            decision.selectedSource.transaction;
        return translated;
    }
    if (decision.mode == kp::PresentationMode::UiQuad)
    {
        translated.mode = product::PresentationMode::UiQuad;
        translated.reason = decision.transitionHold
            ? product::DecisionReason::AwaitingFreshStereoAfterUi
            : product::classifyRetailState(input) == product::RetailState::Loading
                ? product::DecisionReason::RetailLoading
                : product::DecisionReason::RetailUi;
        translated.transitionHold = decision.transitionHold;
        translated.presentedUiSourceFrame = decision.selectedSource.frame;
        translated.presentedSourceEpoch = decision.selectedSource.epoch;
        translated.presentedSourceFrame = decision.selectedSource.frame;
        translated.presentedSourceTransaction =
            decision.selectedSource.transaction;
        translated.pointerEnabled = decision.pointerMayRender
            && shared::runtimeUiInputAllowed(input.menuBits);
        return translated;
    }

    const product::RetailState state = product::classifyRetailState(input);
    translated.reason = state == product::RetailState::Unknown
        ? product::DecisionReason::UnknownRuntime
        : state == product::RetailState::Gameplay
            ? product::DecisionReason::IncompleteEvidence
            : state == product::RetailState::Loading
                ? product::DecisionReason::RetailLoading
                : product::DecisionReason::RetailUi;
    return translated;
}
}
