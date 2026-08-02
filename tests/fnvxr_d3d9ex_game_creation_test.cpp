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
        case 3: incomplete.legacyCreationAvailable = false; break;
        case 4: incomplete.runtimeMutationDeferredToFullAuthority = false; break;
        case 5: incomplete.uiCaptureDeferredToAuthorizedBridge = false; break;
        default: break;
        }
        if (assessGameD3D9Bootstrap(incomplete).authorized())
            return fail("incomplete D3D bootstrap evidence was authorized");
    }

    if (selectGameD3D9CreationBackend(true)
        != GameD3D9CreationBackend::LegacyD3D9)
    {
        return fail("fused forwarding path did not preserve legacy D3D9");
    }
    if (selectGameD3D9CreationBackend(false)
        != GameD3D9CreationBackend::Unavailable)
    {
        return fail("forwarding path silently changed legacy API semantics");
    }
    std::cout << "ordinary D3D9 game bootstrap/creation policy tests passed\n";
    return EXIT_SUCCESS;
}
