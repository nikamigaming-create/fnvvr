#include "fnvxr_windows_module_census.h"

#include <cstdlib>
#include <iostream>
#include <type_traits>

namespace
{
using namespace fnvxr::inventory;
using namespace fnvxr::module_census;

constexpr ProcessCreationIdentity ProcessIdentity {
    29072u,
    133812345678901234u
};
constexpr std::uint64_t Generation = 41u;
constexpr std::string_view SyntheticSha =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

[[noreturn]] void fail(const char* message)
{
    std::cerr << message << '\n';
    std::exit(1);
}

void require(bool condition, const char* message)
{
    if (!condition)
        fail(message);
}

ObservedPeIdentity syntheticIdentity(std::size_t index)
{
    return snapshotPeIdentity({
        PeMachineX86,
        static_cast<std::uint32_t>(0x10000000u + index),
        static_cast<std::uint32_t>(0x1000u + index * 0x100u),
        0x10000000u,
        static_cast<std::uint64_t>(0x2000u + index),
        SyntheticSha
    });
}

ModuleObservation completeObservation(std::size_t index)
{
    const ExpectedModule& expected = AllowedModules[index];
    ModuleObservation module {};
    module.origin = expected.origin;
    module.normalizedRelativePath = expected.normalizedRelativePath;
    module.normalizedModuleName = baseName(expected.normalizedRelativePath);
    module.identity = expected.exactIdentityRequired
        ? snapshotPeIdentity(expected.exactIdentity)
        : syntheticIdentity(index);
    module.runtimeModuleBase = static_cast<std::uint32_t>(
        0x01000000ull + index * 0x02000000ull);
    module.runtimeModuleBytes = module.identity.sizeOfImage;
    module.canonicalPathObserved = true;
    module.authenticodeValid = expected.origin != ModuleOrigin::GameTree;
    module.loadedExecutableSectionsMatched = true;
    return module;
}

void appendCompleteSnapshot(OwnedWindowsModuleCensus& census, bool post)
{
    for (std::size_t index = 0; index < AllowedModuleCount; ++index)
    {
        const ModuleObservation module = completeObservation(index);
        const bool appended = post
            ? census.appendPostProof(module)
            : census.appendPrimary(module);
        require(appended, "a bounded census observation was not accepted");
    }
}

void testOwnedSealedEvidence()
{
    static_assert(!std::is_copy_constructible_v<OwnedWindowsModuleCensus>);
    static_assert(!std::is_move_constructible_v<OwnedWindowsModuleCensus>);
    static_assert(OwnedWindowsModuleCensus::Capacity == 512u);
    static_assert(!OwnedWindowsModuleCensus::ProductionAuthorizationAvailable);

    OwnedWindowsModuleCensus census(ProcessIdentity, Generation);
    require(census.phase() == CensusPhase::CollectingPrimary,
        "a valid census did not begin in the primary phase");

    ModuleObservation first = completeObservation(0u);
    require(census.appendPrimary(first), "primary observation append failed");
    first.runtimeModuleBase ^= 0x1000u;
    for (std::size_t index = 1; index < AllowedModuleCount; ++index)
        require(census.appendPrimary(completeObservation(index)),
            "primary census did not own its bounded observation");

    require(census.completePrimaryToolhelp32(true),
        "primary Toolhelp completion failed");
    require(census.completeProofCollection(900u),
        "proof-completion boundary failed");
    appendCompleteSnapshot(census, true);
    require(census.completePostProofToolhelp32(901u),
        "post-proof Toolhelp completion failed");
    require(census.seal(), "complete census did not seal");

    const ModuleInventoryEvidence* evidence = census.sealedView();
    require(evidence != nullptr, "sealed census exposed no stable view");
    require(evidence->modules != evidence->postProofModules,
        "primary and post-proof storage aliases");
    require(evidence->moduleCount == AllowedModuleCount
            && evidence->postProofModuleCount == AllowedModuleCount,
        "sealed census counts changed");
    require(evidence->modules[0].runtimeModuleBase
            == completeObservation(0u).runtimeModuleBase,
        "the census retained caller-owned observation storage");

    require(sameProcessCreation(
                evidence->transactionProcessIdentity, ProcessIdentity)
            && sameProcessCreation(
                evidence->primaryProcessIdentity, ProcessIdentity)
            && sameProcessCreation(
                evidence->postProofProcessIdentity, ProcessIdentity)
            && sameProcessCreation(
                evidence->proxyPolicy.processIdentity, ProcessIdentity)
            && sameProcessCreation(
                evidence->jipProof.processIdentity, ProcessIdentity)
            && sameProcessCreation(
                evidence->showOffProof.processIdentity, ProcessIdentity),
        "sealed evidence crossed process-creation identities");
    require(evidence->transactionGeneration == Generation
            && evidence->primaryEvidenceGeneration == Generation
            && evidence->postProofEvidenceGeneration == Generation
            && evidence->proxyPolicy.evidenceGeneration == Generation
            && evidence->jipProof.evidenceGeneration == Generation
            && evidence->showOffProof.evidenceGeneration == Generation,
        "sealed evidence crossed generations");

    require(evidence->transactionBeginOrdinal == 0u,
        "an external census fabricated transaction adjacency");
    require(!census.productionAuthorized(),
        "a diagnostic census granted production authorization");
    require(evidence->jipProof.disposition == ProofDisposition::Failed
            && evidence->showOffProof.disposition == ProofDisposition::Failed,
        "unimplemented live proofs were not failed closed");

    const ModuleObservation* const primaryAddress = evidence->modules;
    const ModuleObservation* const postAddress = evidence->postProofModules;
    require(!census.appendPrimary(completeObservation(0u)),
        "a sealed census accepted mutation");
    require(census.sealedView()->modules == primaryAddress
            && census.sealedView()->postProofModules == postAddress,
        "a rejected post-seal mutation destabilized the view");

    const ModuleInventoryAssessment assessment = assessModuleInventory(*evidence);
    require(!assessment.overallAccepted
            && assessment.failure
                == InventoryFailure::RevalidationNotTransactionAdjacent,
        "transactionBeginOrdinal zero did not fail closed");
}

void testCapacityAndOrderingFailClosed()
{
    OwnedWindowsModuleCensus overflow(ProcessIdentity, Generation);
    ModuleObservation module {};
    module.normalizedModuleName = "module.dll";
    for (std::size_t index = 0; index < OwnedWindowsModuleCensus::Capacity; ++index)
        require(overflow.appendPrimary(module),
            "the bounded census rejected an in-range observation");
    require(!overflow.appendPrimary(module),
        "the census accepted a 513th observation");
    require(overflow.failure() == CensusFailure::CapacityExceeded
            && overflow.sealedView() == nullptr,
        "capacity overflow did not poison the census");

    OwnedWindowsModuleCensus wrongOrder(ProcessIdentity, Generation);
    require(!wrongOrder.appendPostProof(module),
        "post-proof observation was accepted before primary completion");
    require(wrongOrder.failure() == CensusFailure::WrongPhase,
        "out-of-order evidence did not fail closed");

    OwnedWindowsModuleCensus missingIdentity({}, Generation);
    require(missingIdentity.failure() == CensusFailure::InvalidProcessIdentity,
        "a missing PID/creation identity was accepted");
    OwnedWindowsModuleCensus missingGeneration(ProcessIdentity, 0u);
    require(missingGeneration.failure() == CensusFailure::InvalidGeneration,
        "generation zero was accepted");
}
}

int main()
{
    testOwnedSealedEvidence();
    testCapacityAndOrderingFailClosed();
    return 0;
}
