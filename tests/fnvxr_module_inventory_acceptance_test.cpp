#include "fnvxr_module_inventory_acceptance.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using namespace fnvxr::inventory;

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

constexpr std::string_view SyntheticSha =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
constexpr std::string_view DifferentSha =
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
constexpr ProcessCreationIdentity SyntheticProcessIdentity {
    29072u,
    133812345678901234u
};
constexpr std::uint64_t SyntheticGeneration = 41u;
constexpr FixedAsciiSnapshot<4u> CompileTimeSnapshot { "fnvr" };
static_assert(CompileTimeSnapshot.valid());
static_assert(CompileTimeSnapshot.view() == "fnvr");

constexpr bool compileTimeNonterminationFailsClosed()
{
    constexpr std::array<char, 4u> bytes { 'n', 'o', 'n', 'e' };
    FixedAsciiSnapshot<4u> snapshot {};
    return !snapshot.captureNullTerminated(bytes.data(), bytes.size())
        && !snapshot.valid();
}
static_assert(compileTimeNonterminationFailsClosed());

ObservedPeIdentity syntheticIdentity(std::size_t index)
{
    const PeIdentity identity {
        PeMachineX86,
        static_cast<std::uint32_t>(0x10000000u + index),
        static_cast<std::uint32_t>(0x1000u + index * 0x100u),
        0x10000000u,
        static_cast<std::uint64_t>(0x2000u + index),
        SyntheticSha
    };
    return snapshotPeIdentity(identity);
}

std::vector<ModuleObservation> completeModuleSet()
{
    std::vector<ModuleObservation> modules;
    modules.reserve(AllowedModuleCount);
    for (std::size_t index = 0; index < AllowedModuleCount; ++index)
    {
        const ExpectedModule& expected = AllowedModules[index];
        ModuleObservation module {};
        module.origin = expected.origin;
        module.normalizedRelativePath = expected.normalizedRelativePath;
        module.normalizedModuleName =
            baseName(expected.normalizedRelativePath);
        module.identity = expected.exactIdentityRequired
            ? snapshotPeIdentity(expected.exactIdentity)
            : syntheticIdentity(index);
        module.runtimeModuleBase = static_cast<std::uint32_t>(
            0x01000000ull + index * 0x02000000ull);
        module.runtimeModuleBytes = module.identity.sizeOfImage;
        module.canonicalPathObserved = true;
        module.authenticodeValid = expected.origin != ModuleOrigin::GameTree;
        module.loadedExecutableSectionsMatched = true;
        modules.push_back(module);
    }
    return modules;
}

TransparentProxyPolicyEvidence completeProxyPolicy()
{
    TransparentProxyPolicyEvidence policy {};
    policy.processIdentity = SyntheticProcessIdentity;
    policy.evidenceGeneration = SyntheticGeneration;
    policy.d3dExactTransparentBuildIdentityMatched = true;
    policy.d3dMappedExecutableSectionsMatched = true;
    policy.d3dActivationSourceFuseFalse = true;
    policy.d3dProductIntegrationSourceFuseFalse = true;
    policy.d3dTransparentForwardingVerified = true;
    policy.dinputExactTransparentBuildIdentityMatched = true;
    policy.dinputMappedExecutableSectionsMatched = true;
    policy.dinputProductionSourceFuseFalse = true;
    policy.dinputProductIntegrationSourceFuseFalse = true;
    policy.dinputTransparentForwardingVerified = true;
    policy.xinputExactTransparentBuildIdentityMatched = true;
    policy.xinputMappedExecutableSectionsMatched = true;
    policy.xinputProductionSourceFuseFalse = true;
    policy.xinputProductIntegrationSourceFuseFalse = true;
    policy.xinputTransparentForwardingVerified = true;
    policy.noConfigurationOrSharedMemoryBypass = true;
    return policy;
}

JipCompatibilityProof completeJipProof()
{
    JipCompatibilityProof proof {};
    proof.processIdentity = SyntheticProcessIdentity;
    proof.evidenceGeneration = SyntheticGeneration;
    proof.disposition = ProofDisposition::Passed;
    proof.loadedExecutableSectionsMatched = true;
    proof.exactOwnedPatchInventoryMatched = true;
    proof.exactHookTargetMatched = true;
    proof.normalizedRetailManifestMatched = true;
    proof.noUnexpectedExecutableWrites = true;
    return proof;
}

ShowOffCompatibilityProof completeShowOffProof()
{
    ShowOffCompatibilityProof proof {};
    proof.processIdentity = SyntheticProcessIdentity;
    proof.evidenceGeneration = SyntheticGeneration;
    proof.disposition = ProofDisposition::Passed;
    proof.loadedExecutableSectionsMatched = true;
    proof.exactOwnedPatchInventoryMatched = true;
    proof.protectedRendererBytesMatched = true;
    proof.noUnexpectedExecutableWrites = true;
    return proof;
}

ModuleInventoryEvidence completeEvidence(
    const std::vector<ModuleObservation>& modules,
    const std::vector<ModuleObservation>& postProofModules)
{
    ModuleInventoryEvidence evidence {};
    evidence.enumerationComplete = true;
    evidence.usedToolhelp32ModuleAndModule32 = true;
    evidence.targetProcessIs32Bit = true;
    evidence.transactionProcessIdentity = SyntheticProcessIdentity;
    evidence.transactionGeneration = SyntheticGeneration;
    evidence.primaryProcessIdentity = SyntheticProcessIdentity;
    evidence.primaryEvidenceGeneration = SyntheticGeneration;
    evidence.modules = modules.data();
    evidence.moduleCount = modules.size();
    evidence.proxyPolicy = completeProxyPolicy();
    evidence.jipProof = completeJipProof();
    evidence.showOffProof = completeShowOffProof();
    evidence.postProofEnumerationComplete = true;
    evidence.postProofUsedToolhelp32ModuleAndModule32 = true;
    evidence.postProofProcessIdentity = SyntheticProcessIdentity;
    evidence.postProofEvidenceGeneration = SyntheticGeneration;
    evidence.postProofModules = postProofModules.data();
    evidence.postProofModuleCount = postProofModules.size();
    evidence.proofCompletionOrdinal = 900u;
    evidence.postProofEnumerationOrdinal = 901u;
    evidence.transactionBeginOrdinal = 902u;
    return evidence;
}

