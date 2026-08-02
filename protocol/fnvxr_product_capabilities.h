#pragma once

#include <cstdint>

namespace fnvxr::product
{
// These leases are deliberately narrower than the final release fuse.  A
// capability can be evaluated for an isolated diagnostic or implementation
// milestone without silently granting every other retail mutation.
enum class Capability : std::uint8_t
{
    CameraStereoVisual = 0u,
    ControllerInput = 1u,
    UiInteraction = 2u,
    VisualRig = 3u,
    CombatAim = 4u,
    GpuPresentation = 5u,
    FullProduct = 6u,
};

enum class InputOwner : std::uint8_t
{
    None = 0u,
    NvseMainGameLoop = 1u,
    DirectInputProxy = 2u,
    XInputProxy = 3u,
};

// The host publishes intent and the xNVSE main-game-loop consumer is the sole
// selected product-side owner.  Both legacy proxy paths stay transparent in a
// product build unless this explicit decision is reviewed and changed.
inline constexpr InputOwner SelectedProductInputOwner =
    InputOwner::NvseMainGameLoop;

constexpr bool inputOwnerIsProxy(InputOwner owner) noexcept
{
    return owner == InputOwner::DirectInputProxy
        || owner == InputOwner::XInputProxy;
}

constexpr bool capabilityRequiresInputOwner(Capability capability) noexcept
{
    return capability == Capability::ControllerInput
        || capability == Capability::UiInteraction;
}

// The booleans below record a synchronous validation result. These identities
// bind that result to one retail process/image/compatibility state and one
// producer/pose/runtime/mode lineage, so separate valid observations cannot be
// accidentally combined into one lease.
struct CapabilityLeaseLineage
{
    std::uint32_t retailProcessId = 0u;
    std::uint64_t retailImageGeneration = 0u;
    std::uint64_t compatibilityInventoryGeneration = 0u;
    std::uint32_t producerProcessId = 0u;
    std::uint64_t producerEpoch = 0u;
    std::uint64_t poseSequence = 0u;
    std::uint64_t runtimeStateSample = 0u;
    std::uint64_t productModeGeneration = 0u;

    constexpr bool complete() const noexcept
    {
        return retailProcessId != 0u
            && retailImageGeneration != 0u
            && compatibilityInventoryGeneration != 0u
            && producerProcessId != 0u
            && producerEpoch != 0u
            && poseSequence != 0u
            && runtimeStateSample != 0u
            && productModeGeneration != 0u;
    }
};

// The data must be sampled at the point of mutation.  In particular, a
// retained startup probe is not a substitute for current image, function,
// ancestry, compatibility, producer, pose, runtime, and mode lineage.
struct CapabilityLeaseEvidence
{
    bool exactLoadedRetailImageCurrent = false;
    bool requiredFunctionBytesCurrent = false;
    bool requiredObjectAncestryCurrent = false;
    bool compatibilityInventoryCurrent = false;
    bool producerProcessCurrent = false;
    bool producerEpochCurrent = false;
    bool poseLineageCurrent = false;
    bool runtimeStateLineageCurrent = false;
    bool productModeCurrent = false;
    CapabilityLeaseLineage lineage {};
    InputOwner inputOwner = InputOwner::None;
    bool exclusiveInputOwnerCurrent = false;
    bool capabilitySpecificProofComplete = false;
};

enum class CapabilityLeaseFailure : std::uint8_t
{
    None = 0u,
    FullProductMustAggregate,
    NotRequested,
    ExactLoadedRetailImageStale,
    RequiredFunctionBytesStale,
    RequiredObjectAncestryStale,
    CompatibilityInventoryStale,
    ProducerProcessStale,
    ProducerEpochStale,
    PoseLineageStale,
    RuntimeStateLineageStale,
    ProductModeStale,
    LineageIdentityMissing,
    InputOwnerMismatch,
    InputOwnerNotExclusive,
    CapabilitySpecificProofIncomplete,
    ReleaseQualificationIncomplete,
};

struct CapabilityLeaseDecision
{
    Capability capability = Capability::FullProduct;
    CapabilityLeaseFailure failure = CapabilityLeaseFailure::NotRequested;

