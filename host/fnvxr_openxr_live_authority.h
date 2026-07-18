#pragma once

#include <cstdint>

namespace fnvxr::host
{
// This is the only frame bound authorized by the product host. The command
// line parser must enforce a positive value no greater than this before the
// OpenXR loader can be touched.
inline constexpr std::uint64_t MaximumProductHostFrames = 7200u;

enum class ProductOpenXrInitializationFailure : std::uint8_t
{
    None,
    FrameBoundMismatch,
    FiniteFrameArgumentNotEnforced,
    ProductPresentationRouteNotIntegrated,
    V5GpuConsumerNotFailClosed,
    SourcePoseLineageNotRequired,
    SourceRuntimeLineageNotRequired,
    ContentAttestedSupervisorNotIntegrated,
    HostFirstOrderingNotRequired,
    RetailReadinessNotRequired,
};

// These are source/build integration facts. They authorize attempting OpenXR
// initialization; they do not assert that a live headset, a retail image, a
// controller, or a tracked weapon has passed the product acceptance decision.
//
// The supervisor attestation is content-bound and host-first, but it is local
// build/source authority rather than cryptographic proof of process identity.
struct ProductOpenXrInitializationProof
{
    std::uint64_t maximumFrameCount = 0u;
    bool finiteFrameArgumentEnforcedBeforeLoader = false;
    bool productPresentationRouteIntegrated = false;
    bool v5GpuConsumerFailsClosed = false;
    bool exactSourcePoseLineageRequired = false;
    bool stableSourceRuntimeLineageRequired = false;
    bool contentAttestedSupervisorIntegrated = false;
    bool hostReadyBeforeRetailStagingRequired = false;
    bool exactRetailRuntimeReadinessRequired = false;
};

struct ProductOpenXrInitializationAuthorization
{
    ProductOpenXrInitializationFailure failure =
        ProductOpenXrInitializationFailure::FrameBoundMismatch;

    constexpr bool authorized() const noexcept
    {
        return failure == ProductOpenXrInitializationFailure::None;
    }
};

constexpr ProductOpenXrInitializationAuthorization
assessProductOpenXrInitialization(
    const ProductOpenXrInitializationProof& proof) noexcept
{
    if (proof.maximumFrameCount != MaximumProductHostFrames)
    {
        return {
            ProductOpenXrInitializationFailure::FrameBoundMismatch,
        };
    }
    if (!proof.finiteFrameArgumentEnforcedBeforeLoader)
    {
        return {
            ProductOpenXrInitializationFailure::FiniteFrameArgumentNotEnforced,
        };
    }
    if (!proof.productPresentationRouteIntegrated)
    {
        return {
            ProductOpenXrInitializationFailure::ProductPresentationRouteNotIntegrated,
        };
    }
    if (!proof.v5GpuConsumerFailsClosed)
    {
        return {
            ProductOpenXrInitializationFailure::V5GpuConsumerNotFailClosed,
        };
    }
    if (!proof.exactSourcePoseLineageRequired)
    {
        return {
            ProductOpenXrInitializationFailure::SourcePoseLineageNotRequired,
        };
    }
    if (!proof.stableSourceRuntimeLineageRequired)
    {
        return {
            ProductOpenXrInitializationFailure::SourceRuntimeLineageNotRequired,
        };
    }
    if (!proof.contentAttestedSupervisorIntegrated)
    {
        return {
            ProductOpenXrInitializationFailure::ContentAttestedSupervisorNotIntegrated,
        };
    }
    if (!proof.hostReadyBeforeRetailStagingRequired)
    {
        return {
            ProductOpenXrInitializationFailure::HostFirstOrderingNotRequired,
        };
    }
    if (!proof.exactRetailRuntimeReadinessRequired)
    {
        return {
            ProductOpenXrInitializationFailure::RetailReadinessNotRequired,
        };
    }
    return { ProductOpenXrInitializationFailure::None };
}

inline constexpr ProductOpenXrInitializationProof
    CompiledProductOpenXrInitializationProof {
        MaximumProductHostFrames,
        true, // finite frame argument is parsed before loader access
        true, // PresentationController owns the final product mode selection
        true, // v5 GPU consumer rejection projects to SafetyBlank
        true, // world admission requires exact source-pose history
        true, // world/UI admission requires stable source-runtime history
        true, // clean product supervisor requires content build attestation
        true, // supervisor waits for host pose before staging retail artifacts
        true, // supervisor accepts only advancing retail runtime plus pose
    };

inline constexpr ProductOpenXrInitializationAuthorization
    CompiledProductOpenXrInitializationAuthorization =
        assessProductOpenXrInitialization(
            CompiledProductOpenXrInitializationProof);

static_assert(CompiledProductOpenXrInitializationAuthorization.authorized());
}