ModuleInventoryAssessment assess(
    ModuleInventoryEvidence evidence,
    const std::vector<ModuleObservation>& modules)
{
    evidence.modules = modules.data();
    evidence.moduleCount = modules.size();
    return assessModuleInventory(evidence);
}

void testOwnedAsciiSnapshots()
{
    FixedAsciiSnapshot<4u> snapshot { "abcd" };
    require(snapshot.valid() && snapshot.view() == "abcd",
        "a capacity-sized ASCII snapshot was rejected");

    const FixedAsciiSnapshot<4u> independentCopy = snapshot;
    snapshot = "wxyz";
    require(independentCopy.valid() && independentCopy.view() == "abcd",
        "an observed text snapshot retained aliased backing storage");

    const std::array<char, 5u> noTerminator { 'a', 'b', 'c', 'd', 'e' };
    require(!snapshot.captureNullTerminated(
                noTerminator.data(), noTerminator.size())
            && !snapshot.valid(),
        "a nonterminated observed string was accepted");

    const std::array<char, 6u> overflow {
        'a', 'b', 'c', 'd', 'e', '\0'
    };
    require(!snapshot.captureNullTerminated(overflow.data(), overflow.size())
            && !snapshot.valid(),
        "an over-capacity observed string was accepted");

    const std::array<char, 4u> embeddedNull { 'a', '\0', 'b', 'c' };
    require(!snapshot.capture(std::string_view(
                embeddedNull.data(), embeddedNull.size()))
            && !snapshot.valid(),
        "an embedded NUL was accepted as snapshot content");

    const std::array<char, 1u> nonAscii { static_cast<char>(0x80u) };
    require(!snapshot.capture(
                std::string_view(nonAscii.data(), nonAscii.size()))
            && !snapshot.valid(),
        "non-ASCII observed text was accepted");

    snapshot = "abc";
    snapshot.bytes[snapshot.length] = 'x';
    require(!snapshot.valid(),
        "a snapshot with a corrupted terminator was accepted");
}

void testCensusShape()
{
    static_assert(AllowedModuleCount == 124u);
    static_assert(
        MaximumDiagnosticModuleCount == 512u,
        "pure acceptance must match the census builder's fixed bound");
    std::size_t game = 0;
    std::size_t steam = 0;
    std::size_t windows = 0;
    std::size_t driver = 0;
    std::size_t exact = 0;
    std::size_t d3dNames = 0;
    std::size_t dinputNames = 0;
    for (const ExpectedModule& module : AllowedModules)
    {
        require(normalizedRelativePath(module.normalizedRelativePath),
            "allowlist contains a non-normalized path");
        require(baseName(module.normalizedRelativePath)
                != "gameoverlayrenderer.dll",
            "Steam overlay entered the allowlist");
        require(baseName(module.normalizedRelativePath) != "nvspcap.dll",
            "NVIDIA capture overlay entered the allowlist");
        if (module.exactIdentityRequired)
        {
            ++exact;
            require(peIdentityObserved(module.exactIdentity),
                "exact allowlist identity is incomplete");
        }
        switch (module.origin)
        {
            case ModuleOrigin::GameTree: ++game; break;
            case ModuleOrigin::SteamRuntime: ++steam; break;
            case ModuleOrigin::WindowsRuntime: ++windows; break;
            case ModuleOrigin::NvidiaDriverRuntime: ++driver; break;
            default: fail("unknown allowlist origin");
        }
        if (baseName(module.normalizedRelativePath) == "d3d9.dll")
            ++d3dNames;
        if (baseName(module.normalizedRelativePath) == "dinput8.dll")
            ++dinputNames;
    }
    require(game == 13u && steam == 5u && windows == 101u && driver == 5u,
        "census origin counts changed");
    require(exact == 10u,
        "main/NVSE/FNVXR/JIP/ShowOff and retail dependency pins changed");
    require(d3dNames == 2u && dinputNames == 2u,
        "known local/system duplicate names are not represented exactly");

    const std::size_t mainIndex = findExpectedModule(
        ModuleOrigin::GameTree,
        "falloutnv.exe");
    const std::size_t fnvxrIndex = findExpectedModule(
        ModuleOrigin::GameTree,
        "data/nvse/plugins/nvse_fnvxr.dll");
    const std::size_t jipIndex = findExpectedModule(
        ModuleOrigin::GameTree,
        "data/nvse/plugins/jip_nvse.dll");
    const std::size_t showOffIndex = findExpectedModule(
        ModuleOrigin::GameTree,
        "data/nvse/plugins/showoffnvse.dll");
    require(mainIndex < AllowedModuleCount
            && fnvxrIndex < AllowedModuleCount
            && jipIndex < AllowedModuleCount
            && showOffIndex < AllowedModuleCount,
        "required exact compatibility module is absent");
    require(AllowedModules[mainIndex].exactIdentity.sha256
            == "518C87F58A6C4D9826E9EF8FBB7F4213882FA70822675610D45AEA2464502A57",
        "retail executable pin changed");
    require(AllowedModules[fnvxrIndex].exactIdentity.sha256
            == "E394CB96DC1F881E66DF5E11877BCCBA55033FE50A4D407FD70CCB359BB3650D",
        "FNVXR plugin pin changed");
    require(AllowedModules[jipIndex].exactIdentity.sha256
            == "9D2779647ED0CE63043390F47FC978E3234AF8E558DC6CB6BCB231478A2D74D4",
        "JIP pin changed");
    require(AllowedModules[showOffIndex].exactIdentity.sha256
            == "37CB22C5288FEDD0D57196C8C2F6BBABA5A1DAFD9CE58F14DAC9410DBEE7EF3E",
        "ShowOff pin changed");
}

