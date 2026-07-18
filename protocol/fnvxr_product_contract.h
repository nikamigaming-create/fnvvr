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
    bool colorPairComplete = false;
    bool depthPairComplete = false;
    bool sameSimulationTick = false;
    bool poseMatched = false;
    bool conservativeVisibilityComplete = false;
    bool resourceGraphComplete = false;
    bool exactShaderSemantics = false;
    bool gpuSynchronized = false;
    bool fresh = false;

    bool completeForWorldStereo() const
    {
        return transactionId != 0
            && sourceFrame != 0
            && poseSequence != 0
            && colorPairComplete
            && depthPairComplete
            && sameSimulationTick
            && poseMatched
            && conservativeVisibilityComplete
            && resourceGraphComplete
            && exactShaderSemantics
            && gpuSynchronized
            && fresh;
    }
};

struct UiFrameProof
{
    std::uint64_t sourceFrame = 0;
    bool retailColorComplete = false;
    bool fresh = false;
    bool retailOwned = false;

    bool completeForUiQuad() const
    {
        return sourceFrame != 0 && retailColorComplete && fresh && retailOwned;
    }
};

struct PresentationInput
{
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
    // The product has no persistent gameplay HUD. Retail interface pixels are
    // shown only inside UiQuad and are not a separate synthetic overlay.
    bool hudVisible = false;
};

// At 72 Hz this is 250 ms. It is deliberately fixed rather than exposed as a
// tuning switch. Expiry is a visible acceptance failure, not a mono fallback.
inline constexpr std::uint32_t MaxUiToStereoHoldFrames = 18;

inline RetailState classifyRetailState(const PresentationInput& input)
{
    if (!input.runtimeFresh)
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
        if (retailState != RetailState::Gameplay)
        {
            awaitingFreshStereoAfterUi_ = true;
            stereoEstablishedForGameplay_ = false;
            uiToStereoHoldFrames_ = 0;

            PresentationDecision decision {};
            decision.reason = retailState == RetailState::InteractiveUi
                ? DecisionReason::RetailUi
                : retailState == RetailState::Loading
                    ? DecisionReason::RetailLoading
                    : DecisionReason::UnknownRuntime;
            if (input.ui.completeForUiQuad())
            {
                decision.mode = PresentationMode::UiQuad;
                decision.pointerEnabled = retailState == RetailState::InteractiveUi
                    && shared::runtimeUiInputAllowed(input.menuBits);
            }
            return decision;
        }

        if (input.stereo.completeForWorldStereo())
        {
            awaitingFreshStereoAfterUi_ = false;
            stereoEstablishedForGameplay_ = true;
            uiToStereoHoldFrames_ = 0;
            PresentationDecision decision {};
            decision.mode = PresentationMode::WorldStereo;
            decision.reason = DecisionReason::StereoWorldReady;
            decision.gameplayVrAccepted = true;
            return decision;
        }

        if (awaitingFreshStereoAfterUi_
            && input.ui.completeForUiQuad()
            && uiToStereoHoldFrames_ < MaxUiToStereoHoldFrames)
        {
            ++uiToStereoHoldFrames_;
            PresentationDecision decision {};
            decision.mode = PresentationMode::UiQuad;
            decision.reason = DecisionReason::AwaitingFreshStereoAfterUi;
            decision.transitionHold = true;
            return decision;
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
    }

private:
    bool awaitingFreshStereoAfterUi_ = false;
    bool stereoEstablishedForGameplay_ = false;
    std::uint32_t uiToStereoHoldFrames_ = 0;
};
}
