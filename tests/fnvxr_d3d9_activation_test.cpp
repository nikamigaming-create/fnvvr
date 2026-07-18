#include "fnvxr_d3d9_activation.h"

#include <array>
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
    using fnvxr::d3d9::CompiledInterpositionPolicy;
    using fnvxr::d3d9::CompiledProductionRendererProof;
    using fnvxr::d3d9::CompiledRetailVrBridgePolicy;
    using fnvxr::d3d9::ProductionRendererAuthorized;
    using fnvxr::d3d9::ProductionRendererProof;
    using fnvxr::d3d9::interpositionPolicy;
    using fnvxr::d3d9::productionRendererAuthorized;

    if (ProductionRendererAuthorized
        || CompiledProductionRendererProof.exactRetailExecutableMatched
        || CompiledProductionRendererProof.exactRendererAbiMatched
        || CompiledProductionRendererProof.engineStereoTransactionIntegrated
        || CompiledProductionRendererProof.nativeGpuTransportIntegrated
        || CompiledProductionRendererProof.productPresentationControllerIntegrated
        || CompiledProductionRendererProof.retainedD3D9HookSetAudited)
    {
        return fail("the checked-in D3D9 production proof must be entirely false");
    }

    if (!CompiledInterpositionPolicy.forwardSystemExports
        || CompiledInterpositionPolicy.wrapDirect3D9
        || CompiledInterpositionPolicy.patchDirect3D9Vtable
        || CompiledInterpositionPolicy.patchDeviceVtable
        || CompiledInterpositionPolicy.accessSharedMappings
        || CompiledInterpositionPolicy.captureOrCpuReadback
        || CompiledInterpositionPolicy.perFrameLogging)
    {
        return fail("the fused D3D9 policy must only forward system exports");
    }

    if (!CompiledRetailVrBridgePolicy.compiled
        || !CompiledRetailVrBridgePolicy.exactCurrentProcessAuthorityRequired
        || !CompiledRetailVrBridgePolicy.exBackedGameDevice
        || !CompiledRetailVrBridgePolicy.retailWorldHookOnly
        || CompiledRetailVrBridgePolicy.patchD3D9DeviceVtable
        || CompiledRetailVrBridgePolicy.cpuImageTransfer
        || CompiledRetailVrBridgePolicy.legacyDrawReplay)
    {
        return fail("the isolated retail bridge policy widened into legacy hooks");
    }

    ProductionRendererProof complete {
        true,
        true,
        true,
        true,
        true,
        true,
    };
    if (!productionRendererAuthorized(complete))
        return fail("complete synthetic production evidence must authorize the future path");

    const std::array<ProductionRendererProof, 6> incomplete {{
        { false, true, true, true, true, true },
        { true, false, true, true, true, true },
        { true, true, false, true, true, true },
        { true, true, true, false, true, true },
        { true, true, true, true, false, true },
        { true, true, true, true, true, false },
    }};
    for (const auto& proof : incomplete)
    {
        if (productionRendererAuthorized(proof))
            return fail("one missing production fact must fail closed");
    }

    const auto authorizedPolicy = interpositionPolicy(true);
    if (!authorizedPolicy.forwardSystemExports
        || !authorizedPolicy.wrapDirect3D9
        || authorizedPolicy.patchDirect3D9Vtable
        || !authorizedPolicy.patchDeviceVtable
        || !authorizedPolicy.accessSharedMappings
        || !authorizedPolicy.captureOrCpuReadback
        || !authorizedPolicy.perFrameLogging)
    {
        return fail("the pure policy must expose every production side effect explicitly");
    }

    std::cout << "fnvxr D3D9 activation contract PASS\n";
    return EXIT_SUCCESS;
}
