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

    fnvxr::d3d9::RetailVrBridgeWin32<4096u> bridge;
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
