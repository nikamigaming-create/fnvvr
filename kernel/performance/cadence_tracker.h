#pragma once

#include "../config_validation.h"
#include "cadence_types.h"
#include "saturating_counter.h"

#include <cstdint>

namespace fnvxr::kernel::performance
{
class CadenceTracker final
{
public:
    explicit CadenceTracker(const ValidatedRuntimeConfig& config) noexcept;

    [[nodiscard]] bool resetEpoch(std::uint64_t epoch) noexcept;
    [[nodiscard]] CadenceEventOutcome beginFrame(
        HostFrameId id,
        MonotonicNanoseconds timestamp) noexcept;
    [[nodiscard]] CadenceEventOutcome recordEyeRendered(
        HostFrameId id,
        Eye eye,
        MonotonicNanoseconds timestamp) noexcept;
    [[nodiscard]] CadenceEventOutcome recordEyeSubmitted(
        HostFrameId id,
        Eye eye,
        presentation::SourceKey transaction,
        MonotonicNanoseconds timestamp) noexcept;
    [[nodiscard]] CadenceEventOutcome endFrame(
        HostFrameId id,
        MonotonicNanoseconds timestamp) noexcept;
    [[nodiscard]] CadenceEventOutcome endFrameNotRequested(
        HostFrameId id,
        MonotonicNanoseconds timestamp) noexcept;
    [[nodiscard]] CadenceEventOutcome abortFrame(
        HostFrameId id,
        MonotonicNanoseconds timestamp) noexcept;
    [[nodiscard]] CadenceSnapshot snapshot() const noexcept;

private:
    struct ActiveFrame final
    {
        bool active = false;
        HostFrameId id {};
        MonotonicNanoseconds begin {};
        MonotonicNanoseconds lastEvent {};
        bool leftRendered = false;
        bool rightRendered = false;
        bool leftSubmitted = false;
        bool rightSubmitted = false;
        presentation::SourceKey leftTransaction {};
        presentation::SourceKey rightTransaction {};
    };

    [[nodiscard]] CadenceEventOutcome validateActiveEvent(
        HostFrameId id,
        MonotonicNanoseconds timestamp) noexcept;
    void accountDrops() noexcept;
    void accountTransaction() noexcept;
    void accountFrameWork(
        MonotonicNanoseconds timestamp,
        bool renderRequested) noexcept;
    void finishFrame(MonotonicNanoseconds timestamp) noexcept;
    [[nodiscard]] CadenceEventOutcome reject(
        CadenceEventOutcome outcome) noexcept;
    void count(SaturatingCounter& counter) noexcept;
    void add(std::uint64_t& destination, std::uint64_t amount) noexcept;
    void clearEpochTelemetry() noexcept;

    std::uint64_t frameBudgetNanoseconds_ = 0;
    std::uint64_t epoch_ = 0;
    std::uint64_t latestFrameSequence_ = 0;
    ActiveFrame active_ {};
    MonotonicNanoseconds lastTimestamp_ {};
    MonotonicNanoseconds firstFrameBegin_ {};
    MonotonicNanoseconds lastFrameEnd_ {};
    std::uint64_t totalFrameWorkNanoseconds_ = 0;
    std::uint64_t maximumFrameWorkNanoseconds_ = 0;
    std::uint64_t totalRequestedFrameWorkNanoseconds_ = 0;
    std::uint64_t maximumRequestedFrameWorkNanoseconds_ = 0;
    bool hasTimestamp_ = false;
    bool hasFreshTransaction_ = false;
    presentation::SourceKey lastFreshTransaction_ {};
    SaturatingCounter framesBegun_;
    SaturatingCounter framesEnded_;
    SaturatingCounter framesNotRequested_;
    SaturatingCounter framesAborted_;
    SaturatingCounter budgetOverruns_;
    SaturatingCounter requestedBudgetOverruns_;
    SaturatingCounter leftRenders_;
    SaturatingCounter rightRenders_;
    SaturatingCounter leftSubmissions_;
    SaturatingCounter rightSubmissions_;
    SaturatingCounter freshTransactions_;
    SaturatingCounter repeatedTransactions_;
    SaturatingCounter staleTransactions_;
    SaturatingCounter leftDrops_;
    SaturatingCounter rightDrops_;
    SaturatingCounter pairDrops_;
    SaturatingCounter eventRejections_;
    SaturatingCounter resets_;
    SaturatingCounter resetRejections_;
    SaturatingCounter counterSaturations_;
};
}
