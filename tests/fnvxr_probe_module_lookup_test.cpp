#include "fnvxr_probe_module_lookup.h"

#include <cstdlib>
#include <iostream>

namespace
{
void expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using fnvxr::probe::ModuleLookupResult;
    using fnvxr::probe::compatibilityModuleProofComplete;

    expect(
        !compatibilityModuleProofComplete(ModuleLookupResult::Found),
        "a present unverified compatibility module must fail closed");
    expect(
        compatibilityModuleProofComplete(ModuleLookupResult::NotFound),
        "a completed enumeration proving the module absent may pass");
    expect(
        !compatibilityModuleProofComplete(ModuleLookupResult::EnumerationFailed),
        "an enumeration failure must not be treated as module absence");

    std::cout << "fnvxr_probe_module_lookup_test: PASS\n";
    return EXIT_SUCCESS;
}
