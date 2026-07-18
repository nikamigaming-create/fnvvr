#pragma once

#include "fnvxr_shared_state.h"

#include <cstdint>

namespace fnvxr::product
{
// Product presentation has exactly two user-visible content modes. SafetyBlank
// is an error state and can never satisfy gameplay acceptance.
enum class PresentationMode : std::uint32_t
{
    SafetyBlank = 0,
    UiQuad = 1,
    WorldStereo = 2,
};

enum class RetailState : std::uint32_t
{
    Unknown = 0,
    InteractiveUi = 1,
    Loading = 2,
    Gameplay = 3,
};

enum class DecisionReason : std::uint32_t
{
    IncompleteEvidence = 0,
    RetailUi = 1,
    RetailLoading = 2,
    UnknownRuntime = 3,
    AwaitingFreshStereoAfterUi = 4,
    StereoWorldReady = 5,
    StereoGameplayLost = 6,
};

struct StereoFrameProof
{
    std::uint64_t transactionId = 0;
    std::uint64_t sourceFrame = 0;
    std::uint64_t poseSequence = 0;
    std::uint64_t runtimeStateSample = 0;
    bool colorPairComplete = false;
    bool depthPairComplete = false;
    bool sameSimulationTick = false;
    bool poseMatched = false;
    bool conservativeVisibilityComplete = false;
    bool resourceGraphComplete = false;
    bool exactShaderSemantics = false;
    bool gpuSynchronized = false;
    bool distinctBinocularViews = false;
    bool independentTranslational6Dof = false;
    bool independentRotational6Dof = false;
    bool authoritativeTrackedRetailWeapon = false;
    bool authoritativeMuzzleAlignment = false;
    bool gameplayHudExcluded = false;
    bool fresh = false;

    bool completeForWorldStereo() const
    {
        return transactionId != 0
            && sourceFrame != 0
            && poseSequence != 0
            && runtimeStateSample != 0
            && colorPairComplete
            && depthPairComplete
            && sameSimulationTick
            && poseMatched
            && conservativeVisibilityComplete
            && resourceGraphComplete
            && exactShaderSemantics
            && gpuSynchronized
            && distinctBinocularViews
            && independentTranslational6Dof
            && independentRotational6Dof
            && authoritativeTrackedRetailWeapon
            && authoritativeMuzzleAlignment
            && gameplayHudExcluded
            && fresh;
    }
};

struct UiFrameProof
{
    std::uint64_t sourceFrame = 0;
    // Identity of the runtime-state observation under which these pixels were
    // captured. Source-frame equality alone cannot prove gameplay pixels came
    // from the currently confirmed UI state.
    std::uint64_t runtimeStateSample = 0;
    bool retailColorComplete = false;
    bool fresh = false;
    bool retailOwned = false;