void testDefaultAndCompleteEvidence()
{
    const ModuleInventoryAssessment defaultResult =
        assessModuleInventory(ModuleInventoryEvidence {});
    require(!defaultResult.overallAccepted
            && !defaultResult.moduleSetAccepted
            && !defaultResult.proxyPoliciesAccepted
            && !defaultResult.jipProofAccepted
            && !defaultResult.showOffProofAccepted
            && defaultResult.failure == InventoryFailure::EnumerationIncomplete,
        "default inventory evidence did not fail closed");

    const std::vector<ModuleObservation> modules = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = modules;
    const ModuleInventoryAssessment complete =
        assessModuleInventory(completeEvidence(modules, postProofModules));
    require(complete.moduleSetAccepted
            && complete.overlaysAbsent
            && complete.proxyPoliciesAccepted
            && complete.jipProofAccepted
            && complete.showOffProofAccepted
            && complete.overallAccepted
            && complete.failure == InventoryFailure::None,
        "synthetic fully proven inventory was rejected");
}

void testEnumerationEnvelope()
{
    const std::vector<ModuleObservation> modules = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = modules;
    ModuleInventoryEvidence evidence = completeEvidence(modules, postProofModules);

    evidence.enumerationComplete = false;
    require(assess(evidence, modules).failure
            == InventoryFailure::EnumerationIncomplete,
        "partial enumeration was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.usedToolhelp32ModuleAndModule32 = false;
    require(assess(evidence, modules).failure
            == InventoryFailure::WrongEnumerationMode,
        "non-MODULE32 enumeration was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.targetProcessIs32Bit = false;
    require(assess(evidence, modules).failure
            == InventoryFailure::TargetNot32Bit,
        "non-x86 target was accepted");
    const ModuleObservation* const poison =
        reinterpret_cast<const ModuleObservation*>(
            static_cast<std::uintptr_t>(1u));
    evidence = completeEvidence(modules, postProofModules);
    evidence.modules = poison;
    evidence.moduleCount = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::PrimaryModuleCountMismatch,
        "zero primary count was not rejected before pointer access");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofModules = poison;
    evidence.postProofModuleCount = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::PostProofModuleCountMismatch,
        "zero post-proof count was not rejected before pointer access");
    evidence = completeEvidence(modules, postProofModules);
    evidence.modules = poison;
    evidence.moduleCount = MaximumDiagnosticModuleCount + 1u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::PrimaryModuleCountMismatch,
        "oversized primary count was not rejected before pointer access");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofModules = poison;
    evidence.postProofModuleCount = MaximumDiagnosticModuleCount + 1u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::PostProofModuleCountMismatch,
        "oversized post-proof count was not rejected before pointer access");
    evidence = completeEvidence(modules, postProofModules);
    evidence.modules = poison;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::ObservationStorageInvalid,
        "misaligned primary observation storage was dereferenced");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofModules = poison;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::PostProofObservationStorageInvalid,
        "misaligned post-proof observation storage was dereferenced");
    evidence = completeEvidence(modules, postProofModules);
    evidence.modules = nullptr;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::ObservationStorageMissing,
        "missing observation storage was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofModules = nullptr;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::PostProofObservationStorageMissing,
        "missing post-proof observation storage was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofModules = evidence.modules;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::PostProofObservationStorageAliased,
        "the primary census was reused as its own re-enumeration");

    std::array<ModuleObservation, AllowedModuleCount + 1u> overlapStorage {};
    evidence = completeEvidence(modules, postProofModules);
    evidence.modules = overlapStorage.data();
    evidence.postProofModules = overlapStorage.data() + 1u;
    const ModuleInventoryAssessment partialOverlap =
        assessModuleInventory(evidence);
    if (partialOverlap.failure
        != InventoryFailure::PostProofObservationStorageAliased)
    {
        std::cerr << "partial-overlap actual failure "
                  << static_cast<int>(partialOverlap.failure)
                  << " address "
                  << reinterpret_cast<std::uintptr_t>(overlapStorage.data())
                  << " alignment " << alignof(ModuleObservation)
                  << " bytes "
                  << evidence.moduleCount * sizeof(ModuleObservation)
                  << '\n';
        fail("partially overlapping census storage was accepted as independent");
    }
}

