#include "fnvxr_product_contract.h"

#include <cstdlib>
#include <iostream>

namespace
{
constexpr std::uint64_t DefaultRuntimeStateSample = 0x1000;

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

fnvxr::product::StereoFrameProof completeStereo(
    std::uint64_t sourceFrame = 100,
    std::uint64_t runtimeStateSample = DefaultRuntimeStateSample)
{
    fnvxr::product::StereoFrameProof proof {};
    proof.transactionId = sourceFrame + 1000;
    proof.sourceFrame = sourceFrame;
    proof.poseSequence = sourceFrame + 2000;
    proof.runtimeStateSample = runtimeStateSample;
    proof.colorPairComplete = true;
    proof.depthPairComplete = true;
    proof.sameSimulationTick = true;
    proof.poseMatched = true;
    proof.conservativeVisibilityComplete = true;
    proof.resourceGraphComplete = true;
    proof.exactShaderSemantics = true;
    proof.gpuSynchronized = true;
    proof.distinctBinocularViews = true;
    proof.independentTranslational6Dof = true;
    proof.independentRotational6Dof = true;
    proof.authoritativeTrackedRetailWeapon = true;
    proof.authoritativeMuzzleAlignment = true;
    proof.gameplayHudExcluded = true;
    proof.fresh = true;
    return proof;
}

fnvxr::product::UiFrameProof completeUi(
    std::uint64_t sourceFrame = 50,
    std::uint64_t runtimeStateSample = DefaultRuntimeStateSample)
{
    return { sourceFrame, runtimeStateSample, true, true, true };
}

fnvxr::product::PresentationInput input(
    std::uint32_t phase,
    std::uint32_t menuBits,
    const fnvxr::product::StereoFrameProof& stereo = {},
    const fnvxr::product::UiFrameProof& ui = completeUi(),
    std::uint64_t runtimeStateSample = DefaultRuntimeStateSample)
{
    fnvxr::product::PresentationInput value {};
    value.runtimeStateSample = runtimeStateSample;
    value.runtimePhase = phase;
    value.menuBits = menuBits;
    value.runtimeFresh = true;
    value.cameraActive = true;
    value.stereo = stereo;
    value.ui = ui;
    return value;
}
}

