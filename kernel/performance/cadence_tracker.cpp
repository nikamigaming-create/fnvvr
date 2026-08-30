#include "cadence_tracker.h"

#include <limits>

namespace fnvxr::kernel::performance
{
CadenceTracker::CadenceTracker(
    const ValidatedRuntimeConfig& config) noexcept
    : frameBudgetNanoseconds_(1'000'000'000ULL /
          config.get().performance.targetRefreshHz)
{
}

bool CadenceTracker::resetEpoch(std::uint64_t epoch) noexcept
{
    if (epoch == 0 || epoch == epoch_)
    {
        count(resetRejections_);
        return false;
    }
    active_ = {};
    epoch_ = epoch;
    latestFrameSequence_ = 0;
    hasFreshTransaction_ = false;
    lastFreshTransaction_ = {};
    clearEpochTelemetry();
    count(resets_);
    return true;
}

CadenceEventOutcome CadenceTracker::beginFrame(
    HostFrameId id,
    MonotonicNanoseconds timestamp) noexcept
{
    if (id.epoch == 0 || id.epoch != epoch_)
        return reject(CadenceEventOutcome::invalid_epoch);
    if (id.sequence == 0 || id.sequence <= latestFrameSequence_)
        return reject(CadenceEventOutcome::invalid_sequence);
    if (active_.active)
        return reject(CadenceEventOutcome::frame_already_active);
    if (hasTimestamp_ && timestamp < lastTimestamp_)
        return reject(CadenceEventOutcome::timestamp_regression);

    active_ = {};
    active_.active = true;
    active_.id = id;
    active_.begin = timestamp;
    active_.lastEvent = timestamp;
    if (framesBegun_.value() == 0)
        firstFrameBegin_ = timestamp;
    lastTimestamp_ = timestamp;
    hasTimestamp_ = true;
    latestFrameSequence_ = id.sequence;
    count(framesBegun_);
    return CadenceEventOutcome::accepted;
}

CadenceEventOutcome CadenceTracker::recordEyeRendered(
    HostFrameId id,
    Eye eye,
    MonotonicNanoseconds timestamp) noexcept
{
    const CadenceEventOutcome ready = validateActiveEvent(id, timestamp);
    if (ready != CadenceEventOutcome::accepted)
        return ready;
    bool& rendered = eye == Eye::left
        ? active_.leftRendered : active_.rightRendered;
    if (rendered)
        return reject(CadenceEventOutcome::duplicate_event);
    rendered = true;
    active_.lastEvent = timestamp;
    lastTimestamp_ = timestamp;
    count(eye == Eye::left ? leftRenders_ : rightRenders_);
    return CadenceEventOutcome::accepted;
}

CadenceEventOutcome CadenceTracker::recordEyeSubmitted(
    HostFrameId id,
    Eye eye,
    presentation::SourceKey transaction,
    MonotonicNanoseconds timestamp) noexcept
{
    const CadenceEventOutcome ready = validateActiveEvent(id, timestamp);
    if (ready != CadenceEventOutcome::accepted)
        return ready;
    if (!presentation::isValid(transaction))
        return reject(CadenceEventOutcome::invalid_transaction);

    bool& submitted = eye == Eye::left
        ? active_.leftSubmitted : active_.rightSubmitted;
    if (submitted)
        return reject(CadenceEventOutcome::duplicate_event);
    submitted = true;
    (eye == Eye::left
            ? active_.leftTransaction : active_.rightTransaction) = transaction;
    active_.lastEvent = timestamp;
    lastTimestamp_ = timestamp;
    count(eye == Eye::left ? leftSubmissions_ : rightSubmissions_);
    return CadenceEventOutcome::accepted;
}

CadenceEventOutcome CadenceTracker::endFrame(
    HostFrameId id,
    MonotonicNanoseconds timestamp) noexcept
{
    const CadenceEventOutcome ready = validateActiveEvent(id, timestamp);
    if (ready != CadenceEventOutcome::accepted)
        return ready;

    accountFrameWork(timestamp, true);
    accountDrops();
    accountTransaction();
    finishFrame(timestamp);
    count(framesEnded_);
    return CadenceEventOutcome::accepted;
}

CadenceEventOutcome CadenceTracker::endFrameNotRequested(
    HostFrameId id,
    MonotonicNanoseconds timestamp) noexcept
{
    const CadenceEventOutcome ready = validateActiveEvent(id, timestamp);
    if (ready != CadenceEventOutcome::accepted)
        return ready;

    accountFrameWork(timestamp, false);
    finishFrame(timestamp);
    count(framesEnded_);
    count(framesNotRequested_);
    return CadenceEventOutcome::accepted;
}

CadenceEventOutcome CadenceTracker::abortFrame(
    HostFrameId id,
    MonotonicNanoseconds timestamp) noexcept
{
    const CadenceEventOutcome ready = validateActiveEvent(id, timestamp);
    if (ready != CadenceEventOutcome::accepted)
        return ready;

    lastTimestamp_ = timestamp;
    active_ = {};
    count(framesAborted_);
    return CadenceEventOutcome::accepted;
}

CadenceSnapshot CadenceTracker::snapshot() const noexcept
{
    CadenceSnapshot result {};
    result.epoch = epoch_;
    result.frameBudgetNanoseconds = frameBudgetNanoseconds_;
    result.framesBegun = framesBegun_.value();
    result.framesEnded = framesEnded_.value();
    result.framesNotRequested = framesNotRequested_.value();
    result.framesAborted = framesAborted_.value();
    result.budgetOverruns = budgetOverruns_.value();
    result.requestedBudgetOverruns = requestedBudgetOverruns_.value();
    result.firstFrameBeginNanoseconds = firstFrameBegin_.value;
    result.lastFrameEndNanoseconds = lastFrameEnd_.value;
    result.observedSpanNanoseconds = lastFrameEnd_.value >= firstFrameBegin_.value
        ? lastFrameEnd_.value - firstFrameBegin_.value : 0u;
    result.totalFrameWorkNanoseconds = totalFrameWorkNanoseconds_;
    result.maximumFrameWorkNanoseconds = maximumFrameWorkNanoseconds_;
    result.totalRequestedFrameWorkNanoseconds =
        totalRequestedFrameWorkNanoseconds_;
    result.maximumRequestedFrameWorkNanoseconds =
        maximumRequestedFrameWorkNanoseconds_;
    result.leftRenders = leftRenders_.value();
    result.rightRenders = rightRenders_.value();
    result.leftSubmissions = leftSubmissions_.value();
    result.rightSubmissions = rightSubmissions_.value();
    result.freshTransactions = freshTransactions_.value();
    result.repeatedTransactions = repeatedTransactions_.value();
    result.staleTransactions = staleTransactions_.value();
    result.leftDrops = leftDrops_.value();
    result.rightDrops = rightDrops_.value();
    result.pairDrops = pairDrops_.value();
    result.eventRejections = eventRejections_.value();
    result.resets = resets_.value();
    result.resetRejections = resetRejections_.value();
    result.counterSaturations = counterSaturations_.value();
    result.frameActive = active_.active;
    result.hasFreshTransaction = hasFreshTransaction_;
    result.lastFreshTransaction = lastFreshTransaction_;
    return result;
}

CadenceEventOutcome CadenceTracker::validateActiveEvent(
    HostFrameId id,
    MonotonicNanoseconds timestamp) noexcept
{
    if (!active_.active)
        return reject(CadenceEventOutcome::no_active_frame);
    if (id.epoch != active_.id.epoch || id.sequence != active_.id.sequence)
        return reject(CadenceEventOutcome::frame_mismatch);
    if (timestamp < active_.lastEvent)
        return reject(CadenceEventOutcome::timestamp_regression);
    return CadenceEventOutcome::accepted;
}

void CadenceTracker::accountDrops() noexcept
{
    if (!active_.leftSubmitted)
        count(leftDrops_);
    if (!active_.rightSubmitted)
        count(rightDrops_);
    if (!active_.leftSubmitted || !active_.rightSubmitted
        || active_.leftTransaction != active_.rightTransaction)
    {
        count(pairDrops_);
    }
}

void CadenceTracker::accountTransaction() noexcept
{
    if (!active_.leftSubmitted || !active_.rightSubmitted)
        return;
    if (active_.leftTransaction != active_.rightTransaction)
    {
        count(staleTransactions_);
        return;
    }

    const presentation::SourceKey transaction = active_.leftTransaction;
    if (!hasFreshTransaction_
        || presentation::isStrictlyNewer(transaction, lastFreshTransaction_))
    {
        hasFreshTransaction_ = true;
        lastFreshTransaction_ = transaction;
        count(freshTransactions_);
    }
    else if (transaction == lastFreshTransaction_)
    {
        count(repeatedTransactions_);
    }
    else
    {
        count(staleTransactions_);
    }
}

void CadenceTracker::accountFrameWork(
    MonotonicNanoseconds timestamp,
    bool renderRequested) noexcept
{
    const std::uint64_t frameWork = elapsed(active_.begin, timestamp).value;
    if (frameWork > frameBudgetNanoseconds_)
    {
        count(budgetOverruns_);
        if (renderRequested)
            count(requestedBudgetOverruns_);
    }
    add(totalFrameWorkNanoseconds_, frameWork);
    if (frameWork > maximumFrameWorkNanoseconds_)
        maximumFrameWorkNanoseconds_ = frameWork;
    if (renderRequested)
    {
        add(totalRequestedFrameWorkNanoseconds_, frameWork);
        if (frameWork > maximumRequestedFrameWorkNanoseconds_)
            maximumRequestedFrameWorkNanoseconds_ = frameWork;
    }
    lastFrameEnd_ = timestamp;
}

void CadenceTracker::finishFrame(MonotonicNanoseconds timestamp) noexcept
{
    lastTimestamp_ = timestamp;
    active_ = {};
}

CadenceEventOutcome CadenceTracker::reject(
    CadenceEventOutcome outcome) noexcept
{
    count(eventRejections_);
    return outcome;
}

void CadenceTracker::count(SaturatingCounter& counter) noexcept
{
    if (!counter.increment() && &counter != &counterSaturations_)
        static_cast<void>(counterSaturations_.increment());
}

void CadenceTracker::add(
    std::uint64_t& destination,
    std::uint64_t amount) noexcept
{
    if (amount > std::numeric_limits<std::uint64_t>::max() - destination)
    {
        destination = std::numeric_limits<std::uint64_t>::max();
        static_cast<void>(counterSaturations_.increment());
        return;
    }
    destination += amount;
}

void CadenceTracker::clearEpochTelemetry() noexcept
{
    firstFrameBegin_ = {};
    lastFrameEnd_ = {};
    totalFrameWorkNanoseconds_ = 0;
    maximumFrameWorkNanoseconds_ = 0;
    totalRequestedFrameWorkNanoseconds_ = 0;
    maximumRequestedFrameWorkNanoseconds_ = 0;
    framesBegun_.reset();
    framesEnded_.reset();
    framesNotRequested_.reset();
    framesAborted_.reset();
    budgetOverruns_.reset();
    requestedBudgetOverruns_.reset();
    leftRenders_.reset();
    rightRenders_.reset();
    leftSubmissions_.reset();
    rightSubmissions_.reset();
    freshTransactions_.reset();
    repeatedTransactions_.reset();
    staleTransactions_.reset();
    leftDrops_.reset();
    rightDrops_.reset();
    pairDrops_.reset();
    eventRejections_.reset();
    resets_.reset();
    resetRejections_.reset();
    counterSaturations_.reset();
}
}
