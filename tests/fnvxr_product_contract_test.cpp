#include "fnvxr_product_contract.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

fnvxr::product::StereoFrameProof completeStereo(std::uint64_t sourceFrame = 100)
{
    fnvxr::product::StereoFrameProof proof {};
    proof.transactionId = sourceFrame + 1000;
    proof.sourceFrame = sourceFrame;
    proof.poseSequence = sourceFrame + 2000;
    proof.colorPairComplete = true;
    proof.depthPairComplete = true;
    proof.sameSimulationTick = true;
    proof.poseMatched = true;
    proof.conservativeVisibilityComplete = true;
    proof.resourceGraphComplete = true;
    proof.exactShaderSemantics = true;
    proof.gpuSynchronized = true;
    proof.fresh = true;
    return proof;
}

fnvxr::product::UiFrameProof completeUi(std::uint64_t sourceFrame = 50)
{
    return { sourceFrame, true, true, true };
}

fnvxr::product::PresentationInput input(
    std::uint32_t phase,
    std::uint32_t menuBits,
    const fnvxr::product::StereoFrameProof& stereo = {},
    const fnvxr::product::UiFrameProof& ui = completeUi())
{
    fnvxr::product::PresentationInput value {};
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
    }

    {
        PresentationController controller;
        const PresentationDecision loading = controller.advance(
            input(RuntimePhaseLoading, RuntimeLoadingMenuBit));
        if (loading.mode != PresentationMode::UiQuad || loading.pointerEnabled)
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
        PresentationController controller;
        const PresentationDecision menu = controller.advance(
            input(RuntimePhaseMenu, RuntimePipBoyMenuBit));
        if (menu.mode != PresentationMode::UiQuad)
            return fail("Pip-Boy must establish UI-quad mode");

        for (std::uint32_t frame = 0; frame < MaxUiToStereoHoldFrames; ++frame)
        {
            const PresentationDecision waiting = controller.advance(
                input(RuntimePhaseGameplay, 0, {}, completeUi(50)));
            if (waiting.mode != PresentationMode::UiQuad
                || !waiting.transitionHold
                || waiting.pointerEnabled
                || waiting.gameplayVrAccepted)
            {
                return fail("UI exit may hold the last quad briefly but may not accept it as gameplay");
            }
        }

        const PresentationDecision expired = controller.advance(
            input(RuntimePhaseGameplay, 0, {}, completeUi(50)));
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
        if (decision.mode != PresentationMode::UiQuad
            || decision.pointerEnabled
            || decision.gameplayVrAccepted)
        {
            return fail("unknown/stale runtime may show the retail safety quad but never gameplay VR");
        }
    }

    std::cout << "fnvxr product presentation contract PASS\n";
    return EXIT_SUCCESS;
}
