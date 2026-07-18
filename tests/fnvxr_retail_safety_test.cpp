#include "fnvxr_retail_safety.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}
}

int main()
{
    using namespace fnvxr::safety;

    if (!compatibleNvseRuntime(false, 1, SupportedRuntimeVersion))
        return fail("the exact supported 1.4.0.525 runtime must be accepted");
    if (compatibleNvseRuntime(true, 1, SupportedRuntimeVersion))
        return fail("the editor must never be accepted as the retail runtime");
    if (compatibleNvseRuntime(false, 0, SupportedRuntimeVersion))
        return fail("an invalid NVSE interface version must be rejected");
    if (compatibleNvseRuntime(false, 1, SupportedRuntimeVersionNoGore))
        return fail("the unproven no-gore executable must be rejected");
    if (compatibleNvseRuntime(false, 1, 1))
        return fail("a merely nonzero runtime version must not be accepted");
    if (retailMutationAllowed(true) || retailMutationAllowed(false))
        return fail("configuration cannot bypass the source-level retail mutation fuse");

    std::cout << "fnvxr retail mutation safety gate PASS\n";
    return EXIT_SUCCESS;
}
