#include "../host/fnvxr_eye_readback_policy.h"

#include <cstdlib>

int main()
{
    using fnvxr::host::eyeReadbackRequired;
    return !eyeReadbackRequired(false, false)
            && eyeReadbackRequired(true, false)
            && eyeReadbackRequired(false, true)
            && eyeReadbackRequired(true, true)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