void testTemporalEvidenceBindings()
{
    const std::vector<ModuleObservation> modules = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = modules;
    ModuleInventoryEvidence evidence =
        completeEvidence(modules, postProofModules);

    evidence.postProofEnumerationComplete = false;
    const ModuleInventoryAssessment incompleteRevalidation =
        assessModuleInventory(evidence);
    require(incompleteRevalidation.failure
                == InventoryFailure::PostProofEnumerationIncomplete
            && !incompleteRevalidation.proxyPoliciesAccepted
            && !incompleteRevalidation.jipProofAccepted
            && !incompleteRevalidation.showOffProofAccepted,
        "an incomplete post-proof enumeration was accepted");

    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofUsedToolhelp32ModuleAndModule32 = false;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::WrongPostProofEnumerationMode,
        "a post-proof census without MODULE|MODULE32 was accepted");

    evidence = completeEvidence(modules, postProofModules);
    evidence.transactionProcessIdentity = {};
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingTransactionProcessIdentity,
        "a transaction without process creation identity was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.primaryProcessIdentity = {};
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingPrimaryProcessIdentity,
        "a primary census without process creation identity was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofProcessIdentity = {};
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingPostProofProcessIdentity,
        "a post-proof census without process creation identity was accepted");

    evidence = completeEvidence(modules, postProofModules);
    evidence.proxyPolicy.processIdentity = {};
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceProcessIdentity,
        "proxy evidence without process creation identity was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.jipProof.processIdentity = {};
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceProcessIdentity,
        "JIP evidence without process creation identity was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.showOffProof.processIdentity = {};
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceProcessIdentity,
        "ShowOff evidence without process creation identity was accepted");

    evidence = completeEvidence(modules, postProofModules);
    ++evidence.primaryProcessIdentity.creationTime100ns;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceProcessIdentityMismatch,
        "primary census from another process creation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.postProofProcessIdentity.processId;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceProcessIdentityMismatch,
        "post-proof census from another process was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.proxyPolicy.processIdentity.creationTime100ns;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceProcessIdentityMismatch,
        "proxy proof from another process creation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.jipProof.processIdentity.creationTime100ns;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceProcessIdentityMismatch,
        "JIP proof from another process creation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.showOffProof.processIdentity.creationTime100ns;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceProcessIdentityMismatch,
        "ShowOff proof from another process creation was accepted");

    evidence = completeEvidence(modules, postProofModules);
    evidence.transactionGeneration = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingTransactionGeneration,
        "a zero transaction generation was accepted");

    evidence = completeEvidence(modules, postProofModules);
    evidence.primaryEvidenceGeneration = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceGeneration,
        "a zero primary generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofEvidenceGeneration = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceGeneration,
        "a zero post-proof generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.proxyPolicy.evidenceGeneration = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceGeneration,
        "a zero proxy generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.jipProof.evidenceGeneration = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceGeneration,
        "a zero JIP generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.showOffProof.evidenceGeneration = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::MissingEvidenceGeneration,
        "a zero ShowOff generation was accepted");

    evidence = completeEvidence(modules, postProofModules);
    ++evidence.primaryEvidenceGeneration;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceGenerationMismatch,
        "primary evidence from another generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.postProofEvidenceGeneration;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceGenerationMismatch,
        "post-proof evidence from another generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.proxyPolicy.evidenceGeneration;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceGenerationMismatch,
        "proxy evidence from another generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.jipProof.evidenceGeneration;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceGenerationMismatch,
        "JIP evidence from another generation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    ++evidence.showOffProof.evidenceGeneration;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::EvidenceGenerationMismatch,
        "ShowOff evidence from another generation was accepted");

    evidence = completeEvidence(modules, postProofModules);
    evidence.proofCompletionOrdinal = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "a missing proof-completion ordinal was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofEnumerationOrdinal = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "a missing revalidation ordinal was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.transactionBeginOrdinal = 0u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "a missing transaction-begin ordinal was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.postProofEnumerationOrdinal = evidence.proofCompletionOrdinal;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "a revalidation that did not follow proof completion was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.proofCompletionOrdinal =
        evidence.postProofEnumerationOrdinal + 1u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "a revalidation ordered before proof completion was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.transactionBeginOrdinal = evidence.postProofEnumerationOrdinal;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "a transaction ordered before its revalidation was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.transactionBeginOrdinal =
        evidence.postProofEnumerationOrdinal - 1u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "an out-of-order transaction/revalidation pair was accepted");
    evidence = completeEvidence(modules, postProofModules);
    evidence.transactionBeginOrdinal =
        evidence.postProofEnumerationOrdinal + 2u;
    require(assessModuleInventory(evidence).failure
            == InventoryFailure::RevalidationNotTransactionAdjacent,
        "a stale revalidation separated from transaction start was accepted");

    evidence = completeEvidence(modules, postProofModules);
    evidence.proofCompletionOrdinal =
        evidence.postProofEnumerationOrdinal - 10u;
    require(assessModuleInventory(evidence).overallAccepted,
        "an ordered proof followed by transaction-adjacent revalidation failed");
}

void testPostProofReenumerationExactlyMatches()
{
    const std::vector<ModuleObservation> modules = completeModuleSet();

    auto requireMismatch = [&modules](
        std::vector<ModuleObservation> postProofModules,
        const char* message)
    {
        const ModuleInventoryAssessment result = assessModuleInventory(
            completeEvidence(modules, postProofModules));
        require(result.failure == InventoryFailure::PostProofObservationMismatch
                && !result.overallAccepted,
            message);
    };

    std::vector<ModuleObservation> changed = modules;
    changed[0].origin = ModuleOrigin::SteamRuntime;
    requireMismatch(changed, "post-proof origin drift was accepted");
    changed = modules;
    changed[0].normalizedRelativePath = "falloutnv-renamed.exe";
    requireMismatch(changed, "post-proof path drift was accepted");
    changed = modules;
    changed[0].normalizedModuleName = "falloutnv-renamed.exe";
    requireMismatch(changed, "post-proof name drift was accepted");
    changed = modules;
    ++changed[0].identity.timeDateStamp;
    requireMismatch(changed, "post-proof PE identity drift was accepted");
    changed = modules;
    ++changed[0].runtimeModuleBase;
    requireMismatch(changed, "post-proof module-base drift was accepted");
    changed = modules;
    --changed[0].runtimeModuleBytes;
    requireMismatch(changed, "post-proof module-extent drift was accepted");
    changed = modules;
    changed[0].canonicalPathObserved = false;
    requireMismatch(changed, "post-proof canonical-path drift was accepted");
    changed = modules;
    changed[0].authenticodeValid = true;
    requireMismatch(changed, "post-proof signature observation drift was accepted");
    changed = modules;
    changed[0].loadedExecutableSectionsMatched = false;
    requireMismatch(changed, "post-proof mapped-code drift was accepted");
    changed = modules;
    const ModuleObservation first = changed[0];
    changed[0] = changed[1];
    changed[1] = first;
    requireMismatch(changed, "post-proof census reordering was accepted");
}

