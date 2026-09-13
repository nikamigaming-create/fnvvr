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
        // A blocking engine load can suspend runtime publication. Keep only
        // the pair already accepted in this lifetime, with its exact poses.
        // No UI or pointer input gains authority from this presentation hold.
        if (input.world.retainedForContinuity
            && isValid(lastAcceptedWorldSource_)
            && input.world.source == lastAcceptedWorldSource_
            && input.world.runtimeSample == lastAcceptedWorldRuntimeSample_
            && worldFailure(input) == DecisionReason::WorldFrameReady)
        {
            PresentationDecision held { PresentationMode::WorldStereo,
                DecisionReason::WorldFrameReady, input.world.source };
            held.spatialRigMayRender = input.trackedRigReady;
            return held;
        }
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
    lastAcceptedWorldRuntimeSample_ = 0;
}

PresentationDecision Coordinator::decideBlockingUi(const PresentationInput& input)
{
    // Menus own input, not the entire visual field. A verified world and an
    // independently verified menu surface can be composed in the same eyes.
    if (worldFailure(input) == DecisionReason::WorldFrameReady)
    {
        PresentationDecision decision { PresentationMode::WorldStereo,
            DecisionReason::WorldFrameReady, input.world.source };
        decision.spatialRigMayRender = input.trackedRigReady;
        decision.wristScreenMayRender = input.trackedRigReady
            && pipBoyRequested(input.runtime.ui) && input.wristContentReady;
        if (uiFailure(input) == DecisionReason::UiFrameReady)
        {
            retainedUiSource_ = input.ui.source;
            latestAcceptedUiSource_ = input.ui.source;
            retainedUiRuntimeSample_ = input.runtime.sample;
            holdAdvances_ = 0;
            decision.overlayUiSource = input.ui.source;
            decision.pointerMayRender = input.runtime.phase != RuntimePhase::Loading;
        }
        lastAcceptedWorldSource_ = input.world.source;
        lastAcceptedWorldRuntimeSample_ = input.world.runtimeSample;
        return decision;
    }
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

    if (!input.world.retainedForContinuity && isValid(latestAcceptedUiSource_)
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
    lastAcceptedWorldRuntimeSample_ = input.world.runtimeSample;
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
        || (input.ui.runtimeSample != input.runtime.sample
            && (input.ui.verifiedRuntimeSample != input.runtime.sample
                || input.ui.runtimeSample > input.runtime.sample)))
    {
        return DecisionReason::UiRuntimeLineageInvalid;
    }
    if (!input.ui.gpu.completeFor(input.ui.source))
        return DecisionReason::UiGpuEvidenceInvalid;
    // World and menu pixels are independent producer channels. A newer world
    // transaction must not blink an otherwise fresh, same-context menu out.
    // Order UI against its own accepted source, with runtime/GPU checks above.
    if (isValid(latestAcceptedUiSource_)
        && !isSameOrNewer(input.ui.source, latestAcceptedUiSource_))
    {
        return DecisionReason::UiSourceRegression;
    }
    return DecisionReason::UiFrameReady;
}

DecisionReason Coordinator::worldFailure(const PresentationInput& input) const
{
    // Retention never accepts a new source. Its pixels, poses, GPU ownership,
    // and original runtime observation must still identify the accepted pair.
    const bool pausedWorld = (input.runtime.ui == UiClassification::PipBoySpatial
            || input.world.retainedForContinuity)
        && isValid(lastAcceptedWorldSource_)
        && input.world.source == lastAcceptedWorldSource_
        && lastAcceptedWorldRuntimeSample_ != 0
        && input.world.runtimeSample == lastAcceptedWorldRuntimeSample_;
    if (!isValid(input.world.source)
        || !input.world.completeStereoPair
        || !input.world.distinctEyeViews
        || (!input.world.fresh && !pausedWorld))
    {
        return DecisionReason::WorldFrameIncomplete;
    }
    if (!input.world.stereoIdentity.coherentWith(input.world.source))
        return DecisionReason::WorldStereoIdentityInvalid;
    if (!pausedWorld && (!input.world.runtimeLineageVerified
        || (input.world.runtimeSample != input.runtime.sample
            && (input.world.verifiedRuntimeSample != input.runtime.sample
                || input.world.runtimeSample > input.runtime.sample))))
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
