#pragma once

#include "fnvxr_module_inventory_acceptance.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace fnvxr::module_census
{
enum class CensusPhase : std::uint8_t
{
    CollectingPrimary = 0,
    AwaitingProofCollection,
    CollectingPostProof,
    ReadyToSeal,
    Sealed,
    Failed
};

enum class CensusFailure : std::uint8_t
{
    None = 0,
    InvalidProcessIdentity,
    InvalidGeneration,
    WrongPhase,
    EmptySnapshot,
    CapacityExceeded,
    TargetNot32Bit,
    InvalidProofCompletionOrdinal,
    InvalidPostProofOrdinal
};

// Platform-neutral ownership and sequencing core for a later read-only Win32
// adapter. It cannot grant production authority: an out-of-process census
// cannot prove transaction adjacency, so transactionBeginOrdinal is fixed at
// zero and the unimplemented runtime proofs are failed closed.
class OwnedWindowsModuleCensus final
{
public:
    static constexpr std::size_t Capacity =
        inventory::MaximumDiagnosticModuleCount;
    static constexpr bool ProductionAuthorizationAvailable = false;

    OwnedWindowsModuleCensus(
        inventory::ProcessCreationIdentity processIdentity,
        std::uint64_t generation)
        : primary_(std::make_unique<inventory::ModuleObservation[]>(Capacity)),
          postProof_(std::make_unique<inventory::ModuleObservation[]>(Capacity)),
          processIdentity_(processIdentity),
          generation_(generation)
    {
        static_assert(Capacity == 512u);
        if (!inventory::processIdentityObserved(processIdentity_))
            reject(CensusFailure::InvalidProcessIdentity);
        else if (generation_ == 0u)
            reject(CensusFailure::InvalidGeneration);
    }

    OwnedWindowsModuleCensus(const OwnedWindowsModuleCensus&) = delete;
    OwnedWindowsModuleCensus& operator=(const OwnedWindowsModuleCensus&) = delete;
    OwnedWindowsModuleCensus(OwnedWindowsModuleCensus&&) = delete;
    OwnedWindowsModuleCensus& operator=(OwnedWindowsModuleCensus&&) = delete;

    bool appendPrimary(
        const inventory::ModuleObservation& observation) noexcept
    {
        if (phase_ != CensusPhase::CollectingPrimary)
            return reject(CensusFailure::WrongPhase);
        if (primaryCount_ == Capacity)
            return reject(CensusFailure::CapacityExceeded);
        primary_[primaryCount_++] = observation;
        return true;
    }

    bool completePrimaryToolhelp32(bool targetProcessIs32Bit) noexcept
    {
        if (phase_ != CensusPhase::CollectingPrimary)
            return reject(CensusFailure::WrongPhase);
        if (primaryCount_ == 0u)
            return reject(CensusFailure::EmptySnapshot);
        if (!targetProcessIs32Bit)
            return reject(CensusFailure::TargetNot32Bit);
        phase_ = CensusPhase::AwaitingProofCollection;
        return true;
    }

    bool completeProofCollection(std::uint64_t completionOrdinal) noexcept
    {
        if (phase_ != CensusPhase::AwaitingProofCollection)
            return reject(CensusFailure::WrongPhase);
        if (completionOrdinal == 0u)
            return reject(CensusFailure::InvalidProofCompletionOrdinal);
        proofCompletionOrdinal_ = completionOrdinal;
        phase_ = CensusPhase::CollectingPostProof;
        return true;
    }

    bool appendPostProof(
        const inventory::ModuleObservation& observation) noexcept
    {
        if (phase_ != CensusPhase::CollectingPostProof)
            return reject(CensusFailure::WrongPhase);
        if (postProofCount_ == Capacity)
            return reject(CensusFailure::CapacityExceeded);
        postProof_[postProofCount_++] = observation;
        return true;
    }

    bool completePostProofToolhelp32(
        std::uint64_t enumerationOrdinal) noexcept
    {
        if (phase_ != CensusPhase::CollectingPostProof)
            return reject(CensusFailure::WrongPhase);
        if (postProofCount_ == 0u)
            return reject(CensusFailure::EmptySnapshot);
        if (enumerationOrdinal == 0u
            || enumerationOrdinal <= proofCompletionOrdinal_)
        {
            return reject(CensusFailure::InvalidPostProofOrdinal);
        }
        postProofEnumerationOrdinal_ = enumerationOrdinal;
        phase_ = CensusPhase::ReadyToSeal;
        return true;
    }

    bool seal() noexcept
    {
        if (phase_ != CensusPhase::ReadyToSeal)
            return reject(CensusFailure::WrongPhase);

        evidence_ = {};
        evidence_.enumerationComplete = true;
        evidence_.usedToolhelp32ModuleAndModule32 = true;
        evidence_.targetProcessIs32Bit = true;
        evidence_.transactionProcessIdentity = processIdentity_;
        evidence_.transactionGeneration = generation_;
        evidence_.primaryProcessIdentity = processIdentity_;
        evidence_.primaryEvidenceGeneration = generation_;
        evidence_.modules = primary_.get();
        evidence_.moduleCount = primaryCount_;

        evidence_.proxyPolicy.processIdentity = processIdentity_;
        evidence_.proxyPolicy.evidenceGeneration = generation_;
        evidence_.jipProof.processIdentity = processIdentity_;
        evidence_.jipProof.evidenceGeneration = generation_;
        evidence_.jipProof.disposition = inventory::ProofDisposition::Failed;
        evidence_.showOffProof.processIdentity = processIdentity_;
        evidence_.showOffProof.evidenceGeneration = generation_;
        evidence_.showOffProof.disposition = inventory::ProofDisposition::Failed;

        evidence_.postProofEnumerationComplete = true;
        evidence_.postProofUsedToolhelp32ModuleAndModule32 = true;
        evidence_.postProofProcessIdentity = processIdentity_;
        evidence_.postProofEvidenceGeneration = generation_;
        evidence_.postProofModules = postProof_.get();
        evidence_.postProofModuleCount = postProofCount_;
        evidence_.proofCompletionOrdinal = proofCompletionOrdinal_;
        evidence_.postProofEnumerationOrdinal = postProofEnumerationOrdinal_;
        evidence_.transactionBeginOrdinal = 0u;

        phase_ = CensusPhase::Sealed;
        return true;
    }

    [[nodiscard]] const inventory::ModuleInventoryEvidence* sealedView()
        const noexcept
    {
        return phase_ == CensusPhase::Sealed ? &evidence_ : nullptr;
    }

    [[nodiscard]] CensusPhase phase() const noexcept
    {
        return phase_;
    }

    [[nodiscard]] CensusFailure failure() const noexcept
    {
        return failure_;
    }

    [[nodiscard]] constexpr bool productionAuthorized() const noexcept
    {
        return false;
    }

private:
    bool reject(CensusFailure failure) noexcept
    {
        // Once sealed, failed mutation attempts cannot alter either the state
        // or the pointer-stable evidence view.
        if (phase_ != CensusPhase::Sealed)
        {
            failure_ = failure;
            phase_ = CensusPhase::Failed;
        }
        return false;
    }

    std::unique_ptr<inventory::ModuleObservation[]> primary_;
    std::unique_ptr<inventory::ModuleObservation[]> postProof_;
    inventory::ProcessCreationIdentity processIdentity_ {};
    std::uint64_t generation_ = 0;
    std::size_t primaryCount_ = 0;
    std::size_t postProofCount_ = 0;
    std::uint64_t proofCompletionOrdinal_ = 0;
    std::uint64_t postProofEnumerationOrdinal_ = 0;
    inventory::ModuleInventoryEvidence evidence_ {};
    CensusPhase phase_ = CensusPhase::CollectingPrimary;
    CensusFailure failure_ = CensusFailure::None;
};
}
