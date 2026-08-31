#pragma once

#include "cadence_types.h"

#include <cstdint>

namespace fnvxr::kernel::performance
{
struct FramePerformanceMetrics final
{
    std::uint64_t frames = 0;
    double framesPerSecond = 0.0;
    double averageWorkMilliseconds = 0.0;
    std::uint64_t maximumWorkNanoseconds = 0;
    std::uint64_t budgetOverruns = 0;
};

struct PairPerformanceMetrics final
{
    std::uint64_t pairs = 0;
    double pairsPerSecond = 0.0;
};

struct CadencePerformanceMetrics final
{
    FramePerformanceMetrics host {};
    FramePerformanceMetrics requested {};
    PairPerformanceMetrics submittedPairs {};
};

[[nodiscard]] CadencePerformanceMetrics deriveCadencePerformance(
    const CadenceSnapshot& cadence) noexcept;
}
