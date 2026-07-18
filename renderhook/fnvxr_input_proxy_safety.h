#pragma once

namespace fnvxr::input_proxy
{
// Input remapping changes retail control semantics and therefore requires the
// same standard of proof as an engine hook. This is a source fuse, not a
// configuration default; environment variables and shared memory cannot
// override it.
inline constexpr bool ProductionInputMutationProofComplete = false;

// A second source fuse prevents an old input experiment from becoming the
// product controller merely because its historical proof bit is changed.
inline constexpr bool ProductInputControllerIntegrated = false;

struct ProductionInputAuthorizationEvidence
{
    bool exactRetailExecutable = false;
    bool exactCompatibilityModuleInventory = false;
    bool authoritativeInputProducer = false;
    bool sharedProtocolMatched = false;
    bool uiModeTransitionsComplete = false;
};

constexpr bool assessProductionInputAuthorization(
    bool proofComplete,
    bool productControllerIntegrated,
    bool requested,
    const ProductionInputAuthorizationEvidence& evidence) noexcept
{
    return proofComplete
        && productControllerIntegrated
        && requested
        && evidence.exactRetailExecutable
        && evidence.exactCompatibilityModuleInventory
        && evidence.authoritativeInputProducer
        && evidence.sharedProtocolMatched
        && evidence.uiModeTransitionsComplete;
}

constexpr bool productionInputMutationAuthorized(
    bool requested,
    const ProductionInputAuthorizationEvidence& evidence) noexcept
{
    return assessProductionInputAuthorization(
        ProductionInputMutationProofComplete,
        ProductInputControllerIntegrated,
        requested,
        evidence);
}

// Neither proxy has a synchronous current-process authorization validator yet.
// Keeping that absence explicit makes both shipping DLLs transparent even if
// configuration or stale shared mappings request the retired input path.
inline constexpr ProductionInputAuthorizationEvidence
    UnverifiedProductionInputAuthorization {};

constexpr bool productionInputMutationAuthorizedForCurrentBuild() noexcept
{
    return productionInputMutationAuthorized(
        true,
        UnverifiedProductionInputAuthorization);
}

static_assert(
    !productionInputMutationAuthorizedForCurrentBuild(),
    "retail input mutation must remain source-fused without exact authorization");
}