void testEveryModuleIsRequiredAndUnique()
{
    const std::vector<ModuleObservation> golden = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = golden;
    const ModuleInventoryEvidence evidence =
        completeEvidence(golden, postProofModules);

    for (std::size_t index = 0; index < golden.size(); ++index)
    {
        std::vector<ModuleObservation> missing = golden;
        missing.erase(missing.begin() + static_cast<std::ptrdiff_t>(index));
        const ModuleInventoryAssessment missingResult = assess(evidence, missing);
        require(!missingResult.overallAccepted
                && missingResult.failure
                    == InventoryFailure::PrimaryModuleCountMismatch,
            "an allowlisted module could be omitted");

        std::vector<ModuleObservation> duplicate = golden;
        duplicate.push_back(golden[index]);
        const ModuleInventoryAssessment duplicateResult = assess(evidence, duplicate);
        require(!duplicateResult.overallAccepted
                && duplicateResult.failure
                    == InventoryFailure::PrimaryModuleCountMismatch,
            "an oversized duplicate census was accepted");

        std::vector<ModuleObservation> unexpected = golden;
        unexpected[index].normalizedRelativePath = "unexpected.dll";
        unexpected[index].normalizedModuleName = "unexpected.dll";
        require(assess(evidence, unexpected).failure
                == InventoryFailure::UnexpectedModule,
            "unknown path replacement was accepted");

        std::vector<ModuleObservation> nameDrift = golden;
        nameDrift[index].normalizedModuleName = "renamed.dll";
        require(assess(evidence, nameDrift).failure
                == InventoryFailure::NamePathMismatch,
            "module-name/path drift was accepted");

        std::vector<ModuleObservation> pathDrift = golden;
        pathDrift[index].canonicalPathObserved = false;
        require(assess(evidence, pathDrift).failure
                == InventoryFailure::NonCanonicalPath,
            "noncanonical module path was accepted");

        std::vector<ModuleObservation> wrongMachine = golden;
        wrongMachine[index].identity.machine = 0x8664u;
        require(assess(evidence, wrongMachine).failure
                == InventoryFailure::InvalidPeIdentity,
            "non-x86 module identity was accepted");

        if (golden[index].origin != ModuleOrigin::GameTree)
        {
            std::vector<ModuleObservation> unsignedRuntime = golden;
            unsignedRuntime[index].authenticodeValid = false;
            require(assess(evidence, unsignedRuntime).failure
                    == InventoryFailure::UnsignedRuntime,
                "unsigned system/runtime module was accepted");
        }
    }

    std::vector<ModuleObservation> duplicateAtExactCount = golden;
    const std::uint32_t replacementBase =
        duplicateAtExactCount.back().runtimeModuleBase;
    duplicateAtExactCount.back() = golden.front();
    duplicateAtExactCount.back().runtimeModuleBase = replacementBase;
    require(assess(evidence, duplicateAtExactCount).failure
            == InventoryFailure::DuplicateModulePath,
        "duplicate path at the exact census count was accepted");

    std::vector<ModuleObservation> extra = golden;
    extra.back().origin = ModuleOrigin::GameTree;
    extra.back().normalizedRelativePath = "unknown_patch.dll";
    extra.back().normalizedModuleName = "unknown_patch.dll";
    extra.back().identity = syntheticIdentity(999u);
    extra.back().runtimeModuleBytes = extra.back().identity.sizeOfImage;
    extra.back().authenticodeValid = false;
    require(assess(evidence, extra).failure == InventoryFailure::UnexpectedModule,
        "unknown game-tree replacement was accepted");

    extra = golden;
    extra.back().origin = ModuleOrigin::Unknown;
    extra.back().normalizedRelativePath = "unknown.dll";
    extra.back().normalizedModuleName = "unknown.dll";
    extra.back().identity = syntheticIdentity(998u);
    extra.back().runtimeModuleBytes = extra.back().identity.sizeOfImage;
    extra.back().authenticodeValid = true;
    require(assess(evidence, extra).failure == InventoryFailure::UnexpectedModule,
        "unknown-origin replacement was accepted");
}

void testEveryExactIdentityField()
{
    const std::vector<ModuleObservation> golden = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = golden;
    const ModuleInventoryEvidence evidence =
        completeEvidence(golden, postProofModules);
    std::size_t tested = 0;
    for (std::size_t index = 0; index < golden.size(); ++index)
    {
        if (!AllowedModules[index].exactIdentityRequired)
            continue;
        ++tested;

        std::vector<ModuleObservation> changed = golden;
        ++changed[index].identity.timeDateStamp;
        require(assess(evidence, changed).failure
                == InventoryFailure::ExactIdentityMismatch,
            "timestamp mismatch was accepted");
        changed = golden;
        ++changed[index].identity.sizeOfImage;
        changed[index].runtimeModuleBytes = changed[index].identity.sizeOfImage;
        require(assess(evidence, changed).failure
                == InventoryFailure::ExactIdentityMismatch,
            "SizeOfImage mismatch was accepted");
        changed = golden;
        ++changed[index].identity.preferredImageBase;
        require(assess(evidence, changed).failure
                == InventoryFailure::ExactIdentityMismatch,
            "image-base mismatch was accepted");
        changed = golden;
        ++changed[index].identity.fileSize;
        require(assess(evidence, changed).failure
                == InventoryFailure::ExactIdentityMismatch,
            "file-size mismatch was accepted");
        changed = golden;
        changed[index].identity.sha256 = DifferentSha;
        require(assess(evidence, changed).failure
                == InventoryFailure::ExactIdentityMismatch,
            "SHA-256 mismatch was accepted");
    }
    require(tested == 10u, "not every exact identity pin was exercised");

    std::vector<ModuleObservation> malformed = golden;
    malformed[0].identity.sha256 = "ABC";
    require(assess(evidence, malformed).failure
            == InventoryFailure::InvalidPeIdentity,
        "malformed SHA-256 observation was accepted");
}

