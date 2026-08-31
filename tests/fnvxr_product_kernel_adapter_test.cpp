#include "../host/fnvxr_product_kernel_adapter.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

fnvxr::host::PresentationTransportProof transport(std::uint64_t transaction)
{
    return { 7, transaction, true, true, true, true, true };
}

fnvxr::product::PresentationInput world(std::uint64_t identity)
{
    fnvxr::product::PresentationInput input {};
    input.runtimeStateSample = 5;
    input.runtimePhase = fnvxr::shared::RuntimePhaseGameplay;
    input.runtimeFresh = true;
    input.cameraActive = true;
    input.stereo = {
        identity, identity, identity, 5,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true };
    return input;
}

fnvxr::product::PresentationInput ui(std::uint64_t identity)
{
    fnvxr::product::PresentationInput input {};
    input.runtimeStateSample = 6;
    input.runtimePhase = fnvxr::shared::RuntimePhaseMenu;
    input.menuBits = fnvxr::shared::RuntimeGenericMenuBit;
    input.runtimeFresh = true;
    input.ui = { identity, 6, true, true, true };
    return input;
}
}

int main()
{
    const auto validated = fnvxr::kernel::validateRuntimeConfig({});
    if (!validated || !validated.config)
        return fail("test config did not validate");
    fnvxr::host::ProductKernelAdapter adapter(*validated.config);

    const auto worldRuntime =
        fnvxr::host::ProductKernelAdapter::runtimeSnapshot(world(1));
    if (worldRuntime.sample != 5
        || worldRuntime.phase
            != fnvxr::kernel::presentation::RuntimePhase::Running
        || worldRuntime.ui
            != fnvxr::kernel::presentation::UiClassification::None
        || !worldRuntime.fresh)
    {
        return fail("runtime authority and ProductKernel input mapping diverged");
    }
    auto pipBoyRuntimeInput = world(2);
    pipBoyRuntimeInput.runtimePhase = fnvxr::shared::RuntimePhaseMenu;
    pipBoyRuntimeInput.menuBits = fnvxr::shared::RuntimePipBoyMenuBit;
    const auto pipBoyRuntime =
        fnvxr::host::ProductKernelAdapter::runtimeSnapshot(
            pipBoyRuntimeInput);
    if (pipBoyRuntime.phase
            != fnvxr::kernel::presentation::RuntimePhase::Running
        || pipBoyRuntime.ui
            != fnvxr::kernel::presentation::UiClassification::PipBoySpatial)
    {
        return fail("spatial Pip-Boy runtime mapping diverged");
    }

    const auto uiDecision = adapter.advance(ui(10), transport(10), false, false);
    if (uiDecision.mode != fnvxr::product::PresentationMode::UiQuad
        || !uiDecision.pointerEnabled
        || uiDecision.presentedUiSourceFrame != 10
        || uiDecision.presentedSourceEpoch != 7
        || uiDecision.presentedSourceFrame != 10
        || uiDecision.presentedSourceTransaction != 10)
    {
        return fail("confirmed retail UI did not cross the kernel adapter");
    }
    const auto held = adapter.advance(world(9), transport(9), true, false);
    if (held.mode != fnvxr::product::PresentationMode::UiQuad
        || !held.transitionHold)
    {
        return fail("older world did not retain the bounded UI hold");
    }
    const auto worldDecision = adapter.advance(world(11), transport(11), true, false);
    if (worldDecision.mode != fnvxr::product::PresentationMode::WorldStereo
        || !worldDecision.gameplayVrAccepted
        || !worldDecision.spatialRigMayRender
        || worldDecision.wristScreenMayRender
        || worldDecision.presentedSourceEpoch != 7
        || worldDecision.presentedSourceFrame != 11
        || worldDecision.presentedSourceTransaction != 11)
    {
        return fail("complete newer world did not cross the kernel adapter");
    }
    const auto replay = adapter.advance(world(11), transport(11), true, false);
    if (replay.mode != fnvxr::product::PresentationMode::SafetyBlank)
        return fail("replayed world identity crossed the kernel adapter");

    adapter.reset();
    auto pipBoy = world(12);
    pipBoy.runtimePhase = fnvxr::shared::RuntimePhaseMenu;
    pipBoy.menuBits = fnvxr::shared::RuntimePipBoyMenuBit;
    const auto pipBoyDecision = adapter.advance(
        pipBoy, transport(12), true, true);
    if (pipBoyDecision.mode != fnvxr::product::PresentationMode::WorldStereo
        || !pipBoyDecision.spatialRigMayRender
        || !pipBoyDecision.wristScreenMayRender)
        return fail("kernel wrist authorization was lost at the host adapter boundary");
    return EXIT_SUCCESS;
}
