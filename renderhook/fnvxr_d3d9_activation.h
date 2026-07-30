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

// The checked-in legacy interposer remains forwarding-only. Changing
// environment values cannot alter this compile-time result; the independently
// bounded visual-trial bridge below does not grant any of these capabilities.
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
// the narrow exact-retail route: Fallout's ordinary D3D9 device, one
// authorized RenderWorldSceneGraph detour, engine-owned eye rendering, a
// bounded CPU readback transport, and a single leased Present slot used to
// finish deferred startup. It never authorizes the retained draw-hook set.
struct RetailVrBridgePolicy
{
    bool compiled = false;
    bool exactCurrentProcessAuthorityRequired = true;
    bool exBackedGameDevice = false;
    bool retailWorldHookOnly = true;
    bool replaceD3D9DeviceVtablePointer = false;
    bool leaseNativePresentSlot = false;
    bool cpuImageTransfer = false;
    bool legacyDrawReplay = false;
};

inline constexpr RetailVrBridgePolicy CompiledRetailVrBridgePolicy {
    true,
    true,
    false,
    true,
    false,
    true,
    true,
    false,
};

// Runtime configuration can request only the bounded CPU visual trial.  It
// cannot widen the compiled bridge policy or authorize the retained D3D9
// replay interposer.
struct RetailVrVisualTrialRequest
{
    bool exactProfileMatched = false;
    bool engineCenterStereoRequested = false;
    bool stereoWorldDisabled = false;
    bool legacyImageDiagnosticsRequested = false;
    bool retainedStereoGameTexturesRequested = false;
    bool unprovenColorOnlyStereoDiagnosticRequested = false;
    bool allowStereoWorld2dFallback = false;
    bool showGamePlaneOnStereoLoss = false;
    bool stereoFallbackMonoFullscreen = false;
};

constexpr bool retailVrVisualTrialAuthorized(
    const RetailVrBridgePolicy& policy,
    const RetailVrVisualTrialRequest& request) noexcept
{
    return policy.compiled
        && policy.exactCurrentProcessAuthorityRequired
        && !policy.exBackedGameDevice
        && policy.retailWorldHookOnly
        && !policy.replaceD3D9DeviceVtablePointer
        && policy.leaseNativePresentSlot
        && policy.cpuImageTransfer
        && !policy.legacyDrawReplay
        && request.exactProfileMatched
        && request.engineCenterStereoRequested
        && !request.stereoWorldDisabled
        && !request.legacyImageDiagnosticsRequested
        && !request.retainedStereoGameTexturesRequested
        && !request.unprovenColorOnlyStereoDiagnosticRequested
        && !request.allowStereoWorld2dFallback
        && !request.showGamePlaneOnStereoLoss
        && !request.stereoFallbackMonoFullscreen;
}

// Physical play uses the same exact-retail, ordinary-D3D9 eye renderer as the
// bounded visual trial. Input remains outside this bridge and is authorized
// independently by the xNVSE current-process capability. Keeping a distinct
// request prevents an interactive launcher flag from silently widening the
// historical visual-trial profile.
struct RetailVrPhysicalPlayRequest
{
    bool exactProfileMatched = false;
    bool physicalHeadsetPlayRequested = false;
    bool engineCenterStereoRequested = false;
    bool stereoWorldDisabled = false;
    bool legacyImageDiagnosticsRequested = false;
    bool retainedStereoGameTexturesRequested = false;
    bool unprovenColorOnlyStereoDiagnosticRequested = false;
    bool allowStereoWorld2dFallback = false;
    bool showGamePlaneOnStereoLoss = false;
    bool stereoFallbackMonoFullscreen = false;
};

constexpr bool retailVrPhysicalPlayAuthorized(
    const RetailVrBridgePolicy& policy,
    const RetailVrPhysicalPlayRequest& request) noexcept
{
    return policy.compiled
        && policy.exactCurrentProcessAuthorityRequired
        && !policy.exBackedGameDevice
        && policy.retailWorldHookOnly
        && !policy.replaceD3D9DeviceVtablePointer
        && policy.leaseNativePresentSlot
        && policy.cpuImageTransfer
        && !policy.legacyDrawReplay
        && request.exactProfileMatched
        && request.physicalHeadsetPlayRequested
        && request.engineCenterStereoRequested
        && !request.stereoWorldDisabled
        && !request.legacyImageDiagnosticsRequested
        && !request.retainedStereoGameTexturesRequested
        && !request.unprovenColorOnlyStereoDiagnosticRequested
        && !request.allowStereoWorld2dFallback
        && !request.showGamePlaneOnStereoLoss
        && !request.stereoFallbackMonoFullscreen;
}

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
static_assert(!CompiledRetailVrBridgePolicy.exBackedGameDevice);
static_assert(CompiledRetailVrBridgePolicy.retailWorldHookOnly);
static_assert(!CompiledRetailVrBridgePolicy.replaceD3D9DeviceVtablePointer);
static_assert(CompiledRetailVrBridgePolicy.leaseNativePresentSlot);
static_assert(CompiledRetailVrBridgePolicy.cpuImageTransfer);
static_assert(!CompiledRetailVrBridgePolicy.legacyDrawReplay);
}