void testObservationTextSnapshotsFailClosed()
{
    const std::vector<ModuleObservation> golden = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = golden;
    const ModuleInventoryEvidence evidence =
        completeEvidence(golden, postProofModules);

    std::vector<ModuleObservation> changed = golden;
    std::array<char, ModulePathSnapshotCharacters + 1u> oversizedPath {};
    for (char& character : oversizedPath)
        character = 'a';
    require(!changed[0].normalizedRelativePath.capture(std::string_view(
                oversizedPath.data(), oversizedPath.size())),
        "test setup failed to reject an oversized path snapshot");
    require(assess(evidence, changed).failure
            == InventoryFailure::InvalidObservationTextSnapshot,
        "an overflowed path snapshot was accepted");

    changed = golden;
    changed[0].normalizedModuleName.captureComplete = false;
    require(assess(evidence, changed).failure
            == InventoryFailure::InvalidObservationTextSnapshot,
        "an incomplete module-name snapshot was accepted");

    changed = golden;
    changed[0].identity.sha256.bytes[64u] = 'x';
    require(assess(evidence, changed).failure
            == InventoryFailure::InvalidObservationTextSnapshot,
        "a nonterminated SHA snapshot was accepted");

    std::vector<ModuleObservation> changedPostProof = golden;
    changedPostProof[0].normalizedRelativePath.captureComplete = false;
    require(assessModuleInventory(
                completeEvidence(golden, changedPostProof)).failure
            == InventoryFailure::PostProofObservationMismatch,
        "an invalid post-proof text snapshot compared equal");
}

void testRuntimeMappingsAreExactAndDisjoint()
{
    const std::vector<ModuleObservation> golden = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = golden;
    const ModuleInventoryEvidence evidence =
        completeEvidence(golden, postProofModules);

    std::vector<ModuleObservation> changed = golden;
    changed[0].runtimeModuleBase = 0u;
    require(assess(evidence, changed).failure
            == InventoryFailure::InvalidRuntimeMapping,
        "a zero runtime module base was accepted");
    changed = golden;
    changed[0].runtimeModuleBytes = 0u;
    require(assess(evidence, changed).failure
            == InventoryFailure::InvalidRuntimeMapping,
        "a zero runtime module extent was accepted");
    changed = golden;
    --changed[0].runtimeModuleBytes;
    require(assess(evidence, changed).failure
            == InventoryFailure::InvalidRuntimeMapping,
        "a runtime extent different from SizeOfImage was accepted");
    changed = golden;
    changed[0].runtimeModuleBase = 0xFFFFFFF0u;
    require(assess(evidence, changed).failure
            == InventoryFailure::InvalidRuntimeMapping,
        "a PE32 mapping extending beyond 4 GiB was accepted");

    changed = golden;
    changed[1].runtimeModuleBase = changed[0].runtimeModuleBase;
    require(assess(evidence, changed).failure
            == InventoryFailure::OverlappingRuntimeMappings,
        "overlapping live module mappings were accepted");

    changed = golden;
    changed.back().runtimeModuleBase = static_cast<std::uint32_t>(
        (1ull << 32u) - changed.back().runtimeModuleBytes);
    const std::vector<ModuleObservation> boundaryPostProof = changed;
    require(assessModuleInventory(
                completeEvidence(changed, boundaryPostProof)).overallAccepted,
        "a valid PE32 mapping ending exactly at 4 GiB was rejected");
}

void testEveryExactGameModuleRequiresMappedCodeProof()
{
    const std::vector<ModuleObservation> golden = completeModuleSet();
    std::size_t tested = 0u;
    for (std::size_t index = 0; index < golden.size(); ++index)
    {
        const ExpectedModule& expected = AllowedModules[index];
        if (expected.origin != ModuleOrigin::GameTree
            || !expected.exactIdentityRequired)
        {
            continue;
        }

        ++tested;
        std::vector<ModuleObservation> changed = golden;
        changed[index].loadedExecutableSectionsMatched = false;
        const std::vector<ModuleObservation> postProofModules = changed;
        const ModuleInventoryAssessment result = assessModuleInventory(
            completeEvidence(changed, postProofModules));
        require(result.failure
                == InventoryFailure::LoadedExecutableSectionsMismatch
                && !result.moduleSetAccepted
                && !result.overallAccepted,
            "an exact game-tree module without mapped-code proof was accepted");
    }
    require(tested == 10u,
        "not every exact game-tree mapped executable was exercised");
}

