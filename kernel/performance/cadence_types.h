#pragma once

#include "../presentation/source_key.h"
#include "monotonic_time.h"

#include <cstdint>

namespace fnvxr::kernel::performance
{
enum class Eye : std::uint8_t
{
    left,
    right,
};

enum class CadenceEventOutcome : std::uint8_t
{
    accepted,
    invalid_epoch,
    invalid_sequence,
    frame_already_active,
    no_active_frame,
    frame_mismatch,
    timestamp_regression,
    duplicate_event,
    invalid_transaction,
};

struct HostFrameId final
{
    std::uint64_t epoch = 0;
    std::uint64_t sequence = 0;
};

struct CadenceSnapshot final
{
    std::uint64_t epoch = 0;
    std::uint64_t frameBudgetNanoseconds = 0;
    std::uint64_t framesBegun = 0;
    std::uint64_t framesEnded = 0;
    std::uint64_t framesAborted = 0;
    std::uint64_t budgetOverruns = 0;
    std::uint64_t firstFrameBeginNanoseconds = 0;
    std::uint64_t lastFrameEndNanoseconds = 0;
    std::uint64_t observedSpanNanoseconds = 0;
    std::uint64_t totalFrameWorkNanoseconds = 0;
    std::uint64_t maximumFrameWorkNanoseconds = 0;
    std::uint64_t leftRenders = 0;
    std::uint64_t rightRenders = 0;
    std::uint64_t leftSubmissions = 0;
    std::uint64_t rightSubmissions = 0;
    std::uint64_t freshTransactions = 0;
    std::uint64_t repeatedTransactions = 0;
    std::uint64_t staleTransactions = 0;
    // Eye drops mean no submission for that eye. A pair also drops when its
    // two submitted eyes name different image transactions.
    std::uint64_t leftDrops = 0;
    std::uint64_t rightDrops = 0;
    std::uint64_t pairDrops = 0;
    std::uint64_t eventRejections = 0;
    std::uint64_t resets = 0;
    std::uint64_t resetRejections = 0;
    std::uint64_t counterSaturations = 0;
    bool frameActive = false;
    bool hasFreshTransaction = false;
    presentation::SourceKey lastFreshTransaction {};
};
}
