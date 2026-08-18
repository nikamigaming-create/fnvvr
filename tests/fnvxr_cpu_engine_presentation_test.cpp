#include "fnvxr_cpu_engine_presentation.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
namespace presentation = fnvxr::host::cpu_engine_presentation;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

presentation::RuntimeSample menuRuntime(std::uint64_t sample)
{
    presentation::RuntimeSample runtime {};
    runtime.sample = sample;
    runtime.phase = fnvxr::shared::RuntimePhaseMenu;
    runtime.menuBits = fnvxr::shared::RuntimeGenericMenuBit;
    runtime.cameraActive = true;
    runtime.fresh = true;
    return runtime;
}

presentation::RuntimeSample gameplayRuntime(std::uint64_t sample)
{
    presentation::RuntimeSample runtime {};
    runtime.sample = sample;
    runtime.phase = fnvxr::shared::RuntimePhaseGameplay;
    runtime.cameraActive = true;
    runtime.fresh = true;
    return runtime;
}

presentation::RuntimeSample pipBoyRuntime(std::uint64_t sample)
{
    presentation::RuntimeSample runtime {};
    runtime.sample = sample;
    runtime.phase = fnvxr::shared::RuntimePhaseMenu;
    runtime.menuBits = fnvxr::shared::RuntimeMenuModeBit
        | fnvxr::shared::RuntimePipBoyMenuBit;
    runtime.cameraActive = true;
    runtime.fresh = true;
    return runtime;
}

presentation::FrameIdentity flatUiFrame(
    std::uint64_t transaction,
    std::uint64_t sourceFrame,
    std::uint64_t runtimeSample)
{
    presentation::FrameIdentity frame {};
    frame.transactionId = transaction;
    frame.sourceFrame = sourceFrame;
    frame.runtimeStateSample = runtimeSample;
    frame.producerMode = fnvxr::shared::StereoProducerMonoUiQuad;
    frame.uiActive = true;
    frame.pixelsComplete = true;
    return frame;
}

presentation::FrameIdentity worldFrame(
    std::uint64_t transaction,
    std::uint64_t sourceFrame,
    std::uint64_t runtimeSample)
{
    presentation::FrameIdentity frame {};
    frame.transactionId = transaction;
    frame.sourceFrame = sourceFrame;
    frame.runtimeStateSample = runtimeSample;
    frame.producerMode = fnvxr::shared::StereoProducerEngineCenter;
    frame.separated = true;
    frame.worldCandidate = true;
    frame.pixelsComplete = true;
    return frame;
}
}

