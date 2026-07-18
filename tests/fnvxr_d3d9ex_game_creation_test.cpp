#include "fnvxr_d3d9ex_game_creation.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}
}

int main()
{
    using fnvxr::d3d9::GameD3D9CreationBackend;
    using fnvxr::d3d9::GameD3D9BootstrapEvidence;
    using fnvxr::d3d9::assessGameD3D9Bootstrap;
    using fnvxr::d3d9::creationBackendRequiresDeviceEx;
    using fnvxr::d3d9::makeGameD3D9ExDisplayModeFields;
    using fnvxr::d3d9::selectGameD3D9CreationBackend;

    constexpr GameD3D9BootstrapEvidence completeBootstrap {
        true,
        true,
        true,
        true,
        true,
        true,
    };
    static_assert(assessGameD3D9Bootstrap(completeBootstrap).authorized());
    for (int missing = 0; missing < 6; ++missing)
    {
        GameD3D9BootstrapEvidence incomplete = completeBootstrap;
        switch (missing)
        {
        case 0: incomplete.executableLeafMatched = false; break;
        case 1: incomplete.loadedExecutableIdentityMatched = false; break;
        case 2: incomplete.win32Process = false; break;
        case 3: incomplete.exCreationAvailable = false; break;
        case 4: incomplete.runtimeMutationDeferredToFullAuthority = false; break;
        case 5: incomplete.uiCaptureDeferredToAuthorizedBridge = false; break;
        default: break;
        }
        if (assessGameD3D9Bootstrap(incomplete).authorized())
            return fail("incomplete D3D bootstrap evidence was authorized");
    }

    if (selectGameD3D9CreationBackend(false, true, true)
        != GameD3D9CreationBackend::LegacyD3D9)
    {
        return fail("fused forwarding path did not preserve legacy D3D9");
    }
    if (selectGameD3D9CreationBackend(true, true, true)
        != GameD3D9CreationBackend::ExBackedD3D9)
    {
        return fail("production path did not select the Ex-backed enumerator");
    }
    if (selectGameD3D9CreationBackend(true, true, false)
        != GameD3D9CreationBackend::Unavailable)
    {
        return fail("production path silently fell back to a non-Ex device");
    }
    if (selectGameD3D9CreationBackend(false, false, true)
        != GameD3D9CreationBackend::Unavailable)
    {
        return fail("forwarding path silently changed legacy API semantics");
    }
    if (!creationBackendRequiresDeviceEx(
            GameD3D9CreationBackend::ExBackedD3D9)
        || creationBackendRequiresDeviceEx(
            GameD3D9CreationBackend::LegacyD3D9))
    {
        return fail("CreateDeviceEx requirement was not exact");
    }

    const auto windowedMode = makeGameD3D9ExDisplayModeFields(
        true,
        24u,
        1920u,
        1080u,
        60u,
        21u,
        0u);
    if (windowedMode.required
        || windowedMode.structureBytes != 0u
        || windowedMode.width != 0u)
    {
        return fail("windowed CreateDeviceEx did not select a null mode");
    }

    const auto fullscreenMode = makeGameD3D9ExDisplayModeFields(
        false,
        24u,
        1920u,
        1080u,
        144u,
        21u,
        0u);
    if (!fullscreenMode.required
        || fullscreenMode.structureBytes != 24u
        || fullscreenMode.width != 1920u
        || fullscreenMode.height != 1080u
        || fullscreenMode.refreshRate != 144u
        || fullscreenMode.format != 21u
        || fullscreenMode.scanLineOrdering != 0u)
    {
        return fail("fullscreen CreateDeviceEx mode did not preserve inputs");
    }

    std::cout << "D3D9Ex game bootstrap/creation policy tests passed\n";
    return EXIT_SUCCESS;
}