int main()
{
    using namespace fnvxr::product;
    using namespace fnvxr::shared;

    const std::uint32_t interactiveMenus[] = {
        RuntimeStartMenuBit,
        RuntimeRaceSexMenuBit,
        RuntimeDialogMenuBit,
        RuntimeVatsMenuBit,
        RuntimePipBoyMenuBit,
        RuntimeGenericMenuBit,
    };

    for (const std::uint32_t menuBit : interactiveMenus)
    {
        PresentationController controller;
        const PresentationDecision decision = controller.advance(
            input(RuntimePhaseMenu, menuBit));
        if (decision.mode != PresentationMode::UiQuad)
            return fail("every interactive retail UI state must use the UI quad");
        if (!decision.pointerEnabled)
            return fail("interactive retail UI must retain ray-to-mouse pointer input");
        if (decision.gameplayVrAccepted || decision.hudVisible)
            return fail("UI quad must never count as gameplay VR or enable a gameplay HUD");
        if (decision.presentedUiSourceFrame != 50)
            return fail("interactive retail UI must identify the exact displayed source frame");
    }

    {
        PresentationController controller;
        constexpr std::uint64_t GameplayStateSample = 0x2000;
        constexpr std::uint64_t MenuStateSample = 0x2001;
        const UiFrameProof priorGameplayMono = completeUi(
            600,
            GameplayStateSample);

        const PresentationDecision gameplay = controller.advance(input(
            RuntimePhaseGameplay,
            0,
            {},
            priorGameplayMono,
            GameplayStateSample));
        if (gameplay.mode != PresentationMode::SafetyBlank)
            return fail("gameplay mono capture fixture must begin in safety blank");

        const PresentationDecision menu = controller.advance(input(
            RuntimePhaseMenu,
            RuntimePipBoyMenuBit,
            {},
            priorGameplayMono,
            MenuStateSample));
        if (menu.mode != PresentationMode::SafetyBlank
            || menu.pointerEnabled
            || menu.presentedUiSourceFrame != 0
            || menu.transitionHold)
        {
            return fail(
                "fresh gameplay pixels with the same source frame must not satisfy a later UI sample");
        }

        const PresentationDecision afterRejectedUi = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, {}));
        if (afterRejectedUi.mode != PresentationMode::SafetyBlank
            || afterRejectedUi.transitionHold
            || afterRejectedUi.presentedUiSourceFrame != 0)
        {
            return fail("runtime-sample-mismatched UI proof must not latch a transition quad");
        }
    }

    {
        PresentationController controller;
        const PresentationDecision loading = controller.advance(
            input(RuntimePhaseLoading, RuntimeLoadingMenuBit));
        if (loading.mode != PresentationMode::UiQuad
            || loading.pointerEnabled
            || loading.presentedUiSourceFrame != 50)
            return fail("loading must use a non-interactive retail UI quad");
    }

    {
        PresentationController controller;
        const PresentationDecision gameplay = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo()));
        if (gameplay.mode != PresentationMode::WorldStereo
            || !gameplay.gameplayVrAccepted
            || gameplay.pointerEnabled
            || gameplay.hudVisible)
        {
            return fail("complete gameplay must be HUD-free stereo with no UI pointer");
        }
    }

    {
        PresentationController controller;
        PresentationInput zeroSample = input(
            RuntimePhaseGameplay,
            0,
            completeStereo(),
            {},
            0);
        const PresentationDecision decision = controller.advance(zeroSample);
        if (decision.mode != PresentationMode::SafetyBlank
            || decision.reason != DecisionReason::UnknownRuntime
            || decision.gameplayVrAccepted)
        {
            return fail("fresh runtime evidence with sample identity zero must be unknown");
        }
    }

    {
        PresentationController controller;
        constexpr std::uint64_t UiStateSample = 0x3000;
        constexpr std::uint64_t GameplayStateSample = 0x3001;
        const PresentationDecision menu = controller.advance(input(
            RuntimePhaseMenu,
            RuntimePipBoyMenuBit,
            {},
            completeUi(700, UiStateSample),
            UiStateSample));
        if (menu.mode != PresentationMode::UiQuad)
            return fail("stereo sample-binding fixture must establish a confirmed UI quad");

        const PresentationDecision wrongSampleStereo = controller.advance(input(
            RuntimePhaseGameplay,
            0,
            completeStereo(701, UiStateSample),
            {},
            GameplayStateSample));
        if (wrongSampleStereo.mode != PresentationMode::UiQuad
            || !wrongSampleStereo.transitionHold
            || wrongSampleStereo.gameplayVrAccepted
            || wrongSampleStereo.presentedUiSourceFrame != 700)
        {
            return fail("world pair produced under UI must not satisfy the next gameplay sample");
        }

        const PresentationDecision currentSampleStereo = controller.advance(input(
            RuntimePhaseGameplay,
            0,
            completeStereo(702, GameplayStateSample),
            {},
            GameplayStateSample));
        if (!currentSampleStereo.gameplayVrAccepted
            || currentSampleStereo.mode != PresentationMode::WorldStereo)
        {
            return fail("world pair bound to the current gameplay sample must remain acceptable");
        }
    }

    {
        PresentationController controller;
        StereoFrameProof missingDepth = completeStereo();
        missingDepth.depthPairComplete = false;
        const PresentationDecision decision = controller.advance(
            input(RuntimePhaseGameplay, 0, missingDepth, {}));
        if (decision.mode != PresentationMode::SafetyBlank || decision.gameplayVrAccepted)
            return fail("color-only gameplay must fail closed, never become a quad or accepted VR");
    }

    {
        PresentationController controller;
        StereoFrameProof weakShaderProof = completeStereo();
        weakShaderProof.exactShaderSemantics = false;
        const PresentationDecision decision = controller.advance(
            input(RuntimePhaseGameplay, 0, weakShaderProof, {}));
        if (decision.mode != PresentationMode::SafetyBlank)
            return fail("partial shader coverage must fail closed");
    }

    {
        struct RequiredProductEvidence
        {
            bool StereoFrameProof::* field;
            const char* failure;
        };
        const RequiredProductEvidence required[] = {
            { &StereoFrameProof::distinctBinocularViews,
              "identical or unproven eye views must fail closed" },
            { &StereoFrameProof::independentTranslational6Dof,
              "unproven independent translational tracking must fail closed" },
            { &StereoFrameProof::independentRotational6Dof,
              "unproven independent rotational tracking must fail closed" },
            { &StereoFrameProof::authoritativeTrackedRetailWeapon,
              "a non-authoritative or untracked retail weapon must fail closed" },
            { &StereoFrameProof::authoritativeMuzzleAlignment,
              "unproven authoritative muzzle alignment must fail closed" },
            { &StereoFrameProof::gameplayHudExcluded,
              "unproven gameplay HUD exclusion must fail closed" },
        };

        for (const RequiredProductEvidence& evidence : required)
        {
            PresentationController controller;
            StereoFrameProof incomplete = completeStereo();
            incomplete.*(evidence.field) = false;
            const PresentationDecision decision = controller.advance(
                input(RuntimePhaseGameplay, 0, incomplete, {}));
            if (decision.mode != PresentationMode::SafetyBlank
                || decision.gameplayVrAccepted)
            {
                return fail(evidence.failure);
            }
        }
    }

    {
        PresentationController controller;
        const PresentationDecision menu = controller.advance(
            input(RuntimePhaseMenu, RuntimePipBoyMenuBit));
        if (menu.mode != PresentationMode::UiQuad)
            return fail("Pip-Boy must establish UI-quad mode");
        if (menu.presentedUiSourceFrame != 50)
            return fail("Pip-Boy must retain the exact displayed retail source frame");

        for (std::uint32_t frame = 0; frame < MaxUiToStereoHoldFrames; ++frame)
        {
            const PresentationDecision waiting = controller.advance(
                input(RuntimePhaseGameplay, 0, {}, completeUi(900 + frame)));
            if (waiting.mode != PresentationMode::UiQuad
                || !waiting.transitionHold
                || waiting.pointerEnabled
                || waiting.gameplayVrAccepted
                || waiting.presentedUiSourceFrame != 50)
            {
                return fail("UI exit may retain only the exact quad actually displayed in confirmed UI");
            }
        }

        const PresentationDecision expired = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, {}));
        if (expired.mode != PresentationMode::SafetyBlank)
            return fail("UI-to-stereo transition hold must be bounded");

        const PresentationDecision ready = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(101), completeUi(50)));
        if (ready.mode != PresentationMode::WorldStereo || !ready.gameplayVrAccepted)
            return fail("fresh stereo must atomically replace the transition quad");

        const PresentationDecision dropout = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, completeUi(51)));
        if (dropout.mode != PresentationMode::SafetyBlank || dropout.transitionHold)
            return fail("stereo loss during gameplay must never fall back to a mono gameplay quad");
    }

    {
        PresentationController controller;
        const PresentationDecision unknown = controller.advance(
            input(RuntimePhaseUnknown, 0, {}, completeUi(700)));
        if (unknown.mode != PresentationMode::SafetyBlank
            || unknown.presentedUiSourceFrame != 0
            || unknown.pointerEnabled)
        {
            return fail("unknown runtime state must not display or latch an arbitrary UI frame");
        }

        const PresentationDecision gameplay = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, {}));
        if (gameplay.mode != PresentationMode::SafetyBlank || gameplay.transitionHold)
            return fail("unknown runtime state must not prime a UI-to-stereo hold");
    }

    {
        PresentationController controller;
        const PresentationDecision menu = controller.advance(
            input(RuntimePhaseMenu, RuntimePipBoyMenuBit, {}, completeUi(71)));
        if (menu.mode != PresentationMode::UiQuad)
            return fail("confirmed UI fixture must display before unknown-state invalidation");

        const PresentationDecision unknown = controller.advance(
            input(RuntimePhaseUnknown, 0, {}, completeUi(72)));
        if (unknown.mode != PresentationMode::SafetyBlank
            || unknown.presentedUiSourceFrame != 0)
        {
            return fail("unknown state must never expose either a retained or incoming UI quad");
        }

        const PresentationDecision gameplay = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, {}));
        if (gameplay.mode != PresentationMode::SafetyBlank || gameplay.transitionHold)
            return fail("unknown-state interruption must invalidate a prior UI transition hold");
    }

    {
        PresentationController controller;
        const PresentationDecision accepted = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(200), {}));
        if (!accepted.gameplayVrAccepted)
            return fail("monotonic-identity fixture must accept its first complete transaction");

        const PresentationDecision replay = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(200), {}));
        if (replay.mode != PresentationMode::SafetyBlank || replay.gameplayVrAccepted)
            return fail("an exactly repeated stereo identity must never be accepted twice");

        const PresentationDecision newer = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(201), {}));
        if (!newer.gameplayVrAccepted)
            return fail("a strictly newer complete transaction must remain acceptable after a replay rejection");
    }

    {
        struct IdentityRegression
        {
            enum Field { Transaction, Source, Pose } field;
            const char* failure;
        };
        const IdentityRegression regressions[] = {
            { IdentityRegression::Transaction,
              "transaction identity must increase strictly" },
            { IdentityRegression::Source,
              "source-frame identity must increase strictly" },
            { IdentityRegression::Pose,
              "pose identity must increase strictly" },
        };

        for (const IdentityRegression& regression : regressions)
        {
            PresentationController controller;
            const StereoFrameProof baseline = completeStereo(300);
            if (!controller.advance(input(RuntimePhaseGameplay, 0, baseline, {})).gameplayVrAccepted)
                return fail("identity-regression fixture must accept its baseline");

            StereoFrameProof candidate = completeStereo(301);
            if (regression.field == IdentityRegression::Transaction)
                candidate.transactionId = baseline.transactionId;
            else if (regression.field == IdentityRegression::Source)
                candidate.sourceFrame = baseline.sourceFrame;
            else
                candidate.poseSequence = baseline.poseSequence;

            const PresentationDecision rejected = controller.advance(
                input(RuntimePhaseGameplay, 0, candidate, {}));
            if (rejected.mode != PresentationMode::SafetyBlank
                || rejected.gameplayVrAccepted)
            {
                return fail(regression.failure);
            }
        }
    }

    {
        PresentationController controller;
        if (!controller.advance(
                input(RuntimePhaseGameplay, 0, completeStereo(400), {})).gameplayVrAccepted)
        {
            return fail("cross-UI identity fixture must accept its initial gameplay frame");
        }
        if (controller.advance(
                input(RuntimePhaseMenu, RuntimePipBoyMenuBit, {}, completeUi(401))).mode
            != PresentationMode::UiQuad)
        {
            return fail("cross-UI identity fixture must enter confirmed UI");
        }
        const PresentationDecision replayAfterUi = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(400), {}));
        if (!replayAfterUi.transitionHold
            || replayAfterUi.gameplayVrAccepted
            || replayAfterUi.presentedUiSourceFrame != 401)
        {
            return fail("entering UI must not reset accepted stereo identity watermarks");
        }
    }

    {
        PresentationController controller;
        if (!controller.advance(
                input(RuntimePhaseGameplay, 0, completeStereo(500), {})).gameplayVrAccepted)
        {
            return fail("stale-UI fixture must accept its baseline gameplay frame");
        }

        const PresentationDecision staleUi = controller.advance(
            input(RuntimePhaseMenu, RuntimePipBoyMenuBit, {}, completeUi(499)));
        if (staleUi.mode != PresentationMode::SafetyBlank
            || staleUi.pointerEnabled
            || staleUi.presentedUiSourceFrame != 0
            || staleUi.transitionHold)
        {
            return fail("UI older than accepted gameplay must not display, enable input, or latch");
        }

        const PresentationDecision gameplay = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, {}));
        if (gameplay.mode != PresentationMode::SafetyBlank
            || gameplay.pointerEnabled
            || gameplay.presentedUiSourceFrame != 0
            || gameplay.transitionHold)
        {
            return fail("rejected stale UI must not prime a later UI-to-stereo hold");
        }
    }

    {
        PresentationController controller;
        if (!controller.advance(
                input(RuntimePhaseGameplay, 0, completeStereo(100), {})).gameplayVrAccepted)
        {
            return fail("post-UI freshness fixture must accept its baseline gameplay frame");
        }
        const PresentationDecision menu = controller.advance(
            input(RuntimePhaseMenu, RuntimePipBoyMenuBit, {}, completeUi(1000)));
        if (menu.mode != PresentationMode::UiQuad
            || menu.presentedUiSourceFrame != 1000)
        {
            return fail("post-UI freshness fixture must retain UI source frame 1000");
        }

        const PresentationDecision olderThanUi = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(101), {}));
        if (olderThanUi.gameplayVrAccepted
            || olderThanUi.mode != PresentationMode::UiQuad
            || !olderThanUi.transitionHold
            || olderThanUi.presentedUiSourceFrame != 1000)
        {
            return fail("stereo older than the retained UI frame must not replace that quad");
        }

        const PresentationDecision postUi = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(1001), {}));
        if (!postUi.gameplayVrAccepted || postUi.mode != PresentationMode::WorldStereo)
            return fail("stereo strictly newer than the retained UI frame must replace it atomically");
    }

    {
        PresentationController controller;
        if (!controller.advance(
                input(RuntimePhaseGameplay, 0, completeStereo(100), {})).gameplayVrAccepted)
        {
            return fail("expired post-UI freshness fixture must accept its baseline gameplay frame");
        }
        if (controller.advance(
                input(RuntimePhaseMenu, RuntimePipBoyMenuBit, {}, completeUi(1000))).mode
            != PresentationMode::UiQuad)
        {
            return fail("expired post-UI freshness fixture must enter confirmed UI");
        }

        for (std::uint32_t frame = 0; frame < MaxUiToStereoHoldFrames; ++frame)
            controller.advance(input(RuntimePhaseGameplay, 0, {}, {}));

        const PresentationDecision expired = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, {}));
        if (expired.mode != PresentationMode::SafetyBlank)
            return fail("expired post-UI freshness fixture must reach safety blank");

        const PresentationDecision staleAfterExpiry = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(101), {}));
        if (staleAfterExpiry.mode != PresentationMode::SafetyBlank
            || staleAfterExpiry.gameplayVrAccepted)
        {
            return fail("UI source freshness watermark must survive transition-hold expiry");
        }

        const PresentationDecision postUi = controller.advance(
            input(RuntimePhaseGameplay, 0, completeStereo(1001), {}));
        if (!postUi.gameplayVrAccepted || postUi.mode != PresentationMode::WorldStereo)
            return fail("post-UI stereo may recover from blank only after it is newer than UI");
    }

    {
        PresentationController controller;
        const PresentationDecision diagnosticMenuBit = controller.advance(
            input(RuntimePhaseGameplay, RuntimeMenuModeBit, completeStereo()));
        if (diagnosticMenuBit.mode != PresentationMode::WorldStereo)
            return fail("the raw diagnostic MenuMode bit alone must not flatten gameplay");
    }

    {
        PresentationController controller;
        PresentationInput staleRuntime = input(RuntimePhaseGameplay, 0, completeStereo());
        staleRuntime.runtimeFresh = false;
        const PresentationDecision decision = controller.advance(staleRuntime);
        if (decision.mode != PresentationMode::SafetyBlank
            || decision.pointerEnabled
            || decision.gameplayVrAccepted
            || decision.presentedUiSourceFrame != 0)
        {
            return fail("unknown/stale runtime must fail blank and cannot latch a retail UI quad");
        }
    }

    std::cout << "fnvxr product presentation contract PASS\n";
    return EXIT_SUCCESS;
}
