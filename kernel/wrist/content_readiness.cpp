#include "content_readiness.h"

namespace fnvxr::kernel::wrist
{
ContentReadinessDecision assessContentReadiness(
    const ContentReadinessInput& input) noexcept
{
    const auto reject = [](ContentReadinessReason reason,
                            std::uint64_t age = 0) noexcept
    {
        return ContentReadinessDecision { false, reason, age, 0, 0 };
    };

    if (!input.menuActive)
        return reject(ContentReadinessReason::MenuInactive);
    if (!input.screenFocused)
        return reject(ContentReadinessReason::ScreenNotFocused);
    if (!input.retained.resourceAvailable)
        return reject(ContentReadinessReason::ResourceUnavailable);
    if (input.retained.resourceGeneration == 0)
        return reject(ContentReadinessReason::ResourceIdentityUnavailable);
    if (!input.retained.pixelsProven)
        return reject(ContentReadinessReason::PixelsUnproven);
    if (!input.retained.acceptanceRecorded)
        return reject(ContentReadinessReason::AcceptanceUnavailable);
    if (input.retained.producerProcessId == 0
        || input.retained.rendererProducerEpoch == 0
        || input.currentProducerProcessId == 0
        || input.currentRendererProducerEpoch == 0)
    {
        return reject(ContentReadinessReason::ProducerIdentityUnavailable);
    }
    if (input.retained.producerProcessId
            != input.currentProducerProcessId
        || input.retained.rendererProducerEpoch
            != input.currentRendererProducerEpoch)
    {
        return reject(ContentReadinessReason::ProducerOwnershipMismatch);
    }
    if (input.nowMilliseconds < input.retained.acceptedAtMilliseconds)
        return reject(ContentReadinessReason::ClockRegressed);

    const std::uint64_t age =
        input.nowMilliseconds - input.retained.acceptedAtMilliseconds;
    if (age > input.maximumAgeMilliseconds)
        return reject(ContentReadinessReason::ContentStale, age);
    if (!input.runtime.source.fresh || input.runtime.source.sample == 0)
        return reject(ContentReadinessReason::SourceRuntimeUnavailable, age);
    if (!input.runtime.current.fresh || input.runtime.current.sample == 0)
        return reject(ContentReadinessReason::CurrentRuntimeUnavailable, age);
    if (input.retained.runtimeSample != input.runtime.source.sample)
        return reject(ContentReadinessReason::ContentSourceMismatch, age);
    if (input.runtime.current.sample < input.runtime.source.sample)
        return reject(ContentReadinessReason::RuntimeOrderInvalid, age);
    if (!input.runtime.stable)
        return reject(ContentReadinessReason::RuntimeLineageUnproven, age);
    if (!input.runtime.source.pipBoyMenu)
        return reject(ContentReadinessReason::SourceNotPipBoy, age);
    if (!input.runtime.current.pipBoyMenu)
        return reject(ContentReadinessReason::CurrentNotPipBoy, age);
    if (!input.runtime.source.worldPresentationContinues)
    {
        return reject(
            ContentReadinessReason::SourceWorldDoesNotContinue, age);
    }
    if (!input.runtime.current.worldPresentationContinues)
    {
        return reject(
            ContentReadinessReason::CurrentWorldDoesNotContinue, age);
    }

    return { true, ContentReadinessReason::Ready, age,
        input.retained.resourceGeneration, input.retained.runtimeSample };
}
}
