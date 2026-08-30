#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::host::gpu_color
{
struct RuntimeEvidence final
{
    std::uint64_t sample = 0;
    std::uint32_t phase = 0;
    std::uint32_t menuBits = 0;
    std::uint32_t showroomActive = 0;
    bool cameraActive = false;
    bool fresh = false;
};

constexpr bool sameRuntimePresentationState(
    const RuntimeEvidence& left,
    const RuntimeEvidence& right) noexcept
{
    return left.phase == right.phase
        && left.menuBits == right.menuBits
        && left.showroomActive == right.showroomActive
        && left.cameraActive == right.cameraActive;
}

constexpr bool stableRuntimeLineage(
    const RuntimeEvidence& source,
    const RuntimeEvidence& current) noexcept
{
    return source.fresh && current.fresh && source.sample != 0
        && current.sample != 0 && source.sample <= current.sample
        && sameRuntimePresentationState(source, current);
}

struct RuntimeHistoryTelemetry final
{
    std::uint64_t records = 0;
    std::uint64_t overwrites = 0;
    std::uint64_t conflicts = 0;
    std::uint64_t exactHits = 0;
    std::uint64_t bracketHits = 0;
    std::uint64_t misses = 0;
    std::uint64_t resets = 0;
};

// Exact sample lookup is O(1). Missing source samples may be bridged only
// inside the fixed 32-sample policy window, so work never scales with Capacity.
template <std::size_t Capacity = 64>
class RuntimeEvidenceHistory final
{
    static_assert(Capacity > 0);

public:
    void reset() noexcept
    {
        entries_ = {};
        ++telemetry_.resets;
    }

    void record(const RuntimeEvidence& evidence) noexcept
    {
        if (!evidence.fresh || evidence.sample == 0)
            return;

        Entry& entry = slot(evidence.sample);
        if (entry.occupied && entry.evidence.sample == evidence.sample)
        {
            if (!entry.conflicted
                && !sameRuntimePresentationState(entry.evidence, evidence))
            {
                entry.evidence.fresh = false;
                entry.conflicted = true;
                ++telemetry_.conflicts;
            }
            return;
        }
        if (entry.occupied)
            ++telemetry_.overwrites;
        entry = { evidence, true, false };
        ++telemetry_.records;
    }

    [[nodiscard]] bool findStableSource(
        std::uint64_t sourceSample,
        const RuntimeEvidence& current,
        RuntimeEvidence& found,
        bool* bracketedOut = nullptr) const noexcept
    {
        found = {};
        if (bracketedOut != nullptr)
            *bracketedOut = false;
        if (sourceSample == 0)
            return miss();

        if (const Entry* exact = find(sourceSample))
        {
            if (exact->conflicted
                || !stableRuntimeLineage(exact->evidence, current))
            {
                return miss();
            }
            found = exact->evidence;
            ++telemetry_.exactHits;
            return true;
        }

        if (!current.fresh || current.sample == 0
            || sourceSample >= current.sample)
        {
            return miss();
        }

        const Entry* lower = nearestLower(sourceSample);
        if (lower == nullptr || lower->conflicted
            || !stableRuntimeLineage(lower->evidence, current)
            || current.sample - lower->evidence.sample > MaxBracketSampleSpan)
        {
            return miss();
        }

        const std::uint64_t span = current.sample - lower->evidence.sample;
        for (std::uint64_t offset = 0; offset <= span; ++offset)
        {
            const Entry* observed = find(lower->evidence.sample + offset);
            if (observed == nullptr)
                continue;
            if (observed->conflicted || !observed->evidence.fresh
                || !sameRuntimePresentationState(observed->evidence, current))
            {
                return miss();
            }
        }

        found = current;
        found.sample = sourceSample;
        if (bracketedOut != nullptr)
            *bracketedOut = true;
        ++telemetry_.bracketHits;
        return true;
    }

    [[nodiscard]] RuntimeHistoryTelemetry telemetry() const noexcept
    {
        return telemetry_;
    }

private:
    static constexpr std::uint64_t MaxBracketSampleSpan = 32;

    struct Entry final
    {
        RuntimeEvidence evidence {};
        bool occupied = false;
        bool conflicted = false;
    };

    [[nodiscard]] static constexpr std::size_t index(
        std::uint64_t sample) noexcept
    {
        return static_cast<std::size_t>(sample % Capacity);
    }

    [[nodiscard]] Entry& slot(std::uint64_t sample) noexcept
    {
        return entries_[index(sample)];
    }

    [[nodiscard]] const Entry* find(std::uint64_t sample) const noexcept
    {
        const Entry& candidate = entries_[index(sample)];
        return candidate.occupied && candidate.evidence.sample == sample
            ? &candidate : nullptr;
    }

    [[nodiscard]] const Entry* nearestLower(
        std::uint64_t sourceSample) const noexcept
    {
        const std::uint64_t search = sourceSample > MaxBracketSampleSpan
            ? MaxBracketSampleSpan : sourceSample - 1;
        for (std::uint64_t distance = 1; distance <= search; ++distance)
        {
            if (const Entry* candidate = find(sourceSample - distance))
                return candidate;
        }
        return nullptr;
    }

    [[nodiscard]] bool miss() const noexcept
    {
        ++telemetry_.misses;
        return false;
    }

    std::array<Entry, Capacity> entries_ {};
    mutable RuntimeHistoryTelemetry telemetry_ {};
};
}
