#include "fnvxr_retail_vr_bridge_win32.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool snapshotEyeTargets(void*) noexcept { return false; }
bool bindEyeTarget(
    void*,
    fnvxr::engine::CenterRendererEye,
    fnvxr::engine::CenterRendererEyeIsolation&) noexcept
{
    return false;
}
bool endEyeTarget(
    void*,
    fnvxr::engine::CenterRendererEye,
    fnvxr::engine::CenterRendererEyeIsolation&) noexcept
{
    return false;
}
void rollbackEyeTarget(
    void*,
    fnvxr::engine::CenterRendererEye,
    fnvxr::engine::CenterRendererEyeIsolation&) noexcept
{
}
bool restoreEyeTargets(void*) noexcept { return false; }
bool prepareCameraFrame(
    void*,
    const fnvxr::engine::RetailWorldAccumulationCallFrame&,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t,
    fnvxr::engine::RetailCenterRuntimeFrame&) noexcept
{
    return false;
}
bool armAccumulationCallRelay(void*, std::uintptr_t) noexcept
{
    return true;
}
void disarmAccumulationCallRelay(void*) noexcept
{
}
bool armRenderPhaseCallRelays(
    void*,
    std::uintptr_t,
    std::uintptr_t) noexcept
{
    return true;
}
void disarmRenderPhaseCallRelays(void*) noexcept
{
}
bool publishCpuPair(
    void*,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t) noexcept
{
    return false;
}
bool publishCpuMonoUiQuad(
    void*,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t) noexcept
{
    return false;
}
bool prepareColorProducer(void*, std::uint64_t) noexcept
{
    return false;
}
fnvxr::d3d9::color_transport::ProducerPublication produceColorPair(
    void*,
    const fnvxr::d3d9::color_transport::ProducerFrameIdentity&) noexcept
{
    return {};
}
}