    bool completeForUiQuad() const
    {
        return sourceFrame != 0
            && runtimeStateSample != 0
            && retailColorComplete
            && fresh
            && retailOwned;
    }
};

struct PresentationInput
{
    // Identity of the exact runtime-state sample represented by runtimePhase,
    // menuBits, showroomActive, runtimeFresh, and cameraActive.
    std::uint64_t runtimeStateSample = 0;
    std::uint32_t runtimePhase = shared::RuntimePhaseUnknown;
    std::uint32_t menuBits = 0;
    std::uint32_t showroomActive = 0;
    bool runtimeFresh = false;
    bool cameraActive = false;
    StereoFrameProof stereo {};
    UiFrameProof ui {};
};

struct PresentationDecision
{
    PresentationMode mode = PresentationMode::SafetyBlank;
    DecisionReason reason = DecisionReason::IncompleteEvidence;
    bool pointerEnabled = false;
    bool gameplayVrAccepted = false;
    bool transitionHold = false;
    // Nonzero only when UiQuad is visible. A transition hold must carry the
    // exact source frame that was displayed during confirmed retail UI.
    std::uint64_t presentedUiSourceFrame = 0;
    // The product has no persistent gameplay HUD. Retail interface pixels are
    // shown only inside UiQuad and are not a separate synthetic overlay.
    bool hudVisible = false;
};

// At 72 Hz this is 250 ms. It is deliberately fixed rather than exposed as a
// tuning switch. Expiry is a visible acceptance failure, not a mono fallback.
inline constexpr std::uint32_t MaxUiToStereoHoldFrames = 18;

inline RetailState classifyRetailState(const PresentationInput& input)
{
    if (!input.runtimeFresh || input.runtimeStateSample == 0)
        return RetailState::Unknown;

    if (input.runtimePhase == shared::RuntimePhaseLoading
        || (input.menuBits & shared::RuntimeLoadingMenuBit) != 0u)
    {
        return RetailState::Loading;
    }

    if (input.cameraActive
        && shared::runtimeGameplayPhase(
            input.runtimePhase,
            input.menuBits,
            input.showroomActive))
    {
        return RetailState::Gameplay;
    }

    if (input.runtimePhase == shared::RuntimePhaseMenu
        || (input.menuBits & shared::RuntimeInteractiveMenuBits) != 0u)
    {
        return RetailState::InteractiveUi;
    }

    return RetailState::Unknown;
}

class PresentationController
{
public:
    PresentationDecision advance(const PresentationInput& input)
    {
        const RetailState retailState = classifyRetailState(input);
        if (retailState == RetailState::Unknown)
        {
            // Unknown evidence cannot display or retain arbitrary UI pixels and
            // cannot establish a future UI-to-stereo transition hold.
            awaitingFreshStereoAfterUi_ = false;
            stereoEstablishedForGameplay_ = false;
            uiToStereoHoldFrames_ = 0;
            retainedUiSourceFrame_ = 0;
            retainedUiRuntimeStateSample_ = 0;

            PresentationDecision decision {};
            decision.reason = DecisionReason::UnknownRuntime;
            return decision;
        }

        if (retailState == RetailState::InteractiveUi
            || retailState == RetailState::Loading)
        {
            stereoEstablishedForGameplay_ = false;
            uiToStereoHoldFrames_ = 0;

            PresentationDecision decision {};
            decision.reason = retailState == RetailState::InteractiveUi
                ? DecisionReason::RetailUi
                : DecisionReason::RetailLoading;

            // Retained pixels from another runtime-state observation are not
            // evidence for this UI state, even if their source frame happens
            // to be equal and every freshness/color bit remains set.
            if (retainedUiRuntimeStateSample_ != input.runtimeStateSample)
            {
                retainedUiSourceFrame_ = 0;
                retainedUiRuntimeStateSample_ = 0;
            }

            // UI and stereo sourceFrame identities share one monotonic retail
            // render-frame domain. Never latch or display a UI proof older
            // than either accepted world output or confirmed UI output. The
            // proof must also originate from this exact confirmed state sample.
            if (input.ui.completeForUiQuad()
                && input.ui.runtimeStateSample == input.runtimeStateSample
                && input.ui.sourceFrame >= lastAcceptedSourceFrame_
                && input.ui.sourceFrame >= latestConfirmedUiSourceFrame_)
            {
                retainedUiSourceFrame_ = input.ui.sourceFrame;
                retainedUiRuntimeStateSample_ = input.ui.runtimeStateSample;
                latestConfirmedUiSourceFrame_ = input.ui.sourceFrame;
            }

            // A hold becomes eligible only after a concrete retail UI frame
            // has actually been selected for display in a confirmed UI state.
            awaitingFreshStereoAfterUi_ = retainedUiSourceFrame_ != 0
                && retainedUiRuntimeStateSample_ == input.runtimeStateSample;
            if (retainedUiSourceFrame_ != 0)
            {
                decision.mode = PresentationMode::UiQuad;
                decision.presentedUiSourceFrame = retainedUiSourceFrame_;
                decision.pointerEnabled = retailState == RetailState::InteractiveUi
                    && shared::runtimeUiInputAllowed(input.menuBits);
            }
            return decision;
        }

        // A complete pair is still stale if it was rendered under a different
        // runtime-state observation (for example, while UI was active). Only
        // the exact current gameplay sample can authorize world presentation.
        if (input.stereo.completeForWorldStereo()
            && input.stereo.runtimeStateSample == input.runtimeStateSample
            && stereoIdentityStrictlyNewer(input.stereo)
            && stereoPostDatesLatestUi(input.stereo))
        {
            awaitingFreshStereoAfterUi_ = false;
            stereoEstablishedForGameplay_ = true;
            uiToStereoHoldFrames_ = 0;
            retainedUiSourceFrame_ = 0;
            retainedUiRuntimeStateSample_ = 0;
            latestConfirmedUiSourceFrame_ = 0;
            lastAcceptedTransactionId_ = input.stereo.transactionId;
            lastAcceptedSourceFrame_ = input.stereo.sourceFrame;
            lastAcceptedPoseSequence_ = input.stereo.poseSequence;
            PresentationDecision decision {};
            decision.mode = PresentationMode::WorldStereo;
            decision.reason = DecisionReason::StereoWorldReady;
            decision.gameplayVrAccepted = true;
            return decision;
        }

        if (awaitingFreshStereoAfterUi_
            && retainedUiSourceFrame_ != 0
            && uiToStereoHoldFrames_ < MaxUiToStereoHoldFrames)
        {
            ++uiToStereoHoldFrames_;
            PresentationDecision decision {};
            decision.mode = PresentationMode::UiQuad;
            decision.reason = DecisionReason::AwaitingFreshStereoAfterUi;
            decision.transitionHold = true;
            decision.presentedUiSourceFrame = retainedUiSourceFrame_;
            return decision;
        }

        if (awaitingFreshStereoAfterUi_
            && uiToStereoHoldFrames_ >= MaxUiToStereoHoldFrames)
        {
            awaitingFreshStereoAfterUi_ = false;
            retainedUiSourceFrame_ = 0;
            retainedUiRuntimeStateSample_ = 0;
        }

        PresentationDecision decision {};
        decision.reason = stereoEstablishedForGameplay_
            ? DecisionReason::StereoGameplayLost
            : DecisionReason::IncompleteEvidence;
        return decision;
    }

