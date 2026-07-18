#pragma once

#include <cstdint>

namespace fnvxr::engine
{
enum class EngineEye : std::uint32_t
{
    Left = 0,
    Right = 1,
};

enum class StereoScheduleFailure : std::uint32_t
{
    None = 0,
    Snapshot = 1,
    ConservativeVisibility = 2,
    LeftAccumulator = 3,
    RightAccumulator = 4,
    InvalidResource = 5,
    AliasedAccumulators = 6,
    LeftBind = 7,
    LeftPopulate = 8,
    LeftRender = 9,
    LeftFinalize = 10,
    LeftEnd = 11,
    RightBind = 12,
    RightPopulate = 13,
    RightRender = 14,
    RightFinalize = 15,
    RightEnd = 16,
    StateRestore = 17,
    LeftIsolationToken = 18,
    RightIsolationToken = 19,
};

// The backend sets backendToken before its first eye-specific mutation. Eye
// identity is deliberately absent: the expected eye remains caller-owned and
// is passed explicitly to end/rollback, so opaque backend state cannot redirect
// cleanup. Zero means no isolation became active; nonzero must be unwound.
struct EyeIsolationToken
{
    std::uint64_t backendToken = 0;

    bool active() const noexcept
    {
        return backendToken != 0;
    }
};

struct StereoScheduleResources
{
    std::uint64_t visibleSet = 0;
    std::uint64_t leftAccumulator = 0;
    std::uint64_t rightAccumulator = 0;

    bool valid() const
    {
        return visibleSet != 0
            && leftAccumulator != 0
            && rightAccumulator != 0;
    }
};

struct StereoScheduleResult
{
    bool complete = false;
    StereoScheduleFailure failure = StereoScheduleFailure::None;
    // Resource handles are public only after a complete schedule. Every
    // failure returns this structure all-zero after the backend has discarded
    // any partial internal allocation.
    StereoScheduleResources resources {};
};

class StereoScheduleBackend
{
public:
    virtual ~StereoScheduleBackend() = default;

    // This operation must be observational/pure: it may copy the state needed
    // for a later restore, but it must not mutate authoritative engine state.
    // A false result therefore aborts without calling restoreAuthoritativeState.
    virtual bool snapshotAuthoritativeState() noexcept = 0;
    virtual bool buildConservativeVisibleSet(std::uint64_t& visibleSet) noexcept = 0;
    virtual bool createFreshAccumulator(
        EngineEye eye,
        std::uint64_t& accumulator) noexcept = 0;

    // NiAccumulator::Add(NiVisibleArray*) may render some geometries
    // immediately. Exact eye camera, color, depth, and auxiliary targets must
    // therefore be bound before populateAccumulator, not merely before render.
    virtual bool bindEyeCameraAndTargets(
        EngineEye eye,
        std::uint64_t accumulator,
        EyeIsolationToken& isolation) noexcept = 0;
    virtual bool populateAccumulator(
        EngineEye eye,
        std::uint64_t accumulator,
        std::uint64_t visibleSet) noexcept = 0;
    virtual bool renderAccumulator(
        EngineEye eye,
        std::uint64_t accumulator) noexcept = 0;
    virtual bool finalizeAccumulator(
        EngineEye eye,
        std::uint64_t accumulator) noexcept = 0;
    // A successful end must clear backendToken. The explicit eye is the
    // caller-owned cleanup identity and must not be inferred from the mutable
    // opaque token. rollbackEyeIsolation must be idempotent and clear any token
    // left active by a partial bind or end.
    virtual bool endEyeIsolation(
        EngineEye eye,
        EyeIsolationToken& isolation) noexcept = 0;
    virtual void rollbackEyeIsolation(
        EngineEye eye,
        EyeIsolationToken& isolation) noexcept = 0;

