#include "../kernel/transport/presentation_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

fnvxr::kernel::ValidatedRuntimeConfig validated(
    fnvxr::kernel::EyeTransport transport)
{
    fnvxr::kernel::RuntimeConfig candidate {};
    candidate.performance.eyeTransport = transport;
    fnvxr::kernel::ConfigValidationResult result =
        fnvxr::kernel::validateRuntimeConfig(candidate);
    if (!result || !result.config)
        std::abort();
    return *result.config;
}

fnvxr::kernel::presentation::RuntimeSnapshot snapshot(
    fnvxr::kernel::presentation::RuntimePhase phase,
    fnvxr::kernel::presentation::UiClassification ui =
        fnvxr::kernel::presentation::UiClassification::None)
{
    return { 42u, phase, ui, true };
}
}

int main()
{
    namespace presentation = fnvxr::kernel::presentation;
    namespace transport = fnvxr::kernel::transport;

    const fnvxr::kernel::ValidatedRuntimeConfig gpu =
        validated(fnvxr::kernel::EyeTransport::GpuColorV5);
    const transport::AuthorityDecision gpuWorld =
        transport::resolvePresentationAuthority(
            gpu, snapshot(presentation::RuntimePhase::Running));
    if (!gpuWorld
        || gpuWorld.authority != transport::PresentationAuthority::GpuProductKernel
        || gpuWorld.presentation != transport::RuntimePresentation::World)
        return fail("GPU world evidence must route only to ProductKernel");

    const transport::AuthorityDecision gpuMenu =
        transport::resolvePresentationAuthority(
            gpu,
            snapshot(presentation::RuntimePhase::Running,
                presentation::UiClassification::Blocking));
    if (!gpuMenu
        || gpuMenu.authority != transport::PresentationAuthority::GpuProductKernel
        || gpuMenu.presentation != transport::RuntimePresentation::BlockingUi)
        return fail("GPU menu evidence must retain ProductKernel authority");

    const fnvxr::kernel::ValidatedRuntimeConfig cpu =
        validated(fnvxr::kernel::EyeTransport::CpuEngineCenter);
    const transport::AuthorityDecision cpuPipBoy =
        transport::resolvePresentationAuthority(
            cpu,
            snapshot(presentation::RuntimePhase::Running,
                presentation::UiClassification::PipBoySpatial));
    if (!cpuPipBoy
        || cpuPipBoy.authority != transport::PresentationAuthority::CpuEngineCenter
        || cpuPipBoy.presentation
            != transport::RuntimePresentation::WorldWithSpatialPipBoy)
        return fail("CPU Pip-Boy world evidence must route only to engine-center policy");

    const transport::AuthorityDecision cpuLoading =
        transport::resolvePresentationAuthority(
            cpu, snapshot(presentation::RuntimePhase::Loading));
    if (!cpuLoading
        || cpuLoading.authority != transport::PresentationAuthority::CpuEngineCenter
        || cpuLoading.presentation != transport::RuntimePresentation::Loading)
        return fail("loading evidence must retain the configured authority");

    presentation::RuntimeSnapshot stale =
        snapshot(presentation::RuntimePhase::Running);
    stale.fresh = false;
    const transport::AuthorityDecision staleDecision =
        transport::resolvePresentationAuthority(gpu, stale);
    if (staleDecision
        || staleDecision.authority != transport::PresentationAuthority::None
        || staleDecision.failure != transport::AuthorityFailure::RuntimeUnavailable)
        return fail("stale runtime evidence must fail closed");

    presentation::RuntimeSnapshot zeroSample =
        snapshot(presentation::RuntimePhase::Running);
    zeroSample.sample = 0u;
    if (transport::resolvePresentationAuthority(cpu, zeroSample))
        return fail("zero runtime identity must fail closed");

    const transport::AuthorityDecision contradictory =
        transport::resolvePresentationAuthority(
            gpu,
            snapshot(presentation::RuntimePhase::Loading,
                presentation::UiClassification::PipBoySpatial));
    if (contradictory
        || contradictory.failure
            != transport::AuthorityFailure::ContradictoryRuntimeEvidence)
        return fail("contradictory loading and spatial-menu evidence must fail closed");

    const transport::AuthorityDecision invalidPhase =
        transport::resolvePresentationAuthority(
            gpu,
            snapshot(static_cast<presentation::RuntimePhase>(255)));
    if (invalidPhase
        || invalidPhase.failure != transport::AuthorityFailure::InvalidRuntimePhase)
        return fail("unknown runtime phase values must fail closed");

    const transport::AuthorityDecision invalidUi =
        transport::resolvePresentationAuthority(
            cpu,
            snapshot(presentation::RuntimePhase::Running,
                static_cast<presentation::UiClassification>(255)));
    if (invalidUi
        || invalidUi.failure
            != transport::AuthorityFailure::InvalidUiClassification)
        return fail("unknown UI classification values must fail closed");

    const transport::AuthorityDecision repeated =
        transport::resolvePresentationAuthority(
            cpu,
            snapshot(presentation::RuntimePhase::Running,
                presentation::UiClassification::PipBoySpatial));
    if (repeated.authority != cpuPipBoy.authority
        || repeated.presentation != cpuPipBoy.presentation
        || repeated.failure != cpuPipBoy.failure)
        return fail("authority resolution must be deterministic");

    return EXIT_SUCCESS;
}
