#pragma once

namespace fnvxr::d3d9
{
// Every field represents production evidence, not a launcher/configuration
// preference.  The retained D3D9 interposer is transparent unless all fields
// are true in the compiled artifact.
struct ProductionRendererProof
{
    bool exactRetailExecutableMatched = false;
    bool exactRendererAbiMatched = false;
    bool engineStereoTransactionIntegrated = false;
    bool nativeGpuTransportIntegrated = false;
    bool productPresentationControllerIntegrated = false;
    bool retainedD3D9HookSetAudited = false;
};

constexpr bool productionRendererAuthorized(const ProductionRendererProof& proof)
{
    return proof.exactRetailExecutableMatched
        && proof.exactRendererAbiMatched
        && proof.engineStereoTransactionIntegrated
        && proof.nativeGpuTransportIntegrated
        && proof.productPresentationControllerIntegrated
        && proof.retainedD3D9HookSetAudited;
}

// Checked-in builds are forwarding shims only.  Changing environment values
// cannot alter this compile-time result.
inline constexpr ProductionRendererProof CompiledProductionRendererProof {};
inline constexpr bool ProductionRendererAuthorized =
    productionRendererAuthorized(CompiledProductionRendererProof);

struct InterpositionPolicy
{
    bool forwardSystemExports = true;
    bool wrapDirect3D9 = false;
    bool patchDirect3D9Vtable = false;
    bool patchDeviceVtable = false;
    bool accessSharedMappings = false;
    bool captureOrCpuReadback = false;
    bool perFrameLogging = false;
};

constexpr InterpositionPolicy interpositionPolicy(bool productionAuthorized)
{
    return {
        true,
        productionAuthorized,
        false,
        productionAuthorized,
        productionAuthorized,
        productionAuthorized,
        productionAuthorized,
    };
}

inline constexpr InterpositionPolicy CompiledInterpositionPolicy =
    interpositionPolicy(ProductionRendererAuthorized);

static_assert(CompiledInterpositionPolicy.forwardSystemExports);
static_assert(!CompiledInterpositionPolicy.wrapDirect3D9);
static_assert(!CompiledInterpositionPolicy.patchDirect3D9Vtable);
static_assert(!CompiledInterpositionPolicy.patchDeviceVtable);
static_assert(!CompiledInterpositionPolicy.accessSharedMappings);
static_assert(!CompiledInterpositionPolicy.captureOrCpuReadback);
static_assert(!CompiledInterpositionPolicy.perFrameLogging);
}
