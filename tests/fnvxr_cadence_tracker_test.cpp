#include "../kernel/performance/cadence_tracker.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
using namespace fnvxr::kernel;
using namespace fnvxr::kernel::performance;

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

presentation::SourceKey transaction(std::uint64_t ordinal)
{
    return { 50, ordinal, ordinal };
}

bool completePair(
    CadenceTracker& tracker,
    HostFrameId frame,
    presentation::SourceKey source,
    std::uint64_t begin,
    std::uint64_t end)
{
    return tracker.beginFrame(frame, { begin }) == CadenceEventOutcome::accepted &&
        tracker.recordEyeRendered(frame, Eye::left, { begin + 1 }) ==
            CadenceEventOutcome::accepted &&
        tracker.recordEyeRendered(frame, Eye::right, { begin + 2 }) ==
            CadenceEventOutcome::accepted &&
        tracker.recordEyeSubmitted(frame, Eye::left, source, { begin + 3 }) ==
            CadenceEventOutcome::accepted &&
        tracker.recordEyeSubmitted(frame, Eye::right, source, { begin + 4 }) ==
            CadenceEventOutcome::accepted &&
        tracker.endFrame(frame, { end }) == CadenceEventOutcome::accepted;
}
}

int main()
{
    const auto validated = validateRuntimeConfig(RuntimeConfig {});
    if (!validated || !validated.config)
        return fail("default 90 Hz runtime configuration did not validate");

    CadenceTracker tracker(*validated.config);
    if (!tracker.resetEpoch(7) || tracker.resetEpoch(7) || tracker.resetEpoch(0))
        return fail("cadence epoch reset validation failed");
    const auto budget = tracker.snapshot().frameBudgetNanoseconds;
    if (budget != 11'111'111ULL)
        return fail("90 Hz frame budget was not normalized exactly");

    if (!completePair(tracker, { 7, 1 }, transaction(1), 100, 100 + budget))
        return fail("frame exactly on budget failed");
    if (!completePair(tracker, { 7, 2 }, transaction(2), 20'000'000,
            20'000'000 + budget + 1))
        return fail("frame one nanosecond over budget failed");
    if (!completePair(tracker, { 7, 3 }, transaction(2), 40'000'000,
            40'000'000 + budget))
        return fail("repeated transaction frame failed");
    if (!completePair(tracker, { 7, 4 }, transaction(1), 55'000'000,
            55'000'000 + budget))
        return fail("stale transaction frame failed");

    const HostFrameId leftDrop { 7, 5 };
    if (tracker.beginFrame(leftDrop, { 70'000'000 }) != CadenceEventOutcome::accepted ||
        tracker.recordEyeSubmitted(leftDrop, Eye::right, transaction(3), { 70'000'001 }) !=
            CadenceEventOutcome::accepted ||
        tracker.endFrame(leftDrop, { 70'000'002 }) != CadenceEventOutcome::accepted)
        return fail("left-eye drop frame failed");

    const HostFrameId rightDrop { 7, 6 };
    if (tracker.beginFrame(rightDrop, { 80'000'000 }) != CadenceEventOutcome::accepted ||
        tracker.recordEyeSubmitted(rightDrop, Eye::left, transaction(3), { 80'000'001 }) !=
            CadenceEventOutcome::accepted ||
        tracker.endFrame(rightDrop, { 80'000'002 }) != CadenceEventOutcome::accepted)
        return fail("right-eye drop frame failed");

    const HostFrameId pairDrop { 7, 7 };
    if (tracker.beginFrame(pairDrop, { 90'000'000 }) != CadenceEventOutcome::accepted ||
        tracker.recordEyeSubmitted(pairDrop, Eye::left, transaction(3), { 90'000'001 }) !=
            CadenceEventOutcome::accepted ||
        tracker.recordEyeSubmitted(pairDrop, Eye::right, transaction(4), { 90'000'002 }) !=
            CadenceEventOutcome::accepted ||
        tracker.endFrame(pairDrop, { 90'000'003 }) != CadenceEventOutcome::accepted)
        return fail("mismatched-eye pair frame failed");

    const HostFrameId aborted { 7, 8 };
    if (tracker.beginFrame(aborted, { 100'000'000 }) != CadenceEventOutcome::accepted ||
        tracker.recordEyeSubmitted(aborted, Eye::left, transaction(5), { 100'000'001 }) !=
            CadenceEventOutcome::accepted ||
        tracker.abortFrame(aborted, { 100'000'002 }) != CadenceEventOutcome::accepted)
        return fail("post-begin frame failure was not aborted cleanly");

    const auto beforeReset = tracker.snapshot();
    if (beforeReset.framesBegun != 8 || beforeReset.framesEnded != 7 ||
        beforeReset.framesAborted != 1 || beforeReset.budgetOverruns != 1 ||
        beforeReset.firstFrameBeginNanoseconds != 100 ||
        beforeReset.lastFrameEndNanoseconds != 90'000'003 ||
        beforeReset.observedSpanNanoseconds != 90'000'003 - 100 ||
        beforeReset.totalFrameWorkNanoseconds != 4 * budget + 8 ||
        beforeReset.maximumFrameWorkNanoseconds != budget + 1 ||
        beforeReset.leftRenders != 4 || beforeReset.rightRenders != 4 ||
        beforeReset.leftSubmissions != 7 || beforeReset.rightSubmissions != 6 ||
        beforeReset.freshTransactions != 2 || beforeReset.repeatedTransactions != 1 ||
        beforeReset.staleTransactions != 2 || beforeReset.leftDrops != 1 ||
        beforeReset.rightDrops != 1 || beforeReset.pairDrops != 3 ||
        beforeReset.eventRejections != 0 || beforeReset.resets != 1 ||
        beforeReset.resetRejections != 2 || beforeReset.frameActive ||
        !beforeReset.hasFreshTransaction ||
        beforeReset.lastFreshTransaction != transaction(2))
        return fail("pre-reset cadence telemetry was incorrect");

    if (!tracker.resetEpoch(8))
        return fail("new cadence epoch was rejected");
    const auto reset = tracker.snapshot();
    if (reset.epoch != 8 || reset.framesBegun != 0 || reset.framesEnded != 0 ||
        reset.framesAborted != 0 || reset.budgetOverruns != 0 ||
        reset.firstFrameBeginNanoseconds != 0 || reset.lastFrameEndNanoseconds != 0 ||
        reset.observedSpanNanoseconds != 0 || reset.totalFrameWorkNanoseconds != 0 ||
        reset.maximumFrameWorkNanoseconds != 0 || reset.leftRenders != 0 ||
        reset.rightRenders != 0 || reset.leftSubmissions != 0 ||
        reset.rightSubmissions != 0 || reset.freshTransactions != 0 ||
        reset.repeatedTransactions != 0 || reset.staleTransactions != 0 ||
        reset.leftDrops != 0 || reset.rightDrops != 0 || reset.pairDrops != 0 ||
        reset.eventRejections != 0 || reset.resets != 1 ||
        reset.resetRejections != 0 || reset.counterSaturations != 0 ||
        reset.frameActive || reset.hasFreshTransaction)
        return fail("epoch reset leaked prior telemetry or transaction state");

    if (tracker.beginFrame({ 7, 9 }, { 110'000'000 }) !=
            CadenceEventOutcome::invalid_epoch ||
        !completePair(tracker, { 8, 1 }, transaction(1), 120'000'000,
            120'000'000 + budget))
        return fail("new epoch did not isolate state and permit sequence reuse");
    if (tracker.beginFrame({ 8, 2 }, { 110'000'000 }) !=
        CadenceEventOutcome::timestamp_regression)
        return fail("monotonic timestamp regression across frames was accepted");

    const auto snapshot = tracker.snapshot();
    if (snapshot.framesBegun != 1 || snapshot.framesEnded != 1 ||
        snapshot.framesAborted != 0 || snapshot.budgetOverruns != 0 ||
        snapshot.firstFrameBeginNanoseconds != 120'000'000 ||
        snapshot.lastFrameEndNanoseconds != 120'000'000 + budget ||
        snapshot.observedSpanNanoseconds != budget ||
        snapshot.totalFrameWorkNanoseconds != budget ||
        snapshot.maximumFrameWorkNanoseconds != budget ||
        snapshot.leftRenders != 1 || snapshot.rightRenders != 1 ||
        snapshot.leftSubmissions != 1 || snapshot.rightSubmissions != 1 ||
        snapshot.freshTransactions != 1 || snapshot.repeatedTransactions != 0 ||
        snapshot.staleTransactions != 0 || snapshot.leftDrops != 0 ||
        snapshot.rightDrops != 0 || snapshot.pairDrops != 0 ||
        snapshot.eventRejections != 2 || snapshot.resets != 1 ||
        snapshot.resetRejections != 0 || snapshot.counterSaturations != 0 ||
        snapshot.frameActive || !snapshot.hasFreshTransaction ||
        snapshot.lastFreshTransaction != transaction(1))
    {
        std::cerr << "snapshot frames=" << snapshot.framesBegun << "/"
                  << snapshot.framesEnded
                  << " work=" << snapshot.totalFrameWorkNanoseconds
                  << " max=" << snapshot.maximumFrameWorkNanoseconds
                  << " first=" << snapshot.firstFrameBeginNanoseconds
                  << " last=" << snapshot.lastFrameEndNanoseconds
                  << " span=" << snapshot.observedSpanNanoseconds
                  << " drops=" << snapshot.leftDrops << "/"
                  << snapshot.rightDrops << "/" << snapshot.pairDrops
                  << " transactions=" << snapshot.freshTransactions << "/"
                  << snapshot.repeatedTransactions << "/"
                  << snapshot.staleTransactions << '\n';
        return fail("cadence telemetry snapshot was incorrect");
    }

    SaturatingCounter safe(std::numeric_limits<std::uint64_t>::max() - 1);
    if (!safe.increment() || safe.increment() ||
        safe.value() != std::numeric_limits<std::uint64_t>::max())
        return fail("telemetry counter did not saturate safely");

    return EXIT_SUCCESS;
}
