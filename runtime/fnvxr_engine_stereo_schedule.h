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
    StereoScheduleResources resources {};
};

class StereoScheduleBackend
{
public:
    virtual ~StereoScheduleBackend() = default;

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
        std::uint64_t accumulator) noexcept = 0;
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
    virtual bool endEyeIsolation(EngineEye eye) noexcept = 0;

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
    bool eyeIsolationActive = false,
    EngineEye activeEye = EngineEye::Left) noexcept
{
    if (eyeIsolationActive)
        backend.endEyeIsolation(activeEye);
    if (!backend.restoreAuthoritativeState())
        failure = StereoScheduleFailure::StateRestore;
    backend.discardResources(
        resources.visibleSet,
        resources.leftAccumulator,
        resources.rightAccumulator);
    return { false, failure, resources };
}

inline StereoScheduleFailure renderScheduledEye(
    StereoScheduleBackend& backend,
    EngineEye eye,
    std::uint64_t accumulator,
    std::uint64_t visibleSet) noexcept
{
    const bool left = eye == EngineEye::Left;
    if (!backend.bindEyeCameraAndTargets(eye, accumulator))
        return left ? StereoScheduleFailure::LeftBind : StereoScheduleFailure::RightBind;
    if (!backend.populateAccumulator(eye, accumulator, visibleSet))
        return left ? StereoScheduleFailure::LeftPopulate : StereoScheduleFailure::RightPopulate;
    if (!backend.renderAccumulator(eye, accumulator))
        return left ? StereoScheduleFailure::LeftRender : StereoScheduleFailure::RightRender;
    if (!backend.finalizeAccumulator(eye, accumulator))
        return left ? StereoScheduleFailure::LeftFinalize : StereoScheduleFailure::RightFinalize;
    if (!backend.endEyeIsolation(eye))
        return left ? StereoScheduleFailure::LeftEnd : StereoScheduleFailure::RightEnd;
    return StereoScheduleFailure::None;
}

inline bool eyeIsolationRemainsActive(StereoScheduleFailure failure)
{
    return failure == StereoScheduleFailure::LeftBind
        || failure == StereoScheduleFailure::LeftPopulate
        || failure == StereoScheduleFailure::LeftRender
        || failure == StereoScheduleFailure::LeftFinalize
        || failure == StereoScheduleFailure::RightBind
        || failure == StereoScheduleFailure::RightPopulate
        || failure == StereoScheduleFailure::RightRender
        || failure == StereoScheduleFailure::RightFinalize;
}

inline StereoScheduleResult executeStereoSchedule(
    StereoScheduleBackend& backend) noexcept
{
    StereoScheduleResources resources {};
    if (!backend.snapshotAuthoritativeState())
    {
        backend.discardResources(0, 0, 0);
        return { false, StereoScheduleFailure::Snapshot, resources };
    }

    if (!backend.buildConservativeVisibleSet(resources.visibleSet))
        return failSchedule(backend, StereoScheduleFailure::ConservativeVisibility, resources);
    if (!backend.createFreshAccumulator(EngineEye::Left, resources.leftAccumulator))
        return failSchedule(backend, StereoScheduleFailure::LeftAccumulator, resources);
    if (!backend.createFreshAccumulator(EngineEye::Right, resources.rightAccumulator))
        return failSchedule(backend, StereoScheduleFailure::RightAccumulator, resources);
    if (!resources.valid())
        return failSchedule(backend, StereoScheduleFailure::InvalidResource, resources);
    if (resources.leftAccumulator == resources.rightAccumulator)
        return failSchedule(backend, StereoScheduleFailure::AliasedAccumulators, resources);

    StereoScheduleFailure failure = renderScheduledEye(
        backend,
        EngineEye::Left,
        resources.leftAccumulator,
        resources.visibleSet);
    if (failure != StereoScheduleFailure::None)
    {
        return failSchedule(
            backend,
            failure,
            resources,
            eyeIsolationRemainsActive(failure),
            EngineEye::Left);
    }

    failure = renderScheduledEye(
        backend,
        EngineEye::Right,
        resources.rightAccumulator,
        resources.visibleSet);
    if (failure != StereoScheduleFailure::None)
    {
        return failSchedule(
            backend,
            failure,
            resources,
            eyeIsolationRemainsActive(failure),
            EngineEye::Right);
    }

    if (!backend.restoreAuthoritativeState())
    {
        backend.discardResources(
            resources.visibleSet,
            resources.leftAccumulator,
            resources.rightAccumulator);
        return { false, StereoScheduleFailure::StateRestore, resources };
    }
    return { true, StereoScheduleFailure::None, resources };
}
}