int main()
{
    namespace liveContract = fnvxr::engine::live_pipboy;
    const liveContract::FocusDecision hover = liveContract::assessFocus({
        true, false, 6u, 12u,
    });
    const liveContract::FocusDecision focus = liveContract::assessFocus({
        true, false, 12u, 12u,
    });
    require(
        hover.hovered && !hover.focused && hover.scale > 1.0f
            && focus.focused && focus.requestOpen
            && focus.scale > hover.scale,
        "pointing did not smoothly focus and enlarge the live Pip-Boy");
    require(
        liveContract::weaponOrbitSlot(0.0f, 1.0f) == 0
            && liveContract::weaponOrbitSlot(1.0f, 0.0f) == 2
            && liveContract::weaponOrbitSlot(0.0f, -1.0f) == 4
            && liveContract::weaponOrbitSlot(-1.0f, 0.0f) == 6
            && liveContract::weaponOrbitSlot(0.1f, 0.1f) == -1,
        "weapon orbit selector lost its eight-slot/deadzone contract");
    require(
        liveContract::physicalControl(0.08f, 0.1f)
                == liveContract::PhysicalControl::StatsDial
            && liveContract::physicalControl(0.08f, 0.5f)
                == liveContract::PhysicalControl::ItemsDial
            && liveContract::physicalControl(0.08f, 0.9f)
                == liveContract::PhysicalControl::DataDial
            && liveContract::physicalControl(0.92f, 0.2f)
                == liveContract::PhysicalControl::ScrollUp
            && liveContract::physicalControl(0.5f, 0.5f)
                == liveContract::PhysicalControl::Screen,
        "live Pip-Boy screen/dial control zones changed");

    const presentation::RuntimeSample menu = menuRuntime(40u);
    const presentation::FrameIdentity ui = flatUiFrame(100u, 1000u, 40u);
    require(
        presentation::flatUiFrameEligible(ui, menu),
        "an exact, visible menu frame did not select the flat UI route");
    require(
        presentation::confirmedRuntimeMode(menu)
            == presentation::RuntimeMode::Ui,
        "a confirmed menu runtime did not select UI mode");
    require(
        presentation::confirmedRuntimeMode(gameplayRuntime(41u))
            == presentation::RuntimeMode::Gameplay,
        "a confirmed gameplay runtime did not select gameplay mode");
    const presentation::RuntimeSample livePipBoyRuntime =
        pipBoyRuntime(42u);
    require(
        presentation::confirmedRuntimeMode(livePipBoyRuntime)
                == presentation::RuntimeMode::Gameplay
            && !presentation::runtimeUiConfirmed(livePipBoyRuntime),
        "live Pip-Boy focus did not retain binocular presentation mode");

    const presentation::RuntimeSample laterMenu = menuRuntime(64u);
    require(
        presentation::retainedFlatUiFrameEligible(
            ui,
            menu,
            laterMenu,
            true),
        "a verified menu texture did not remain eligible as its UI epoch advanced");
    require(
        !presentation::retainedFlatUiFrameEligible(
            ui,
            menu,
            gameplayRuntime(65u),
            true),
        "a retained menu texture survived a confirmed gameplay transition");
    require(
        !presentation::retainedFlatUiFrameEligible(
            ui,
            menu,
            laterMenu,
            false),
        "a retained menu texture crossed a closed UI epoch");

    const presentation::UiBoundary uiBoundary =
        presentation::boundaryFromUi(ui);
    require(
        uiBoundary.transactionId == 100u && uiBoundary.sourceFrame == 1000u,
        "the UI boundary lost its full ordering identity");

    presentation::FrameIdentity staleWorld = worldFrame(99u, 999u, 42u);
    require(
        !presentation::binocularWorldFrameEligible(
            staleWorld,
            gameplayRuntime(42u),
            uiBoundary),
        "a world frame published before the menu boundary was revived");

    presentation::FrameIdentity sameTransactionWorld = worldFrame(100u, 1000u, 42u);
    require(
        !presentation::binocularWorldFrameEligible(
            sameTransactionWorld,
            gameplayRuntime(42u),
            uiBoundary),
        "a world frame reused the menu transaction identity");

    const presentation::FrameIdentity resumedWorld = worldFrame(101u, 1001u, 42u);
    require(
        presentation::binocularWorldFrameEligible(
            resumedWorld,
            gameplayRuntime(42u),
            uiBoundary),
        "the first new post-menu world pair did not resume binocular presentation");
    require(
        presentation::binocularWorldFrameEligible(
            resumedWorld,
            livePipBoyRuntime,
            {}),
        "a current live Pip-Boy world pair was rejected");

    // Source-pose freshness controls advancing proof, not whether the verified
    // world layer remains visible between exact producer updates.
    const presentation::WorldPresentationDecision staleWorldDecision =
        presentation::assessBinocularWorldFrame(
            resumedWorld,
            gameplayRuntime(42u),
            gameplayRuntime(99u),
            uiBoundary,
            false);
    require(
        staleWorldDecision.present && !staleWorldDecision.advancesFreshProof,
        "a stale verified world frame caused a presentation dropout");
    const presentation::WorldPresentationDecision freshWorldDecision =
        presentation::assessBinocularWorldFrame(
            resumedWorld,
            gameplayRuntime(42u),
            gameplayRuntime(99u),
            uiBoundary,
            true);
    require(
        freshWorldDecision.present && freshWorldDecision.advancesFreshProof,
        "a fresh verified world frame did not advance producer evidence");
    const presentation::RuntimeSample missingSourceRuntime {};
    const presentation::WorldPresentationDecision historyMissDecision =
        presentation::assessBinocularWorldFrame(
            resumedWorld,
            missingSourceRuntime,
            gameplayRuntime(99u),
            uiBoundary,
            false);
    require(
        historyMissDecision.present
            && !historyMissDecision.advancesFreshProof,
        "a transient source-runtime history miss blanked retained gameplay");
    require(
        presentation::preserveVerifiedWorldAcrossCellChange(true, true)
            && !presentation::preserveVerifiedWorldAcrossCellChange(false, true)
            && !presentation::preserveVerifiedWorldAcrossCellChange(true, false),
        "cell transition retention escaped the verified CPU-world boundary");
    presentation::FrameIdentity wrongSampleUi = ui;
    wrongSampleUi.runtimeStateSample = 39u;
    require(
        !presentation::flatUiFrameEligible(wrongSampleUi, menu),
        "a menu frame with a stale runtime sample was accepted");

    presentation::FrameIdentity invisibleUi = ui;
    invisibleUi.pixelsComplete = false;
    require(
        !presentation::flatUiFrameEligible(invisibleUi, menu),
        "a menu frame without verified pixels was accepted");

    presentation::FrameIdentity relabeledUi = ui;
    relabeledUi.separated = true;
    relabeledUi.worldCandidate = true;
    require(
        !presentation::flatUiBoundaryValid(relabeledUi),
        "a binocular candidate was relabeled as a flat UI frame");
    require(
        !presentation::binocularWorldFrameEligible(
            relabeledUi,
            gameplayRuntime(42u),
            {}),
        "a flat UI producer was admitted to the binocular world route");

    presentation::FrameIdentity wrongSampleWorld = resumedWorld;
    wrongSampleWorld.runtimeStateSample = 41u;
    require(
        !presentation::binocularWorldFrameEligible(
            wrongSampleWorld,
            gameplayRuntime(42u),
            uiBoundary),
        "a world frame with stale runtime lineage was accepted");

    presentation::FrameIdentity missingIdentity = resumedWorld;
    missingIdentity.sourceFrame = 0u;
    require(
        !presentation::binocularWorldFrameEligible(
            missingIdentity,
            gameplayRuntime(42u),
            uiBoundary),
        "a world frame without a source-frame identity was accepted");

    presentation::RuntimeSample unfreshGameplay = gameplayRuntime(42u);
    unfreshGameplay.fresh = false;
    require(
        !presentation::binocularWorldFrameEligible(
            resumedWorld,
            unfreshGameplay,
            uiBoundary),
        "an unverified runtime observation was accepted for world presentation");

    std::cout << "CPU engine UI/world transition contract passed\n";
    return EXIT_SUCCESS;
}
