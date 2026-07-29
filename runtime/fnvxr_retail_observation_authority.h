#pragma once

#include "fnvxr_retail_compatibility_proof.h"

namespace fnvxr::engine
{
// Menu/runtime observation is deliberately narrower than world mutation.
// It requires the exact same-process dual-pass compatibility proof, but it
// does not require live world objects that cannot exist at the main menu.
constexpr bool retailObservationAuthorized(
    const compatibility::RetailCompatibilityProof& proof) noexcept
{
    return proof.compatible
        && proof.failure == compatibility::RetailCompatibilityFailure::None
        && proof.evidence.retailExecutableIdentityMatched
        && proof.evidence.moduleSnapshotStable
        && proof.evidence.jip5730ExactOrAbsent
        && proof.evidence.johnnyGuitar528ExactOrAbsent
        && proof.evidence.showOff184ExactOrAbsent
        && proof.evidence.renderFirstPersonStockOrJipNormalized
        && proof.evidence.protectedCoreBodiesMatched
        && proof.evidence.protectedFunctionInventoryMatched
        && proof.evidence.protectedVtableSlotsMatched
        && proof.evidence.protectedVtableBlocksMatched
        && proof.evidence.synchronousSameProcess
        && proof.diagnostics.runtimeImageBase != 0u
        && proof.diagnostics.processId != 0u
        && proof.diagnostics.processCreationTime100ns != 0u;
}
}