int main()
{
    fnvxr::d3d9::RetailVrPrivateRenderDispatchGate privateRenderGate;
    require(
        !privateRenderGate.active()
            && privateRenderGate.tryEnter()
            && privateRenderGate.active()
            && !privateRenderGate.tryEnter(),
        "private-render dispatch gate admitted a nested bridge transaction");
    privateRenderGate.leave();
    require(
        !privateRenderGate.active() && privateRenderGate.tryEnter(),
        "private-render dispatch gate did not reopen after the outer transaction");
    privateRenderGate.leave();

    fnvxr::d3d9::RetailV5PublicationSequence publications;
    std::uint64_t firstWorld = 0u;
    std::uint64_t ui = 0u;
    std::uint64_t secondWorld = 0u;
    require(
        publications.claim(
            fnvxr::gpu::color_v5::PresentationMode::BinocularWorld,
            firstWorld)
            && publications.claim(
                fnvxr::gpu::color_v5::PresentationMode::MonoUiQuad,
                ui)
            && publications.claim(
                fnvxr::gpu::color_v5::PresentationMode::BinocularWorld,
                secondWorld)
            && firstWorld == 1u
            && ui == 2u
            && secondWorld == 3u,
        "world/UI/world v5 transaction identities regressed");
    std::uint64_t invalid = 99u;
    require(
        !publications.claim(
            fnvxr::gpu::color_v5::PresentationMode::Unknown,
            invalid)
            && invalid == 0u,
        "unknown presentation mode consumed a publication identity");

    fnvxr::d3d9::RetailVrBridgeOperations cpuOperations {};
    cpuOperations.context = &cpuOperations;
    cpuOperations.eyeTargets = {
        &cpuOperations,
        &snapshotEyeTargets,
        &bindEyeTarget,
        &endEyeTarget,
        &rollbackEyeTarget,
        &restoreEyeTargets,
    };
    cpuOperations.armAccumulationCallRelay = &armAccumulationCallRelay;
    cpuOperations.disarmAccumulationCallRelay = &disarmAccumulationCallRelay;
    cpuOperations.armRenderPhaseCallRelays = &armRenderPhaseCallRelays;
    cpuOperations.disarmRenderPhaseCallRelays =
        &disarmRenderPhaseCallRelays;
    cpuOperations.prepareDistinctCameraFrame = &prepareCameraFrame;
    cpuOperations.publicationTransport =
        fnvxr::d3d9::RetailVrPublicationTransport::CpuReadback;
    cpuOperations.publishCpuPair = &publishCpuPair;
    cpuOperations.publishCpuMonoUiQuad = &publishCpuMonoUiQuad;
    require(
        fnvxr::d3d9::retailVrBridgeOperationsComplete(cpuOperations),
        "complete ordinary-D3D9 CPU publication operations were rejected");
    cpuOperations.publishCpuMonoUiQuad = nullptr;
    require(
        !fnvxr::d3d9::retailVrBridgeOperationsComplete(cpuOperations),
        "CPU bridge accepted a world publisher without its mono UI publisher");
    cpuOperations.publishCpuMonoUiQuad = &publishCpuMonoUiQuad;
    cpuOperations.publishCpuPair = nullptr;
    require(
        !fnvxr::d3d9::retailVrBridgeOperationsComplete(cpuOperations),
        "bridge accepted neither a CPU publisher nor a complete GPU publisher");

    fnvxr::d3d9::RetailVrBridgeOperations gpuOperations {};
    gpuOperations.context = &gpuOperations;
    gpuOperations.eyeTargets = {
        &gpuOperations,
        &snapshotEyeTargets,
        &bindEyeTarget,
        &endEyeTarget,
        &rollbackEyeTarget,
        &restoreEyeTargets,
    };
    gpuOperations.armAccumulationCallRelay = &armAccumulationCallRelay;
    gpuOperations.disarmAccumulationCallRelay = &disarmAccumulationCallRelay;
    gpuOperations.armRenderPhaseCallRelays = &armRenderPhaseCallRelays;
    gpuOperations.disarmRenderPhaseCallRelays =
        &disarmRenderPhaseCallRelays;
    gpuOperations.prepareDistinctCameraFrame = &prepareCameraFrame;
    gpuOperations.publicationTransport =
        fnvxr::d3d9::RetailVrPublicationTransport::GpuSharedTextures;
    gpuOperations.prepareColorProducer = &prepareColorProducer;
    gpuOperations.produceColorPair = &produceColorPair;
    require(
        fnvxr::d3d9::retailVrBridgeOperationsComplete(gpuOperations),
        "complete GPU-shared-texture publication operations were rejected");

    gpuOperations.publishCpuPair = &publishCpuPair;
    require(
        !fnvxr::d3d9::retailVrBridgeOperationsComplete(gpuOperations),
        "GPU bridge accepted a hidden CPU fallback");
    gpuOperations.publishCpuPair = nullptr;
    gpuOperations.publishCpuMonoUiQuad = &publishCpuMonoUiQuad;
    require(
        !fnvxr::d3d9::retailVrBridgeOperationsComplete(gpuOperations),
        "GPU bridge accepted a hidden CPU UI fallback");
    gpuOperations.publishCpuMonoUiQuad = nullptr;
    gpuOperations.publicationTransport =
        fnvxr::d3d9::RetailVrPublicationTransport::CpuReadback;
    require(
        !fnvxr::d3d9::retailVrBridgeOperationsComplete(gpuOperations),
        "CPU bridge accepted GPU callbacks instead of an explicit CPU route");

    fnvxr::d3d9::RetailVrBridgeWin32<4096u> bridge;
    const auto initialDiagnostics = bridge.frameDiagnostics();
    require(
        initialDiagnostics.dispatchCount == 0u
            && initialDiagnostics.stereoCompleteCount == 0u
            && !initialDiagnostics.eyeCamera.captured,
        "bridge diagnostics did not start empty");
    require(
        !bridge.initialize({}, 0u, 0u, 0u),
        "empty bridge operations unexpectedly initialized");
#if defined(_WIN32) && defined(_M_IX86)
    require(
        bridge.failure()
            == fnvxr::d3d9::RetailVrBridgeFailure::OperationsIncomplete,
        "Win32 empty bridge did not reject its operation table");
#else
    require(
        bridge.failure()
            == fnvxr::d3d9::RetailVrBridgeFailure::UnsupportedArchitecture,
        "non-x86 bridge did not fail before production authority");
#endif
    std::cout << "retail VR bridge architecture fuse passed\n";
    return EXIT_SUCCESS;
}
