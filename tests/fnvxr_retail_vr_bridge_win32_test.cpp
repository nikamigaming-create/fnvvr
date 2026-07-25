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
    const fnvxr::engine::RetailWorldHookDispatchFrame&,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t,
    fnvxr::engine::RetailCenterRuntimeFrame&) noexcept
{
    return false;
}
bool publishCpuPair(
    void*,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t) noexcept
{
    return false;
}
}

int main()
{
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
    cpuOperations.prepareDistinctCameraFrame = &prepareCameraFrame;
    cpuOperations.publishCpuPair = &publishCpuPair;
    require(
        fnvxr::d3d9::retailVrBridgeOperationsComplete(cpuOperations),
        "complete ordinary-D3D9 CPU publication operations were rejected");
    cpuOperations.publishCpuPair = nullptr;
    require(
        !fnvxr::d3d9::retailVrBridgeOperationsComplete(cpuOperations),
        "bridge accepted neither a CPU publisher nor a complete GPU publisher");

    fnvxr::d3d9::RetailVrBridgeWin32<4096u> bridge;
    const auto initialDiagnostics = bridge.frameDiagnostics();
    require(
        initialDiagnostics.dispatchCount == 0u
            && initialDiagnostics.stereoCompleteCount == 0u,
        "bridge diagnostics did not start empty");
    require(
        !bridge.initialize({}, 0u),
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
