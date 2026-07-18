#include "fnvxr_input_proxy_safety.h"

#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}
}

int main()
{
    using namespace fnvxr::input_proxy;

    constexpr ProductionInputAuthorizationEvidence complete {
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true
    };

    if (!assessProductionInputAuthorization(true, true, true, complete)
        || !assessProductionInputAuthorizationDecision(
            true,
            true,
            true,
            complete).complete())
    {
        return fail("synthetic complete production input authorization was rejected");
    }
    if (assessProductionInputAuthorization(false, true, true, complete)
        || assessProductionInputAuthorization(true, false, true, complete)
        || assessProductionInputAuthorization(true, true, false, complete))
    {
        return fail("a source or request gate could be bypassed");
    }

    constexpr ProductionInputAuthorizationFailure expectedFailures[] = {
        ProductionInputAuthorizationFailure::ExactRetailExecutableUnverified,
        ProductionInputAuthorizationFailure::CompatibilityInventoryUnverified,
        ProductionInputAuthorizationFailure::InputProducerUnauthenticated,
        ProductionInputAuthorizationFailure::ProducerProcessIdentityUnbound,
        ProductionInputAuthorizationFailure::ProducerEpochUnavailable,
        ProductionInputAuthorizationFailure::SharedProtocolUnmatched,
        ProductionInputAuthorizationFailure::RuntimeStateSampleUnmatched,
        ProductionInputAuthorizationFailure::UiModeTransitionsIncomplete,
        ProductionInputAuthorizationFailure::ConfigurationFreeMappingIncomplete,
        ProductionInputAuthorizationFailure::ExclusiveInputOwnershipUnproven,
        ProductionInputAuthorizationFailure::StaleProducerNeutralizationIncomplete,
    };
    for (int missing = 0; missing < 11; ++missing)
    {
        ProductionInputAuthorizationEvidence incomplete = complete;
        switch (missing)
        {
            case 0: incomplete.exactRetailExecutable = false; break;
            case 1: incomplete.exactCompatibilityModuleInventory = false; break;
            case 2: incomplete.authoritativeInputProducer = false; break;
            case 3: incomplete.producerProcessIdentityBound = false; break;
            case 4: incomplete.producerEpochCurrent = false; break;
            case 5: incomplete.sharedProtocolMatched = false; break;
            case 6: incomplete.runtimeStateSampleMatched = false; break;
            case 7: incomplete.uiModeTransitionsComplete = false; break;
            case 8: incomplete.configurationFreeProductMapping = false; break;
            case 9: incomplete.exclusiveRetailInputOwnership = false; break;
            case 10: incomplete.staleProducerNeutralizationComplete = false; break;
            default: break;
        }
        const ProductionInputAuthorizationDecision decision =
            assessProductionInputAuthorizationDecision(
                true,
                true,
                true,
                incomplete);
        if (decision.complete()
            || decision.failure != expectedFailures[missing]
            || assessProductionInputAuthorization(
                true,
                true,
                true,
                incomplete))
        {
            return fail("incomplete production input evidence was accepted");
        }
    }

    if (assessProductionInputAuthorizationDecision(
            false,
            true,
            true,
            complete).failure
            != ProductionInputAuthorizationFailure::ProductionProofIncomplete
        || assessProductionInputAuthorizationDecision(
            true,
            false,
            true,
            complete).failure
            != ProductionInputAuthorizationFailure::ProductControllerNotIntegrated
        || assessProductionInputAuthorizationDecision(
            true,
            true,
            false,
            complete).failure
            != ProductionInputAuthorizationFailure::NotRequested)
    {
        return fail("input authorization source/request failure diagnosis changed");
    }

    if (ProductionInputMutationProofComplete
        || ProductInputControllerIntegrated
        || productionInputMutationAuthorized(true, complete)
        || productionInputMutationAuthorizedForCurrentBuild())
    {
        return fail("shipping input mutation is not fail-closed");
    }

    std::cout << "input proxy production authorization is fail-closed\n";
    return 0;
}
