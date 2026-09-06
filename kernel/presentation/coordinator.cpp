#include "coordinator.h"

namespace fnvxr::kernel::presentation
{
namespace
{
bool blocksWorld(UiClassification classification)
{
    return classification == UiClassification::Blocking
        || classification == UiClassification::BlockingWithPipBoy;
}

bool pipBoyRequested(UiClassification classification)
{
    return classification == UiClassification::PipBoySpatial
        || classification == UiClassification::BlockingWithPipBoy;
}
}

Coordinator::Coordinator(CoordinatorPolicy policy)
    : policy_(policy)
{
}

PresentationDecision Coordinator::advance(const PresentationInput& input)
{
    if (!input.runtime.fresh
        || input.runtime.sample == 0
        || input.runtime.phase == RuntimePhase::Unknown)
    {
        reset();
        return {};
    }

    if (input.runtime.phase == RuntimePhase::Loading || blocksWorld(input.runtime.ui))
        return decideBlockingUi(input);

    return decideWorld(input);
}

void Coordinator::reset()
{
    clearRetainedUi();
    latestAcceptedUiSource_ = {};
    lastAcceptedWorldSource_ = {};
}

PresentationDecision Coordinator::decideBlockingUi(const PresentationInput& input)
{
    const DecisionReason failure = uiFailure(input);
    if (failure == DecisionReason::UiFrameReady)
    {
        retainedUiSource_ = input.ui.source;
        latestAcceptedUiSource_ = input.ui.source;
        retainedUiRuntimeSample_ = input.runtime.sample;
        holdAdvances_ = 0;
        PresentationDecision decision { PresentationMode::UiQuad,
            DecisionReason::UiFrameReady, retainedUiSource_ };
        decision.pointerMayRender = input.runtime.phase != RuntimePhase::Loading;
        return decision;
    }

    if (policy_.holdLastUiWhileBlocking
        && isValid(retainedUiSource_)
        && retainedUiRuntimeSample_ == input.runtime.sample)
    {
        return heldUi(DecisionReason::UiFrameHeld);
    }

    return { PresentationMode::SafetyBlank, failure };
}

PresentationDecision Coordinator::decideWorld(const PresentationInput& input)
{
    const DecisionReason failure = worldFailure(input);
    if (failure != DecisionReason::WorldFrameReady)
    {
        if (isValid(retainedUiSource_))
            return heldUi(DecisionReason::AwaitingNewerWorldHeld);
        return { PresentationMode::SafetyBlank, failure };
    }

    if (isValid(latestAcceptedUiSource_)
        && !isStrictlyNewer(input.world.source, latestAcceptedUiSource_))
    {
        if (isValid(retainedUiSource_)
            && holdAdvances_ < policy_.maxUiHoldAdvances)
            return heldUi(DecisionReason::AwaitingNewerWorldHeld);
        clearRetainedUi();
        return { PresentationMode::SafetyBlank, DecisionReason::WorldNotNewerThanUi };
    }

    if (isValid(lastAcceptedWorldSource_)
        && !isSameOrNewer(input.world.source, lastAcceptedWorldSource_))
    {
        return { PresentationMode::SafetyBlank,
            DecisionReason::WorldNotNewerThanAccepted };
    }

    const bool rigMayRender = input.trackedRigReady;
    const bool wristMayRender = rigMayRender
        && pipBoyRequested(input.runtime.ui)
        && input.wristContentReady;
    const SourceKey selected = input.world.source;
    lastAcceptedWorldSource_ = selected;
    clearRetainedUi();
    latestAcceptedUiSource_ = {};
    return { PresentationMode::WorldStereo, DecisionReason::WorldFrameReady,
        selected, rigMayRender, wristMayRender };
}

DecisionReason Coordinator::uiFailure(const PresentationInput& input) const
{
    if (!isValid(input.ui.source)
        || !input.ui.completeRetailColor
        || !input.ui.monoRetailView
        || !input.ui.fresh)
    {
        return DecisionReason::UiFrameIncomplete;
    }
    if (!input.ui.runtimeLineageVerified
        || input.ui.runtimeSample != input.runtime.sample)
    {
        return DecisionReason::UiRuntimeLineageInvalid;
    }
    if (!input.ui.gpu.completeFor(input.ui.source))
        return DecisionReason::UiGpuEvidenceInvalid;
    if (isValid(lastAcceptedWorldSource_)
        && !isSameOrNewer(input.ui.source, lastAcceptedWorldSource_))
    {
        return DecisionReason::UiSourceOlderThanWorld;
    }
    if (isValid(latestAcceptedUiSource_)
        && !isSameOrNewer(input.ui.source, latestAcceptedUiSource_))
    {
        return DecisionReason::UiSourceRegression;
    }
    return DecisionReason::UiFrameReady;
}

DecisionReason Coordinator::worldFailure(const PresentationInput& input) const
{
    if (!isValid(input.world.source)
        || !input.world.completeStereoPair
        || !input.world.distinctEyeViews
        || !input.world.fresh)
    {
        return DecisionReason::WorldFrameIncomplete;
    }
    if (!input.world.stereoIdentity.coherentWith(input.world.source))
        return DecisionReason::WorldStereoIdentityInvalid;
    if (!input.world.runtimeLineageVerified
        || input.world.runtimeSample != input.runtime.sample)
    {
        return DecisionReason::WorldRuntimeLineageInvalid;
    }
    if (!input.world.gpu.completeFor(input.world.source))
        return DecisionReason::WorldGpuEvidenceInvalid;
    if (!input.world.retail.renderComplete())
        return DecisionReason::WorldRetailPrerequisitesInvalid;
    if (!exactPoseExistsFor(input, input.world.source))
        return DecisionReason::WorldPoseJoinMissing;
    return DecisionReason::WorldFrameReady;
}

bool Coordinator::exactPoseExistsFor(
    const PresentationInput& input,
    const SourceKey& source) const
{
    return input.poseHistory.exact && input.poseHistory.source == source;
}

PresentationDecision Coordinator::heldUi(DecisionReason reason)
{
    if (holdAdvances_ >= policy_.maxUiHoldAdvances)
    {
        clearRetainedUi();
        return { PresentationMode::SafetyBlank, DecisionReason::HoldExpired };
    }

    ++holdAdvances_;
    PresentationDecision decision { PresentationMode::UiQuad, reason,
        retainedUiSource_ };
    decision.transitionHold = true;
    return decision;
}

void Coordinator::clearRetainedUi()
{
    retainedUiSource_ = {};
    retainedUiRuntimeSample_ = 0;
    holdAdvances_ = 0;
}
}
