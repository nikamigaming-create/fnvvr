#include "../host/fnvxr_host_ui_capture_gate.h"

#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::host::ui_capture::Decision;
using fnvxr::host::ui_capture::RuntimeSample;
using fnvxr::host::ui_capture::WindowCapture;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

RuntimeSample menu(std::uint64_t sample = 40u)
{
    return {
        sample,
        fnvxr::shared::RuntimePhaseMenu,
        fnvxr::shared::RuntimeGenericMenuBit,
        0u,
        false,
        true,
    };
}

RuntimeSample loading(std::uint64_t sample = 40u)
{
    return {
        sample,
        fnvxr::shared::RuntimePhaseLoading,
        fnvxr::shared::RuntimeLoadingMenuBit,
        0u,
        false,
        true,
    };
}

RuntimeSample gameplay(std::uint64_t sample = 40u)
{
    return {
        sample,
        fnvxr::shared::RuntimePhaseGameplay,
        0u,
        0u,
        true,
        true,
    };
}

WindowCapture completeCapture()
{
    return { 90u, 2048u, 1280u, 2048u * 1280u, true, true, true };
}
}

int main()
{
    using namespace fnvxr::host::ui_capture;
    using Failure = Decision::Failure;

    const Decision acceptedMenu = assess(menu(), completeCapture(), menu());
    require(acceptedMenu.accepted(), "stable confirmed menu capture was rejected");
    require(
        acceptedMenu.proof.sourceFrame == 90u
            && acceptedMenu.proof.runtimeStateSample == 40u
            && acceptedMenu.proof.retailColorComplete
            && acceptedMenu.proof.fresh
            && acceptedMenu.proof.retailOwned,
        "accepted UI proof lost exact capture/runtime identity");
    require(
        assess(loading(), completeCapture(), loading()).accepted(),
        "stable loading capture was rejected");

    const Decision advancedMenu = assess(
        menu(40u),
        completeCapture(),
        menu(47u));
    require(
        advancedMenu.accepted()
            && advancedMenu.failure == Failure::None
            && advancedMenu.proof.runtimeStateSample == 47u,
        "monotonically advancing identical UI state did not bind the post-copy sample");
    fnvxr::product::PresentationInput advancedPresentation {};
    advancedPresentation.runtimeStateSample = 47u;
    advancedPresentation.runtimePhase = fnvxr::shared::RuntimePhaseMenu;
    advancedPresentation.menuBits = fnvxr::shared::RuntimeGenericMenuBit;
    advancedPresentation.runtimeFresh = true;
    advancedPresentation.ui = advancedMenu.proof;
    fnvxr::product::PresentationController advancedController;
    require(
        advancedController.advance(advancedPresentation).mode
            == fnvxr::product::PresentationMode::UiQuad,
        "post-copy runtime evidence did not admit its matching UI proof");
    advancedPresentation.runtimeStateSample = 40u;
    fnvxr::product::PresentationController staleController;
    require(
        staleController.advance(advancedPresentation).mode
            == fnvxr::product::PresentationMode::SafetyBlank,
        "a post-copy UI proof was relabeled as its older pre-copy sample");

    const Decision regressed = assess(
        menu(47u),
        completeCapture(),
        menu(40u));
    require(
        !regressed.accepted()
            && regressed.failure == Failure::RuntimeSampleRegressed,
        "regressed runtime identity survived the capture sandwich");

    require(
        !assess(gameplay(), completeCapture(), gameplay()).accepted(),
        "gameplay pixels were admitted as a mono UI quad");

    RuntimeSample changedSample = menu(41u);
    require(
        assess(menu(40u), completeCapture(), changedSample).accepted(),
        "neighboring monotonic samples with identical UI state were rejected");
    RuntimeSample changedMenu = menu();
    changedMenu.menuBits = fnvxr::shared::RuntimeDialogMenuBit;
    const Decision changedMenuDecision = assess(menu(), completeCapture(), changedMenu);
    require(
        !changedMenuDecision.accepted()
            && changedMenuDecision.failure == Failure::RuntimeStateChanged,
        "changed menu classification survived the capture sandwich");
    RuntimeSample changedCamera = menu();
    changedCamera.cameraActive = true;
    require(
        !assess(menu(), completeCapture(), changedCamera).accepted(),
        "changed camera state survived the capture sandwich");
    RuntimeSample stale = menu();
    stale.fresh = false;
    const Decision staleDecision = assess(stale, completeCapture(), stale);
    require(
        !staleDecision.accepted()
            && staleDecision.failure == Failure::BeforeRuntimeUnavailable,
        "stale runtime evidence admitted a UI capture");
    RuntimeSample zero = menu(0u);
    require(
        !assess(zero, completeCapture(), zero).accepted(),
        "zero runtime identity admitted a UI capture");

    WindowCapture failed = completeCapture();
    failed.complete = false;
    const Decision failedDecision = assess(menu(), failed, menu());
    require(
        !failedDecision.accepted()
            && failedDecision.failure == Failure::CaptureIncomplete,
        "failed window copy admitted a UI capture");
    WindowCapture black = completeCapture();
    black.visibleContent = false;
    const Decision blackDecision = assess(menu(), black, menu());
    require(
        !blackDecision.accepted()
            && blackDecision.failure == Failure::CaptureNotVisible,
        "black window copy admitted a UI capture");
    WindowCapture foreign = completeCapture();
    foreign.retailWindowOwned = false;
    require(
        !assess(menu(), foreign, menu()).accepted(),
        "foreign window pixels were labeled retail-owned");
    WindowCapture wrongSize = completeCapture();
    --wrongSize.pixelCount;
    require(
        !assess(menu(), wrongSize, menu()).accepted(),
        "truncated pixel payload admitted a UI capture");
    WindowCapture noSource = completeCapture();
    noSource.sourceFrame = 0u;
    require(
        !assess(menu(), noSource, menu()).accepted(),
        "zero source-frame identity admitted a UI capture");

    std::cout << "host UI capture sandwich gate passed\n";
    return 0;
}
