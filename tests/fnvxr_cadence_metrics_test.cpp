#include "../kernel/performance/cadence_metrics.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::kernel::performance::CadenceSnapshot;
using fnvxr::kernel::performance::deriveCadencePerformance;

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

bool near(double actual, double expected) noexcept
{
    return std::abs(actual - expected) < 0.000'001;
}
}

int main()
{
    CadenceSnapshot mixed {};
    mixed.framesEnded = 10;
    mixed.framesNotRequested = 8;
    mixed.observedSpanNanoseconds = 1'000'000'000;
    mixed.totalFrameWorkNanoseconds = 80'000'000;
    mixed.maximumFrameWorkNanoseconds = 12'000'000;
    mixed.budgetOverruns = 3;
    mixed.totalRequestedFrameWorkNanoseconds = 18'000'000;
    mixed.maximumRequestedFrameWorkNanoseconds = 10'000'000;
    mixed.requestedBudgetOverruns = 1;
    mixed.leftSubmissions = 2;
    mixed.rightSubmissions = 2;

    const auto metrics = deriveCadencePerformance(mixed);
    if (metrics.host.frames != 10 || !near(metrics.host.framesPerSecond, 10.0)
        || !near(metrics.host.averageWorkMilliseconds, 8.0)
        || metrics.host.maximumWorkNanoseconds != 12'000'000
        || metrics.host.budgetOverruns != 3)
    {
        return fail("host metrics did not include all ended frames");
    }
    if (metrics.requested.frames != 2
        || !near(metrics.requested.framesPerSecond, 2.0)
        || !near(metrics.requested.averageWorkMilliseconds, 9.0)
        || metrics.requested.maximumWorkNanoseconds != 10'000'000
        || metrics.requested.budgetOverruns != 1)
    {
        return fail("requested metrics included non-requested frame work");
    }
    if (metrics.submittedPairs.pairs != 2
        || !near(metrics.submittedPairs.pairsPerSecond, 2.0))
    {
        return fail("submitted-pair rate was incorrect");
    }

    CadenceSnapshot notRequested {};
    notRequested.framesEnded = 9;
    notRequested.framesNotRequested = 9;
    notRequested.observedSpanNanoseconds = 100'000'000;
    notRequested.totalFrameWorkNanoseconds = 90'000'000;
    notRequested.maximumFrameWorkNanoseconds = 15'000'000;
    notRequested.budgetOverruns = 9;
    notRequested.totalRequestedFrameWorkNanoseconds = 90'000'000;
    notRequested.maximumRequestedFrameWorkNanoseconds = 15'000'000;
    notRequested.requestedBudgetOverruns = 9;
    notRequested.leftSubmissions = 9;
    notRequested.rightSubmissions = 9;
    const auto idle = deriveCadencePerformance(notRequested);
    if (idle.host.frames != 9 || !near(idle.host.framesPerSecond, 90.0)
        || idle.requested.frames != 0 || idle.requested.framesPerSecond != 0.0
        || idle.requested.averageWorkMilliseconds != 0.0
        || idle.requested.maximumWorkNanoseconds != 0
        || idle.requested.budgetOverruns != 0
        || idle.submittedPairs.pairs != 0)
    {
        return fail("non-requested frames inflated requested metrics");
    }

    CadenceSnapshot dropped {};
    dropped.framesEnded = 4;
    dropped.observedSpanNanoseconds = 2'000'000'000;
    dropped.leftSubmissions = 3;
    dropped.rightSubmissions = 3;
    dropped.pairDrops = 2;
    const auto pairs = deriveCadencePerformance(dropped);
    if (pairs.submittedPairs.pairs != 2
        || !near(pairs.submittedPairs.pairsPerSecond, 1.0))
    {
        return fail("cross-frame eye submissions inflated submitted pairs");
    }

    CadenceSnapshot inconsistent {};
    inconsistent.framesEnded = 3;
    inconsistent.framesNotRequested = 4;
    inconsistent.requestedBudgetOverruns = 8;
    inconsistent.totalRequestedFrameWorkNanoseconds = 12'000'000;
    inconsistent.maximumRequestedFrameWorkNanoseconds = 12'000'000;
    const auto closed = deriveCadencePerformance(inconsistent);
    if (closed.requested.frames != 0
        || closed.requested.framesPerSecond != 0.0
        || closed.requested.averageWorkMilliseconds != 0.0
        || closed.requested.maximumWorkNanoseconds != 0
        || closed.requested.budgetOverruns != 0
        || closed.submittedPairs.pairs != 0)
    {
        return fail("inconsistent cadence snapshot did not fail closed");
    }

    CadenceSnapshot zeroSpan {};
    zeroSpan.framesEnded = 1;
    zeroSpan.totalFrameWorkNanoseconds = 5'000'000;
    zeroSpan.maximumFrameWorkNanoseconds = 5'000'000;
    const auto instantaneous = deriveCadencePerformance(zeroSpan);
    if (instantaneous.host.framesPerSecond != 0.0
        || !near(instantaneous.host.averageWorkMilliseconds, 5.0))
    {
        return fail("zero-span metrics were not deterministic");
    }

    return EXIT_SUCCESS;
}