void testOverlaysAreExplicitlyProhibited()
{
    const std::vector<ModuleObservation> golden = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = golden;
    const ModuleInventoryEvidence evidence =
        completeEvidence(golden, postProofModules);

    std::vector<ModuleObservation> modules = golden;
    modules.back().origin = ModuleOrigin::SteamRuntime;
    modules.back().normalizedRelativePath = "gameoverlayrenderer.dll";
    modules.back().normalizedModuleName = "gameoverlayrenderer.dll";
    modules.back().identity = syntheticIdentity(500u);
    modules.back().runtimeModuleBytes = modules.back().identity.sizeOfImage;
    modules.back().authenticodeValid = true;
    ModuleInventoryAssessment result = assess(evidence, modules);
    require(result.failure == InventoryFailure::ProhibitedSteamOverlay
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "Steam render overlay was accepted");

    modules = golden;
    modules.back().origin = ModuleOrigin::WindowsRuntime;
    modules.back().normalizedRelativePath = "system32/nvspcap.dll";
    modules.back().normalizedModuleName = "nvspcap.dll";
    modules.back().identity = syntheticIdentity(501u);
    modules.back().runtimeModuleBytes = modules.back().identity.sizeOfImage;
    modules.back().authenticodeValid = true;
    result = assess(evidence, modules);
    require(result.failure == InventoryFailure::ProhibitedNvidiaCapture
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "NVIDIA capture overlay was accepted");

    const auto withExtraOverlay = [](
        const std::vector<ModuleObservation>& source,
        ModuleOrigin origin,
        std::string_view relativePath,
        std::string_view moduleName,
        std::size_t identityIndex) {
        std::vector<ModuleObservation> extra = source;
        ModuleObservation overlay = source.back();
        overlay.origin = origin;
        overlay.normalizedRelativePath = relativePath;
        overlay.normalizedModuleName = moduleName;
        overlay.identity = syntheticIdentity(identityIndex);
        overlay.runtimeModuleBytes = overlay.identity.sizeOfImage;
        overlay.authenticodeValid = true;
        extra.push_back(overlay);
        return extra;
    };

    std::vector<ModuleObservation> extra = withExtraOverlay(
        golden,
        ModuleOrigin::SteamRuntime,
        "gameoverlayrenderer.dll",
        "gameoverlayrenderer.dll",
        502u);
    result = assess(evidence, extra);
    require(result.failure == InventoryFailure::ProhibitedSteamOverlay
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "an extra Steam overlay was reduced to a primary count mismatch");

    extra = withExtraOverlay(
        golden,
        ModuleOrigin::WindowsRuntime,
        "system32/nvspcap.dll",
        "nvspcap.dll",
        503u);
    result = assess(evidence, extra);
    require(result.failure == InventoryFailure::ProhibitedNvidiaCapture
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "an extra NVIDIA overlay was reduced to a primary count mismatch");

    std::vector<ModuleObservation> postExtra = withExtraOverlay(
        golden,
        ModuleOrigin::SteamRuntime,
        "gameoverlayrenderer.dll",
        "gameoverlayrenderer.dll",
        504u);
    result = assessModuleInventory(completeEvidence(golden, postExtra));
    require(result.failure == InventoryFailure::ProhibitedSteamOverlay
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "an extra post-proof Steam overlay was reduced to a count mismatch");

    postExtra = withExtraOverlay(
        golden,
        ModuleOrigin::WindowsRuntime,
        "system32/nvspcap.dll",
        "nvspcap.dll",
        505u);
    result = assessModuleInventory(completeEvidence(golden, postExtra));
    require(result.failure == InventoryFailure::ProhibitedNvidiaCapture
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "an extra post-proof NVIDIA overlay was reduced to a count mismatch");

    extra = withExtraOverlay(
        golden,
        ModuleOrigin::SteamRuntime,
        "gameoverlayrenderer.dll",
        "gameoverlayrenderer.dll",
        506u);
    extra.front().normalizedModuleName.captureComplete = false;
    result = assess(evidence, extra);
    require(result.failure == InventoryFailure::InvalidObservationTextSnapshot
            && !result.overallAccepted,
        "an invalid primary name snapshot was scanned as an overlay");

    postExtra = withExtraOverlay(
        golden,
        ModuleOrigin::WindowsRuntime,
        "system32/nvspcap.dll",
        "nvspcap.dll",
        507u);
    postExtra.front().normalizedModuleName.captureComplete = false;
    result = assessModuleInventory(completeEvidence(golden, postExtra));
    require(result.failure == InventoryFailure::InvalidObservationTextSnapshot
            && !result.overallAccepted,
        "an invalid post-proof name snapshot was scanned as an overlay");

    std::vector<ModuleObservation> bothOverlays = withExtraOverlay(
        golden,
        ModuleOrigin::SteamRuntime,
        "gameoverlayrenderer.dll",
        "gameoverlayrenderer.dll",
        508u);
    bothOverlays = withExtraOverlay(
        bothOverlays,
        ModuleOrigin::WindowsRuntime,
        "system32/nvspcap.dll",
        "nvspcap.dll",
        509u);
    result = assess(evidence, bothOverlays);
    require(bothOverlays.size() == AllowedModuleCount + 2u
            && result.failure == InventoryFailure::ProhibitedSteamOverlay
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "the observed 126-module dual-overlay census became a count mismatch");

    std::vector<ModuleObservation> orderedDirty = golden;
    ModuleObservation benignExtra = golden.back();
    benignExtra.origin = ModuleOrigin::Unknown;
    benignExtra.normalizedRelativePath = "unrecognized_extra.dll";
    benignExtra.normalizedModuleName = "unrecognized_extra.dll";
    benignExtra.identity = syntheticIdentity(510u);
    benignExtra.runtimeModuleBytes = benignExtra.identity.sizeOfImage;
    benignExtra.authenticodeValid = true;
    orderedDirty.push_back(benignExtra);
    orderedDirty = withExtraOverlay(
        orderedDirty,
        ModuleOrigin::WindowsRuntime,
        "system32/nvspcap.dll",
        "nvspcap.dll",
        511u);
    result = assess(evidence, orderedDirty);
    require(orderedDirty.size() == AllowedModuleCount + 2u
            && result.failure == InventoryFailure::ProhibitedNvidiaCapture
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "a non-overlay extra before NVIDIA capture masked the prohibition");

    std::vector<ModuleObservation> mixedCase = golden;
    mixedCase.push_back(benignExtra);
    mixedCase = withExtraOverlay(
        mixedCase,
        ModuleOrigin::SteamRuntime,
        "gameoverlayrenderer.dll",
        "GameOverlayRenderer.dll",
        512u);
    result = assess(evidence, mixedCase);
    require(mixedCase.size() == AllowedModuleCount + 2u
            && result.failure == InventoryFailure::ProhibitedSteamOverlay
            && !result.overlaysAbsent
            && !result.overallAccepted,
        "mixed-case Toolhelp overlay basename became a count mismatch");

    std::vector<ModuleObservation> lookalike = withExtraOverlay(
        golden,
        ModuleOrigin::SteamRuntime,
        "notgameoverlayrenderer.dll",
        "notGameOverlayRenderer.dll",
        513u);
    result = assess(evidence, lookalike);
    require(result.failure == InventoryFailure::PrimaryModuleCountMismatch
            && !result.overallAccepted,
        "a non-exact overlay basename was treated as prohibited");
}

