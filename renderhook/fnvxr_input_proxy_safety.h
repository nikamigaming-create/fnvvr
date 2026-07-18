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

enum class ProductionInputAuthorizationFailure
{
    None,
    ProductionProofIncomplete,
    ProductControllerNotIntegrated,
    NotRequested,
    ExactRetailExecutableUnverified,
    CompatibilityInventoryUnverified,
    InputProducerUnauthenticated,
    ProducerProcessIdentityUnbound,
    ProducerEpochUnavailable,
    SharedProtocolUnmatched,
    RuntimeStateSampleUnmatched,
    UiModeTransitionsIncomplete,
    ConfigurationFreeMappingIncomplete,
    ExclusiveInputOwnershipUnproven,
    StaleProducerNeutralizationIncomplete,
};

struct ProductionInputAuthorizationEvidence
{
    bool exactRetailExecutable = false;
    bool exactCompatibilityModuleInventory = false;
    bool authoritativeInputProducer = false;
    bool producerProcessIdentityBound = false;
    bool producerEpochCurrent = false;
    bool sharedProtocolMatched = false;
    bool runtimeStateSampleMatched = false;
    bool uiModeTransitionsComplete = false;
    bool configurationFreeProductMapping = false;
    bool exclusiveRetailInputOwnership = false;
    bool staleProducerNeutralizationComplete = false;
};

struct ProductionInputAuthorizationDecision
{
    ProductionInputAuthorizationFailure failure =
        ProductionInputAuthorizationFailure::ProductionProofIncomplete;

    constexpr bool complete() const noexcept
    {
        return failure == ProductionInputAuthorizationFailure::None;
    }
};

constexpr ProductionInputAuthorizationDecision
assessProductionInputAuthorizationDecision(
    bool proofComplete,
    bool productControllerIntegrated,
    bool requested,
    const ProductionInputAuthorizationEvidence& evidence) noexcept
{
    if (!proofComplete)
    {
        return {
            ProductionInputAuthorizationFailure::ProductionProofIncomplete,
        };
    }
    if (!productControllerIntegrated)
    {
        return {
            ProductionInputAuthorizationFailure::ProductControllerNotIntegrated,
        };
    }
    if (!requested)
        return { ProductionInputAuthorizationFailure::NotRequested };
    if (!evidence.exactRetailExecutable)
    {
        return {
            ProductionInputAuthorizationFailure::ExactRetailExecutableUnverified,
        };
    }
    if (!evidence.exactCompatibilityModuleInventory)
    {
        return {
            ProductionInputAuthorizationFailure::CompatibilityInventoryUnverified,
        };
    }
    if (!evidence.authoritativeInputProducer)
    {
        return {
            ProductionInputAuthorizationFailure::InputProducerUnauthenticated,
        };
    }
    if (!evidence.producerProcessIdentityBound)
    {
        return {
            ProductionInputAuthorizationFailure::ProducerProcessIdentityUnbound,
        };
    }
    if (!evidence.producerEpochCurrent)
    {
        return {
            ProductionInputAuthorizationFailure::ProducerEpochUnavailable,
        };
    }
    if (!evidence.sharedProtocolMatched)
    {
        return {
            ProductionInputAuthorizationFailure::SharedProtocolUnmatched,
        };
    }
    if (!evidence.runtimeStateSampleMatched)
    {
        return {
            ProductionInputAuthorizationFailure::RuntimeStateSampleUnmatched,
        };
    }
    if (!evidence.uiModeTransitionsComplete)
    {
        return {
            ProductionInputAuthorizationFailure::UiModeTransitionsIncomplete,
        };
    }
    if (!evidence.configurationFreeProductMapping)
    {
        return {
            ProductionInputAuthorizationFailure::ConfigurationFreeMappingIncomplete,
        };
    }
    if (!evidence.exclusiveRetailInputOwnership)
    {
        return {
            ProductionInputAuthorizationFailure::ExclusiveInputOwnershipUnproven,
        };
    }
    if (!evidence.staleProducerNeutralizationComplete)
    {
        return {
            ProductionInputAuthorizationFailure::StaleProducerNeutralizationIncomplete,
        };
    }
    return { ProductionInputAuthorizationFailure::None };
}

constexpr bool assessProductionInputAuthorization(
    bool proofComplete,
    bool productControllerIntegrated,
    bool requested,
    const ProductionInputAuthorizationEvidence& evidence) noexcept
{
    return assessProductionInputAuthorizationDecision(
        proofComplete,
        productControllerIntegrated,
        requested,
        evidence).complete();
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
