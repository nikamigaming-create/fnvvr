#include "presentation_authority.h"

namespace fnvxr::kernel::transport
{
namespace
{
AuthorityDecision failure(AuthorityFailure reason) noexcept
{
    return { PresentationAuthority::None, RuntimePresentation::None, reason };
}

bool knownUi(presentation::UiClassification ui) noexcept
{
    using Ui = presentation::UiClassification;
    return ui == Ui::None || ui == Ui::Blocking
        || ui == Ui::PipBoySpatial || ui == Ui::BlockingWithPipBoy;
}

RuntimePresentation classify(
    const presentation::RuntimeSnapshot& runtime,
    AuthorityFailure& failureReason) noexcept
{
    using Phase = presentation::RuntimePhase;
    using Ui = presentation::UiClassification;

    if (runtime.phase == Phase::Unknown)
    {
        failureReason = AuthorityFailure::InvalidRuntimePhase;
        return RuntimePresentation::None;
    }
    if (!knownUi(runtime.ui))
    {
        failureReason = AuthorityFailure::InvalidUiClassification;
        return RuntimePresentation::None;
    }
    if (runtime.phase == Phase::Loading)
    {
        if (runtime.ui == Ui::PipBoySpatial
            || runtime.ui == Ui::BlockingWithPipBoy)
        {
            failureReason = AuthorityFailure::ContradictoryRuntimeEvidence;
            return RuntimePresentation::None;
        }
        return RuntimePresentation::Loading;
    }
    if (runtime.phase != Phase::Running)
    {
        failureReason = AuthorityFailure::InvalidRuntimePhase;
        return RuntimePresentation::None;
    }
    if (runtime.ui == Ui::Blocking || runtime.ui == Ui::BlockingWithPipBoy)
        return RuntimePresentation::BlockingUi;
    if (runtime.ui == Ui::PipBoySpatial)
        return RuntimePresentation::WorldWithSpatialPipBoy;
    return RuntimePresentation::World;
}
}

AuthorityDecision::operator bool() const noexcept
{
    return authority != PresentationAuthority::None
        && presentation != RuntimePresentation::None
        && failure == AuthorityFailure::None;
}

AuthorityDecision resolvePresentationAuthority(
    const ValidatedRuntimeConfig& config,
    const presentation::RuntimeSnapshot& runtime) noexcept
{
    if (!runtime.fresh || runtime.sample == 0u)
        return failure(AuthorityFailure::RuntimeUnavailable);

    AuthorityFailure failureReason = AuthorityFailure::None;
    const RuntimePresentation presentation = classify(runtime, failureReason);
    if (failureReason != AuthorityFailure::None)
        return failure(failureReason);

    switch (config.get().performance.eyeTransport)
    {
    case EyeTransport::GpuColorV5:
        return { PresentationAuthority::GpuProductKernel,
            presentation, AuthorityFailure::None };
    case EyeTransport::CpuEngineCenter:
        return { PresentationAuthority::CpuEngineCenter,
            presentation, AuthorityFailure::None };
    }
    return failure(AuthorityFailure::UnsupportedTransport);
}
}