    virtual bool restoreAuthoritativeState() noexcept = 0;
    virtual void discardResources(
        std::uint64_t visibleSet,
        std::uint64_t leftAccumulator,
        std::uint64_t rightAccumulator) noexcept = 0;
};

inline StereoScheduleResult failSchedule(
    StereoScheduleBackend& backend,
    StereoScheduleFailure failure,
    const StereoScheduleResources& resources,
    EyeIsolationToken* isolation = nullptr,
    EngineEye isolationEye = EngineEye::Left) noexcept
{
    if (isolation && isolation->active())
        backend.rollbackEyeIsolation(isolationEye, *isolation);
    if (!backend.restoreAuthoritativeState())
        failure = StereoScheduleFailure::StateRestore;
    backend.discardResources(
        resources.visibleSet,
        resources.leftAccumulator,
        resources.rightAccumulator);
    return { false, failure, {} };
}

struct ScheduledEyeResult
{
    StereoScheduleFailure failure = StereoScheduleFailure::None;
    EyeIsolationToken isolation {};
};

inline ScheduledEyeResult renderScheduledEye(
    StereoScheduleBackend& backend,
    EngineEye eye,
    std::uint64_t accumulator,
    std::uint64_t visibleSet) noexcept
{
    const bool left = eye == EngineEye::Left;
    ScheduledEyeResult result {};
    if (!backend.bindEyeCameraAndTargets(eye, accumulator, result.isolation))
    {
        result.failure = left
            ? StereoScheduleFailure::LeftBind
            : StereoScheduleFailure::RightBind;
        return result;
    }
    if (!result.isolation.active())
    {
        result.failure = left
            ? StereoScheduleFailure::LeftIsolationToken
            : StereoScheduleFailure::RightIsolationToken;
        return result;
    }
    if (!backend.populateAccumulator(eye, accumulator, visibleSet))
    {
        result.failure = left
            ? StereoScheduleFailure::LeftPopulate
            : StereoScheduleFailure::RightPopulate;
        return result;
    }
    if (!backend.renderAccumulator(eye, accumulator))
    {
        result.failure = left
            ? StereoScheduleFailure::LeftRender
            : StereoScheduleFailure::RightRender;
        return result;
    }
    if (!backend.finalizeAccumulator(eye, accumulator))
    {
        result.failure = left
            ? StereoScheduleFailure::LeftFinalize
            : StereoScheduleFailure::RightFinalize;
        return result;
    }
    if (!backend.endEyeIsolation(eye, result.isolation)
        || result.isolation.active())
    {
        result.failure = left
            ? StereoScheduleFailure::LeftEnd
            : StereoScheduleFailure::RightEnd;
        return result;
    }
    return result;
}

inline StereoScheduleResult executeStereoSchedule(
    StereoScheduleBackend& backend) noexcept
{
    // Backend allocation APIs receive provisional writable outputs. Once all
    // three handles are validated, copy them into a private const snapshot and
    // use only that snapshot for eye work, cleanup, and the public result.
    StereoScheduleResources backendResourceOutputs {};
    if (!backend.snapshotAuthoritativeState())
    {
        backend.discardResources(0, 0, 0);
        return { false, StereoScheduleFailure::Snapshot, {} };
    }

    if (!backend.buildConservativeVisibleSet(backendResourceOutputs.visibleSet))
        return failSchedule(
            backend,
            StereoScheduleFailure::ConservativeVisibility,
            backendResourceOutputs);
    if (!backend.createFreshAccumulator(
            EngineEye::Left,
            backendResourceOutputs.leftAccumulator))
    {
        return failSchedule(
            backend,
            StereoScheduleFailure::LeftAccumulator,
            backendResourceOutputs);
    }
    if (!backend.createFreshAccumulator(
            EngineEye::Right,
            backendResourceOutputs.rightAccumulator))
    {
        return failSchedule(
            backend,
            StereoScheduleFailure::RightAccumulator,
            backendResourceOutputs);
    }
    if (!backendResourceOutputs.valid())
        return failSchedule(
            backend,
            StereoScheduleFailure::InvalidResource,
            backendResourceOutputs);
    if (backendResourceOutputs.leftAccumulator
        == backendResourceOutputs.rightAccumulator)
    {
        return failSchedule(
            backend,
            StereoScheduleFailure::AliasedAccumulators,
            backendResourceOutputs);
    }

    const StereoScheduleResources resources = backendResourceOutputs;

    ScheduledEyeResult eyeResult = renderScheduledEye(
        backend,
        EngineEye::Left,
        resources.leftAccumulator,
        resources.visibleSet);
    if (eyeResult.failure != StereoScheduleFailure::None)
    {
        return failSchedule(
            backend,
            eyeResult.failure,
            resources,
            &eyeResult.isolation,
            EngineEye::Left);
    }

    eyeResult = renderScheduledEye(
        backend,
        EngineEye::Right,
        resources.rightAccumulator,
        resources.visibleSet);
    if (eyeResult.failure != StereoScheduleFailure::None)
    {
        return failSchedule(
            backend,
            eyeResult.failure,
            resources,
            &eyeResult.isolation,
            EngineEye::Right);
    }

    if (!backend.restoreAuthoritativeState())
    {
        backend.discardResources(
            resources.visibleSet,
            resources.leftAccumulator,
            resources.rightAccumulator);
        return { false, StereoScheduleFailure::StateRestore, {} };
    }
    return { true, StereoScheduleFailure::None, resources };
}
}
