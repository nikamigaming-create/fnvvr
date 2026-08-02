#include "fnvxr_product_capabilities.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

constexpr fnvxr::product::CapabilityLeaseEvidence completeEvidence()
{
    using fnvxr::product::InputOwner;
    fnvxr::product::CapabilityLeaseEvidence evidence {};
    evidence.exactLoadedRetailImageCurrent = true;
    evidence.requiredFunctionBytesCurrent = true;
    evidence.requiredObjectAncestryCurrent = true;
    evidence.compatibilityInventoryCurrent = true;
    evidence.producerProcessCurrent = true;
    evidence.producerEpochCurrent = true;
    evidence.poseLineageCurrent = true;
    evidence.runtimeStateLineageCurrent = true;
    evidence.productModeCurrent = true;
    evidence.lineage = {
        0x1234u,
        7u,
        11u,
        0x5678u,
        13u,
        17u,
        19u,
        23u,
    };
    evidence.inputOwner = InputOwner::NvseMainGameLoop;
    evidence.exclusiveInputOwnerCurrent = true;
    evidence.capabilitySpecificProofComplete = true;
    return evidence;
}

fnvxr::product::FullProductCapabilityEvidence completeFullProductEvidence()
{
    const fnvxr::product::CapabilityLeaseEvidence complete = completeEvidence();
    fnvxr::product::FullProductCapabilityEvidence evidence {};
    evidence.cameraStereoVisual = complete;
    evidence.controllerInput = complete;
    evidence.uiInteraction = complete;
    evidence.visualRig = complete;
    evidence.combatAim = complete;
    evidence.gpuPresentation = complete;
    evidence.releaseQualificationComplete = true;
    return evidence;
}
}

int main()
{
    using namespace fnvxr::product;

    static_assert(SelectedProductInputOwner == InputOwner::NvseMainGameLoop);
    static_assert(!inputOwnerIsProxy(SelectedProductInputOwner));
    static_assert(inputOwnerIsProxy(InputOwner::DirectInputProxy));
    static_assert(inputOwnerIsProxy(InputOwner::XInputProxy));

    constexpr Capability nonAggregateCapabilities[] = {
        Capability::CameraStereoVisual,
        Capability::ControllerInput,
        Capability::UiInteraction,
        Capability::VisualRig,
        Capability::CombatAim,
        Capability::GpuPresentation,
    };
    constexpr CapabilityLeaseEvidence complete = completeEvidence();
    for (const Capability capability : nonAggregateCapabilities)
    {
        const CapabilityLeaseDecision decision = assessCapabilityLease(
            capability,
            true,
            complete);
        if (!decision.granted() || decision.capability != capability)
            return fail("complete isolated capability evidence was rejected");
    }

    if (assessCapabilityLease(
            Capability::CameraStereoVisual,
            false,
            complete).failure != CapabilityLeaseFailure::NotRequested
        || assessCapabilityLease(
            Capability::FullProduct,
            true,
            complete).failure
            != CapabilityLeaseFailure::FullProductMustAggregate)
    {
        return fail("capability request and aggregate-only boundaries changed");
    }

    struct RequiredEvidence
    {
        bool CapabilityLeaseEvidence::* field = nullptr;
        CapabilityLeaseFailure failure = CapabilityLeaseFailure::None;
    };
    constexpr RequiredEvidence required[] = {
        { &CapabilityLeaseEvidence::exactLoadedRetailImageCurrent,
          CapabilityLeaseFailure::ExactLoadedRetailImageStale },
        { &CapabilityLeaseEvidence::requiredFunctionBytesCurrent,
          CapabilityLeaseFailure::RequiredFunctionBytesStale },
        { &CapabilityLeaseEvidence::requiredObjectAncestryCurrent,
          CapabilityLeaseFailure::RequiredObjectAncestryStale },
        { &CapabilityLeaseEvidence::compatibilityInventoryCurrent,
          CapabilityLeaseFailure::CompatibilityInventoryStale },
        { &CapabilityLeaseEvidence::producerProcessCurrent,
          CapabilityLeaseFailure::ProducerProcessStale },
        { &CapabilityLeaseEvidence::producerEpochCurrent,
          CapabilityLeaseFailure::ProducerEpochStale },
        { &CapabilityLeaseEvidence::poseLineageCurrent,
          CapabilityLeaseFailure::PoseLineageStale },
        { &CapabilityLeaseEvidence::runtimeStateLineageCurrent,
          CapabilityLeaseFailure::RuntimeStateLineageStale },
        { &CapabilityLeaseEvidence::productModeCurrent,
          CapabilityLeaseFailure::ProductModeStale },
        { &CapabilityLeaseEvidence::capabilitySpecificProofComplete,
          CapabilityLeaseFailure::CapabilitySpecificProofIncomplete },
    };
    for (const RequiredEvidence item : required)
    {
        CapabilityLeaseEvidence incomplete = complete;
        incomplete.*(item.field) = false;
        const CapabilityLeaseDecision decision = assessCapabilityLease(
            Capability::CameraStereoVisual,
            true,
            incomplete);
        if (decision.granted() || decision.failure != item.failure)
            return fail("a stale common mutation prerequisite was accepted");
    }

    CapabilityLeaseEvidence missingLineage = complete;
    missingLineage.lineage.poseSequence = 0u;
    if (assessCapabilityLease(
            Capability::CameraStereoVisual,
            true,
            missingLineage).failure
        != CapabilityLeaseFailure::LineageIdentityMissing)
    {
        return fail("a lease without concrete lineage identities was accepted");
    }

    CapabilityLeaseEvidence wrongOwner = complete;
    wrongOwner.inputOwner = InputOwner::DirectInputProxy;
    if (assessCapabilityLease(
            Capability::ControllerInput,
            true,
            wrongOwner).failure != CapabilityLeaseFailure::InputOwnerMismatch
        || assessCapabilityLease(
            Capability::UiInteraction,
            true,
            wrongOwner).failure != CapabilityLeaseFailure::InputOwnerMismatch)
    {
        return fail("a proxy input owner was accepted for product input or UI");
    }
    CapabilityLeaseEvidence sharedOwner = complete;
    sharedOwner.exclusiveInputOwnerCurrent = false;
    if (assessCapabilityLease(
            Capability::ControllerInput,
            true,
            sharedOwner).failure
        != CapabilityLeaseFailure::InputOwnerNotExclusive)
    {
        return fail("non-exclusive controller ownership was accepted");
    }

    FullProductCapabilityEvidence full = completeFullProductEvidence();
    if (!assessFullProductLease(true, full).granted())
        return fail("complete aggregate release evidence was rejected");
    full.combatAim.capabilitySpecificProofComplete = false;
    const FullProductLeaseDecision rejectedCombat = assessFullProductLease(true, full);
    if (rejectedCombat.granted()
        || rejectedCombat.rejectedCapability != Capability::CombatAim
        || rejectedCombat.failure
            != CapabilityLeaseFailure::CapabilitySpecificProofIncomplete)
    {
        return fail("full product lease hid a rejected component capability");
    }
    full = completeFullProductEvidence();
    full.releaseQualificationComplete = false;
    const FullProductLeaseDecision rejectedRelease = assessFullProductLease(true, full);
    if (rejectedRelease.granted()
        || rejectedRelease.rejectedCapability != Capability::FullProduct
        || rejectedRelease.failure
            != CapabilityLeaseFailure::ReleaseQualificationIncomplete)
    {
        return fail("full product lease bypassed release qualification");
    }

    std::cout << "product capability lease contract passed\n";
    return EXIT_SUCCESS;
}
