#include "fnvxr_openxr_live_authority.h"

#include <array>
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
    using namespace fnvxr::host;

    static_assert(MaximumProductHostFrames == 7200u);
    static_assert(
        CompiledProductOpenXrInitializationAuthorization.authorized());

    if (!CompiledProductOpenXrInitializationAuthorization.authorized())
        return fail("compiled product OpenXR initialization must be authorized");

    const ProductOpenXrInitializationProof complete {
        MaximumProductHostFrames,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
    };
    if (!assessProductOpenXrInitialization(complete).authorized())
        return fail("complete integration evidence was rejected");

    const std::array<ProductOpenXrInitializationProof, 10u> incomplete {{
        { 0u, true, true, true, true, true, true, true, true },
        { MaximumProductHostFrames + 1u, true, true, true, true, true, true, true, true },
        { MaximumProductHostFrames, false, true, true, true, true, true, true, true },
        { MaximumProductHostFrames, true, false, true, true, true, true, true, true },
        { MaximumProductHostFrames, true, true, false, true, true, true, true, true },
        { MaximumProductHostFrames, true, true, true, false, true, true, true, true },
        { MaximumProductHostFrames, true, true, true, true, false, true, true, true },
        { MaximumProductHostFrames, true, true, true, true, true, false, true, true },
        { MaximumProductHostFrames, true, true, true, true, true, true, false, true },
        { MaximumProductHostFrames, true, true, true, true, true, true, true, false },
    }};
    for (const ProductOpenXrInitializationProof& proof : incomplete)
    {
        if (assessProductOpenXrInitialization(proof).authorized())
            return fail("one missing initialization fact must fail closed");
    }

    if (assessProductOpenXrInitialization(incomplete[2u]).failure
        != ProductOpenXrInitializationFailure::FiniteFrameArgumentNotEnforced)
    {
        return fail("authorization did not preserve the exact failure reason");
    }

    std::cout << "fnvxr product OpenXR initialization authority PASS\n";
    return EXIT_SUCCESS;
}
