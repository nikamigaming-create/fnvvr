#include "fnvxr_retail_observation_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::engine::compatibility::RetailCompatibilityFailure;
using fnvxr::engine::compatibility::RetailCompatibilityProof;

RetailCompatibilityProof completeProof()
{
    RetailCompatibilityProof proof {};
    proof.evidence = {
        true, true, true, true, true, true,
        true, true, true, true,
    };
    proof.diagnostics.runtimeImageBase = 0x400000u;
    proof.diagnostics.processId = 42u;
    proof.diagnostics.processCreationTime100ns = 99u;
    proof.failure = RetailCompatibilityFailure::None;
    proof.compatible = true;
    return proof;
}

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using fnvxr::engine::retailObservationAuthorized;
    RetailCompatibilityProof proof = completeProof();
    require(retailObservationAuthorized(proof),
        "complete same-process compatibility proof rejected observation");

    for (int missing = 0; missing < 10; ++missing)
    {
        proof = completeProof();
        bool* fields[] = {
            &proof.evidence.retailExecutableIdentityMatched,
            &proof.evidence.moduleSnapshotStable,
            &proof.evidence.jip5730ExactOrAbsent,
            &proof.evidence.showOff184ExactOrAbsent,
            &proof.evidence.renderFirstPersonStockOrJipNormalized,
            &proof.evidence.protectedCoreBodiesMatched,
            &proof.evidence.protectedFunctionInventoryMatched,
            &proof.evidence.protectedVtableSlotsMatched,
            &proof.evidence.protectedVtableBlocksMatched,
            &proof.evidence.synchronousSameProcess,
        };
        *fields[missing] = false;
        require(!retailObservationAuthorized(proof),
            "incomplete compatibility evidence authorized observation");
    }

    proof = completeProof();
    proof.compatible = false;
    require(!retailObservationAuthorized(proof),
        "incompatible process authorized observation");
    proof = completeProof();
    proof.failure = RetailCompatibilityFailure::ProtectedFunctionMismatch;
    require(!retailObservationAuthorized(proof),
        "failed compatibility proof authorized observation");
    proof = completeProof();
    proof.diagnostics.runtimeImageBase = 0u;
    require(!retailObservationAuthorized(proof),
        "unbound proof authorized observation");

    std::cout << "retail observation authority gate passed\n";
    return 0;
}
