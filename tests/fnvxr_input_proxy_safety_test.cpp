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
        true
    };

    if (!assessProductionInputAuthorization(true, true, true, complete))
        return fail("synthetic complete production input authorization was rejected");
    if (assessProductionInputAuthorization(false, true, true, complete)
        || assessProductionInputAuthorization(true, false, true, complete)
        || assessProductionInputAuthorization(true, true, false, complete))
    {
        return fail("a source or request gate could be bypassed");
    }

    for (int missing = 0; missing < 5; ++missing)
    {
        ProductionInputAuthorizationEvidence incomplete = complete;
        switch (missing)
        {
            case 0: incomplete.exactRetailExecutable = false; break;
            case 1: incomplete.exactCompatibilityModuleInventory = false; break;
            case 2: incomplete.authoritativeInputProducer = false; break;
            case 3: incomplete.sharedProtocolMatched = false; break;
            case 4: incomplete.uiModeTransitionsComplete = false; break;
            default: break;
        }
        if (assessProductionInputAuthorization(true, true, true, incomplete))
            return fail("incomplete production input evidence was accepted");
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
