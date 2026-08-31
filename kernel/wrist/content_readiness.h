#pragma once

#include <cstdint>

namespace fnvxr::kernel::wrist
{
struct RetainedContentEvidence final
{
    std::uint64_t resourceGeneration = 0;
    std::uint64_t runtimeSample = 0;
    std::uint64_t acceptedAtMilliseconds = 0;
    std::uint32_t producerProcessId = 0;
    std::uint64_t rendererProducerEpoch = 0;
    bool resourceAvailable = false;
    bool pixelsProven = false;
    bool acceptanceRecorded = false;
};

struct RuntimeContentState final
{
    std::uint64_t sample = 0;
    bool fresh = false;
    bool pipBoyMenu = false;
    bool worldPresentationContinues = false;
};

struct RuntimeContentLineage final
{
    RuntimeContentState source {};
    RuntimeContentState current {};
    bool stable = false;
};

struct ContentReadinessInput final
{
    bool menuActive = false;
    bool screenFocused = false;
    RetainedContentEvidence retained {};
    RuntimeContentLineage runtime {};
    std::uint32_t currentProducerProcessId = 0;
    std::uint64_t currentRendererProducerEpoch = 0;
    std::uint64_t nowMilliseconds = 0;
    std::uint64_t maximumAgeMilliseconds = 0;
};

enum class ContentReadinessReason
{
    MenuInactive,
    ScreenNotFocused,
    ResourceUnavailable,
    ResourceIdentityUnavailable,
    PixelsUnproven,
    AcceptanceUnavailable,
    ProducerIdentityUnavailable,
    ProducerOwnershipMismatch,
    ClockRegressed,
    ContentStale,
    SourceRuntimeUnavailable,
    CurrentRuntimeUnavailable,
    ContentSourceMismatch,
    RuntimeOrderInvalid,
    RuntimeLineageUnproven,
    SourceNotPipBoy,
    CurrentNotPipBoy,
    SourceWorldDoesNotContinue,
    CurrentWorldDoesNotContinue,
    Ready,
};

struct ContentReadinessDecision final
{
    bool ready = false;
    ContentReadinessReason reason = ContentReadinessReason::MenuInactive;
    std::uint64_t ageMilliseconds = 0;
    std::uint64_t resourceGeneration = 0;
    std::uint64_t runtimeSample = 0;
};

[[nodiscard]] ContentReadinessDecision assessContentReadiness(
    const ContentReadinessInput& input) noexcept;
}
