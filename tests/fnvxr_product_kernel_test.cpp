#include "../kernel/product_kernel.h"

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
    fnvxr::kernel::RuntimeConfig config {};
    auto kernel = fnvxr::kernel::ProductKernel::create(config);
    if (!kernel || kernel->config().get().performance.targetRefreshHz != 90)
        return fail("valid defaults did not create the product kernel");

    config.performance.maximumFramesInFlight = 2;
    if (fnvxr::kernel::ProductKernel::create(config))
        return fail("invalid configuration crossed the kernel boundary");

    fnvxr::kernel::RuntimeConfig separatelyOwnedRaw {};
    const auto validated = fnvxr::kernel::validateRuntimeConfig(
        separatelyOwnedRaw);
    if (!validated || !validated.config)
        return fail("validated construction fixture did not validate");
    auto validatedKernel = fnvxr::kernel::ProductKernel::fromValidated(
        *validated.config);
    separatelyOwnedRaw.performance.targetRefreshHz = 72;
    if (validatedKernel.config().get().performance.targetRefreshHz != 90)
        return fail("kernel did not own its validated configuration value");

    using namespace fnvxr::kernel::presentation;
    PresentationInput input {};
    input.runtime = { 3, RuntimePhase::Running, UiClassification::None, true };
    const SourceKey source { 7, 8, 9 };
    input.world.source = source;
    input.world.runtimeSample = 3;
    input.world.stereoIdentity = { source, source, true };
    input.world.gpu = { source, true, true, true, true, true };
    input.world.retail = { true, true, true, true, true, true, true, true, true };
    input.world.completeStereoPair = true;
    input.world.distinctEyeViews = true;
    input.world.runtimeLineageVerified = true;
    input.world.fresh = true;
    input.poseHistory = { { 7, 8, 9 }, true };
    input.trackedRigReady = true;
    const PresentationDecision decision = kernel->present(input);
    if (decision.mode != PresentationMode::WorldStereo
        || !decision.spatialRigMayRender)
    {
        return fail("coherent world state did not cross the product entry point");
    }
    return EXIT_SUCCESS;
}
