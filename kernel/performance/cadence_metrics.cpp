#include "cadence_metrics.h"

#include <algorithm>

namespace fnvxr::kernel::performance
{
namespace
{
double rate(std::uint64_t count, std::uint64_t spanNanoseconds) noexcept
{
    if (spanNanoseconds == 0)
        return 0.0;
    return static_cast<double>(count) * 1.0e9
        / static_cast<double>(spanNanoseconds);
}

double averageMilliseconds(
    std::uint64_t totalNanoseconds,
    std::uint64_t count) noexcept
{
    if (count == 0)
        return 0.0;
    return static_cast<double>(totalNanoseconds)
        / static_cast<double>(count) / 1.0e6;
}

std::uint64_t difference(
    std::uint64_t total,
    std::uint64_t excluded) noexcept
{
    return total >= excluded ? total - excluded : 0;
}
}

CadencePerformanceMetrics deriveCadencePerformance(
    const CadenceSnapshot& cadence) noexcept
{
    CadencePerformanceMetrics result {};

    result.host.frames = cadence.framesEnded;
    result.host.framesPerSecond = rate(
        result.host.frames, cadence.observedSpanNanoseconds);
    result.host.averageWorkMilliseconds = averageMilliseconds(
        cadence.totalFrameWorkNanoseconds, result.host.frames);
    result.host.maximumWorkNanoseconds = result.host.frames > 0
        ? cadence.maximumFrameWorkNanoseconds
        : 0;
    result.host.budgetOverruns = std::min(
        cadence.budgetOverruns, result.host.frames);

    result.requested.frames = difference(
        cadence.framesEnded, cadence.framesNotRequested);
    result.requested.framesPerSecond = rate(
        result.requested.frames, cadence.observedSpanNanoseconds);
    result.requested.averageWorkMilliseconds = averageMilliseconds(
        cadence.totalRequestedFrameWorkNanoseconds,
        result.requested.frames);
    result.requested.maximumWorkNanoseconds = result.requested.frames > 0
        ? cadence.maximumRequestedFrameWorkNanoseconds
        : 0;
    result.requested.budgetOverruns = std::min(
        cadence.requestedBudgetOverruns, result.requested.frames);

    const std::uint64_t completeRequestedPairs = difference(
        result.requested.frames, cadence.pairDrops);
    result.submittedPairs.pairs = std::min({
        completeRequestedPairs,
        cadence.leftSubmissions,
        cadence.rightSubmissions,
    });
    result.submittedPairs.pairsPerSecond = rate(
        result.submittedPairs.pairs, cadence.observedSpanNanoseconds);
    return result;
}
}