    constexpr bool granted() const noexcept
    {
        return failure == CapabilityLeaseFailure::None;
    }
};

constexpr CapabilityLeaseDecision assessCapabilityLease(
    Capability capability,
    bool requested,
    const CapabilityLeaseEvidence& evidence) noexcept
{
    if (capability == Capability::FullProduct)
    {
        return { capability, CapabilityLeaseFailure::FullProductMustAggregate };
    }
    if (!requested)
        return { capability, CapabilityLeaseFailure::NotRequested };
    if (!evidence.exactLoadedRetailImageCurrent)
    {
        return { capability, CapabilityLeaseFailure::ExactLoadedRetailImageStale };
    }
    if (!evidence.requiredFunctionBytesCurrent)
    {
        return { capability, CapabilityLeaseFailure::RequiredFunctionBytesStale };
    }
    if (!evidence.requiredObjectAncestryCurrent)
    {
        return { capability, CapabilityLeaseFailure::RequiredObjectAncestryStale };
    }
    if (!evidence.compatibilityInventoryCurrent)
    {
        return { capability, CapabilityLeaseFailure::CompatibilityInventoryStale };
    }
    if (!evidence.producerProcessCurrent)
    {
        return { capability, CapabilityLeaseFailure::ProducerProcessStale };
    }
    if (!evidence.producerEpochCurrent)
    {
        return { capability, CapabilityLeaseFailure::ProducerEpochStale };
    }
    if (!evidence.poseLineageCurrent)
    {
        return { capability, CapabilityLeaseFailure::PoseLineageStale };
    }
    if (!evidence.runtimeStateLineageCurrent)
    {
        return { capability, CapabilityLeaseFailure::RuntimeStateLineageStale };
    }
    if (!evidence.productModeCurrent)
    {
        return { capability, CapabilityLeaseFailure::ProductModeStale };
    }
    if (!evidence.lineage.complete())
    {
        return { capability, CapabilityLeaseFailure::LineageIdentityMissing };
    }
    if (capabilityRequiresInputOwner(capability))
    {
        if (evidence.inputOwner != SelectedProductInputOwner)
        {
            return { capability, CapabilityLeaseFailure::InputOwnerMismatch };
        }
        if (!evidence.exclusiveInputOwnerCurrent)
        {
            return { capability, CapabilityLeaseFailure::InputOwnerNotExclusive };
        }
    }
    if (!evidence.capabilitySpecificProofComplete)
    {
        return {
            capability,
            CapabilityLeaseFailure::CapabilitySpecificProofIncomplete,
        };
    }
    return { capability, CapabilityLeaseFailure::None };
}

struct FullProductCapabilityEvidence
{
    CapabilityLeaseEvidence cameraStereoVisual {};
    CapabilityLeaseEvidence controllerInput {};
    CapabilityLeaseEvidence uiInteraction {};
    CapabilityLeaseEvidence visualRig {};
    CapabilityLeaseEvidence combatAim {};
    CapabilityLeaseEvidence gpuPresentation {};
    bool releaseQualificationComplete = false;
};

struct FullProductLeaseDecision
{
    Capability rejectedCapability = Capability::FullProduct;
    CapabilityLeaseFailure failure = CapabilityLeaseFailure::NotRequested;

    constexpr bool granted() const noexcept
    {
        return failure == CapabilityLeaseFailure::None;
    }
};

constexpr FullProductLeaseDecision assessFullProductLease(
    bool requested,
    const FullProductCapabilityEvidence& evidence) noexcept
{
    if (!requested)
        return { Capability::FullProduct, CapabilityLeaseFailure::NotRequested };

    const CapabilityLeaseDecision camera = assessCapabilityLease(
        Capability::CameraStereoVisual,
        true,
        evidence.cameraStereoVisual);
    if (!camera.granted())
        return { camera.capability, camera.failure };

    const CapabilityLeaseDecision input = assessCapabilityLease(
        Capability::ControllerInput,
        true,
        evidence.controllerInput);
    if (!input.granted())
        return { input.capability, input.failure };

    const CapabilityLeaseDecision ui = assessCapabilityLease(
        Capability::UiInteraction,
        true,
        evidence.uiInteraction);
    if (!ui.granted())
        return { ui.capability, ui.failure };

    const CapabilityLeaseDecision rig = assessCapabilityLease(
        Capability::VisualRig,
        true,
        evidence.visualRig);
    if (!rig.granted())
        return { rig.capability, rig.failure };

    const CapabilityLeaseDecision combat = assessCapabilityLease(
        Capability::CombatAim,
        true,
        evidence.combatAim);
    if (!combat.granted())
        return { combat.capability, combat.failure };

    const CapabilityLeaseDecision gpu = assessCapabilityLease(
        Capability::GpuPresentation,
        true,
        evidence.gpuPresentation);
    if (!gpu.granted())
        return { gpu.capability, gpu.failure };

    if (!evidence.releaseQualificationComplete)
    {
        return {
            Capability::FullProduct,
            CapabilityLeaseFailure::ReleaseQualificationIncomplete,
        };
    }
    return { Capability::FullProduct, CapabilityLeaseFailure::None };
}
}
