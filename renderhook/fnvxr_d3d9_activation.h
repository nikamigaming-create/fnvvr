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

// This is separate from the retired replay interposer above. It describes
// the narrow exact-retail route: an Ex-backed enumerator, one authorized
// RenderWorldSceneGraph detour, engine-owned eye rendering, and GPU-only color
// transport. It never authorizes the retained D3D9 device-vtable hook set.
struct RetailVrBridgePolicy
{
    bool compiled = false;
    bool exactCurrentProcessAuthorityRequired = true;
    bool exBackedGameDevice = true;
    bool retailWorldHookOnly = true;
    bool patchD3D9DeviceVtable = false;
    bool cpuImageTransfer = false;
    bool legacyDrawReplay = false;
};

inline constexpr RetailVrBridgePolicy CompiledRetailVrBridgePolicy {
    true,
    true,
    true,
    true,
    false,
    false,
    false,
};

static_assert(CompiledInterpositionPolicy.forwardSystemExports);
static_assert(!CompiledInterpositionPolicy.wrapDirect3D9);
static_assert(!CompiledInterpositionPolicy.patchDirect3D9Vtable);
static_assert(!CompiledInterpositionPolicy.patchDeviceVtable);
static_assert(!CompiledInterpositionPolicy.accessSharedMappings);
static_assert(!CompiledInterpositionPolicy.captureOrCpuReadback);
static_assert(!CompiledInterpositionPolicy.perFrameLogging);
static_assert(CompiledRetailVrBridgePolicy.compiled);
static_assert(
    CompiledRetailVrBridgePolicy.exactCurrentProcessAuthorityRequired);
static_assert(CompiledRetailVrBridgePolicy.exBackedGameDevice);
static_assert(CompiledRetailVrBridgePolicy.retailWorldHookOnly);
static_assert(!CompiledRetailVrBridgePolicy.patchD3D9DeviceVtable);
static_assert(!CompiledRetailVrBridgePolicy.cpuImageTransfer);
static_assert(!CompiledRetailVrBridgePolicy.legacyDrawReplay);
}