    void reset()
    {
        awaitingFreshStereoAfterUi_ = false;
        stereoEstablishedForGameplay_ = false;
        uiToStereoHoldFrames_ = 0;
        retainedUiSourceFrame_ = 0;
        retainedUiRuntimeStateSample_ = 0;
        lastAcceptedTransactionId_ = 0;
        lastAcceptedSourceFrame_ = 0;
        lastAcceptedPoseSequence_ = 0;
        latestConfirmedUiSourceFrame_ = 0;
    }

private:
    bool stereoIdentityStrictlyNewer(const StereoFrameProof& proof) const
    {
        return proof.transactionId > lastAcceptedTransactionId_
            && proof.sourceFrame > lastAcceptedSourceFrame_
            && proof.poseSequence > lastAcceptedPoseSequence_;
    }

    bool stereoPostDatesLatestUi(const StereoFrameProof& proof) const
    {
        // UI and stereo sourceFrame values are retail render-frame identities
        // from one monotonic domain. Keep this watermark even after the
        // bounded UI quad hold expires: blank is safer than resurrecting a
        // world image captured before the UI frame the user just saw.
        return latestConfirmedUiSourceFrame_ == 0
            || proof.sourceFrame > latestConfirmedUiSourceFrame_;
    }

    bool awaitingFreshStereoAfterUi_ = false;
    bool stereoEstablishedForGameplay_ = false;
    std::uint32_t uiToStereoHoldFrames_ = 0;
    std::uint64_t retainedUiSourceFrame_ = 0;
    std::uint64_t retainedUiRuntimeStateSample_ = 0;
    std::uint64_t latestConfirmedUiSourceFrame_ = 0;
    std::uint64_t lastAcceptedTransactionId_ = 0;
    std::uint64_t lastAcceptedSourceFrame_ = 0;
    std::uint64_t lastAcceptedPoseSequence_ = 0;
};
}