void testEveryProxyPolicyGate()
{
    using Member = bool TransparentProxyPolicyEvidence::*;
    constexpr Member members[] {
        &TransparentProxyPolicyEvidence::d3dExactTransparentBuildIdentityMatched,
        &TransparentProxyPolicyEvidence::d3dMappedExecutableSectionsMatched,
        &TransparentProxyPolicyEvidence::d3dActivationSourceFuseFalse,
        &TransparentProxyPolicyEvidence::d3dProductIntegrationSourceFuseFalse,
        &TransparentProxyPolicyEvidence::d3dTransparentForwardingVerified,
        &TransparentProxyPolicyEvidence::dinputExactTransparentBuildIdentityMatched,
        &TransparentProxyPolicyEvidence::dinputMappedExecutableSectionsMatched,
        &TransparentProxyPolicyEvidence::dinputProductionSourceFuseFalse,
        &TransparentProxyPolicyEvidence::dinputProductIntegrationSourceFuseFalse,
        &TransparentProxyPolicyEvidence::dinputTransparentForwardingVerified,
        &TransparentProxyPolicyEvidence::xinputExactTransparentBuildIdentityMatched,
        &TransparentProxyPolicyEvidence::xinputMappedExecutableSectionsMatched,
        &TransparentProxyPolicyEvidence::xinputProductionSourceFuseFalse,
        &TransparentProxyPolicyEvidence::xinputProductIntegrationSourceFuseFalse,
        &TransparentProxyPolicyEvidence::xinputTransparentForwardingVerified,
        &TransparentProxyPolicyEvidence::noConfigurationOrSharedMemoryBypass
    };

    const std::vector<ModuleObservation> modules = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = modules;
    for (Member member : members)
    {
        ModuleInventoryEvidence evidence =
            completeEvidence(modules, postProofModules);
        evidence.proxyPolicy.*member = false;
        const ModuleInventoryAssessment result = assess(evidence, modules);
        require(result.moduleSetAccepted
                && !result.proxyPoliciesAccepted
                && result.jipProofAccepted
                && result.showOffProofAccepted
                && !result.overallAccepted
                && result.failure == InventoryFailure::ProxyPolicyUnverified,
            "an incomplete transparent-proxy policy was accepted");
    }
}

void testJipAndShowOffProofsAreIndependent()
{
    const std::vector<ModuleObservation> modules = completeModuleSet();
    const std::vector<ModuleObservation> postProofModules = modules;

    ModuleInventoryEvidence evidence =
        completeEvidence(modules, postProofModules);
    evidence.jipProof.disposition = ProofDisposition::NotRun;
    ModuleInventoryAssessment result = assess(evidence, modules);
    require(result.moduleSetAccepted
            && result.proxyPoliciesAccepted
            && !result.jipProofAccepted
            && result.showOffProofAccepted
            && result.failure == InventoryFailure::JipProofUnverified,
        "unverified JIP proof was not independently rejected");

    using JipMember = bool JipCompatibilityProof::*;
    constexpr JipMember jipMembers[] {
        &JipCompatibilityProof::loadedExecutableSectionsMatched,
        &JipCompatibilityProof::exactOwnedPatchInventoryMatched,
        &JipCompatibilityProof::exactHookTargetMatched,
        &JipCompatibilityProof::normalizedRetailManifestMatched,
        &JipCompatibilityProof::noUnexpectedExecutableWrites
    };
    for (JipMember member : jipMembers)
    {
        evidence = completeEvidence(modules, postProofModules);
        evidence.jipProof.*member = false;
        result = assess(evidence, modules);
        require(!result.jipProofAccepted
                && result.showOffProofAccepted
                && result.failure == InventoryFailure::JipProofUnverified,
            "incomplete JIP proof was accepted");
    }

    evidence = completeEvidence(modules, postProofModules);
    evidence.showOffProof.disposition = ProofDisposition::Failed;
    result = assess(evidence, modules);
    require(result.jipProofAccepted
            && !result.showOffProofAccepted
            && result.failure == InventoryFailure::ShowOffProofUnverified,
        "failed ShowOff proof was not independently rejected");

    using ShowOffMember = bool ShowOffCompatibilityProof::*;
    constexpr ShowOffMember showOffMembers[] {
        &ShowOffCompatibilityProof::loadedExecutableSectionsMatched,
        &ShowOffCompatibilityProof::exactOwnedPatchInventoryMatched,
        &ShowOffCompatibilityProof::protectedRendererBytesMatched,
        &ShowOffCompatibilityProof::noUnexpectedExecutableWrites
    };
    for (ShowOffMember member : showOffMembers)
    {
        evidence = completeEvidence(modules, postProofModules);
        evidence.showOffProof.*member = false;
        result = assess(evidence, modules);
        require(result.jipProofAccepted
                && !result.showOffProofAccepted
                && result.failure == InventoryFailure::ShowOffProofUnverified,
            "incomplete ShowOff proof was accepted");
    }
}
}

int main()
{
    testOwnedAsciiSnapshots();
    testCensusShape();
    testDefaultAndCompleteEvidence();
    testEnumerationEnvelope();
    testTemporalEvidenceBindings();
    testPostProofReenumerationExactlyMatches();
    testEveryModuleIsRequiredAndUnique();
    testEveryExactIdentityField();
    testObservationTextSnapshotsFailClosed();
    testRuntimeMappingsAreExactAndDisjoint();
    testEveryExactGameModuleRequiresMappedCodeProof();
    testOverlaysAreExplicitlyProhibited();
    testEveryProxyPolicyGate();
    testJipAndShowOffProofsAreIndependent();
    std::cout << "module inventory acceptance contract PASS\n";
    return 0;
}
