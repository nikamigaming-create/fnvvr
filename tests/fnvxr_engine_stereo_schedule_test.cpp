#include "fnvxr_engine_stereo_schedule.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

class FakeBackend final : public fnvxr::engine::StereoScheduleBackend
{
public:
    enum Call : int
    {
        Snapshot = 1,
        CullVisible = 2,
        CreateLeft = 3,
        CreateRight = 4,
        BindLeft = 5,
        PopulateLeft = 6,
        RenderLeft = 7,
        FinalizeLeft = 8,
        EndLeft = 9,
        BindRight = 10,
        PopulateRight = 11,
        RenderRight = 12,
        FinalizeRight = 13,
        EndRight = 14,
        Restore = 15,
        Discard = 16,
    };

    bool aliasAccumulators = false;
    bool failLeftBind = false;
    bool failRightRender = false;
    bool failRestore = false;
    std::vector<int> calls;
    std::uint64_t populatedVisible[2] {};
    std::uint64_t populatedAccumulator[2] {};

    bool snapshotAuthoritativeState() noexcept override
    {
        calls.push_back(Snapshot);
        return true;
    }

    bool buildConservativeVisibleSet(std::uint64_t& visibleSet) noexcept override
    {
        calls.push_back(CullVisible);
        visibleSet = 101;
        return true;
    }

    bool createFreshAccumulator(
        fnvxr::engine::EngineEye eye,
        std::uint64_t& accumulator) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? CreateLeft : CreateRight);
        accumulator = eye == fnvxr::engine::EngineEye::Left || aliasAccumulators ? 201 : 202;
        return true;
    }

    bool bindEyeCameraAndTargets(
        fnvxr::engine::EngineEye eye,
        std::uint64_t accumulator) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? BindLeft : BindRight);
        return accumulator != 0
            && (eye != fnvxr::engine::EngineEye::Left || !failLeftBind);
    }

    bool populateAccumulator(
        fnvxr::engine::EngineEye eye,
        std::uint64_t accumulator,
        std::uint64_t visibleSet) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? PopulateLeft : PopulateRight);
        const std::size_t index = eye == fnvxr::engine::EngineEye::Left ? 0 : 1;
        populatedAccumulator[index] = accumulator;
        populatedVisible[index] = visibleSet;
        return true;
    }

    bool renderAccumulator(
        fnvxr::engine::EngineEye eye,
        std::uint64_t) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? RenderLeft : RenderRight);
        return eye != fnvxr::engine::EngineEye::Right || !failRightRender;
    }

    bool finalizeAccumulator(
        fnvxr::engine::EngineEye eye,
        std::uint64_t) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? FinalizeLeft : FinalizeRight);
        return true;
    }

    bool endEyeIsolation(fnvxr::engine::EngineEye eye) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? EndLeft : EndRight);
        return true;
    }

    bool restoreAuthoritativeState() noexcept override
    {
        calls.push_back(Restore);
        return !failRestore;
    }

    void discardResources(
        std::uint64_t,
        std::uint64_t,
        std::uint64_t) noexcept override
    {
        calls.push_back(Discard);
    }
};
}

int main()
{
    using namespace fnvxr::engine;

    {
        FakeBackend backend;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        const std::vector<int> expected {
            FakeBackend::Snapshot,
            FakeBackend::CullVisible,
            FakeBackend::CreateLeft,
            FakeBackend::CreateRight,
            FakeBackend::BindLeft,
            FakeBackend::PopulateLeft,
            FakeBackend::RenderLeft,
            FakeBackend::FinalizeLeft,
            FakeBackend::EndLeft,
            FakeBackend::BindRight,
            FakeBackend::PopulateRight,
            FakeBackend::RenderRight,
            FakeBackend::FinalizeRight,
            FakeBackend::EndRight,
            FakeBackend::Restore,
        };
        if (!result.complete || result.failure != StereoScheduleFailure::None)
            return fail("known-good engine stereo schedule must complete");
        if (backend.calls != expected)
            return fail("engine stereo call order changed");
        if (backend.populatedVisible[0] != backend.populatedVisible[1]
            || backend.populatedVisible[0] == 0)
        {
            return fail("both eyes must consume the same conservative visible set");
        }
        if (backend.populatedAccumulator[0] == backend.populatedAccumulator[1])
            return fail("each eye must use a fresh accumulator");
    }

    {
        FakeBackend backend;
        backend.aliasAccumulators = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::AliasedAccumulators)
            return fail("aliased eye accumulators must fail closed");
        for (const int call : backend.calls)
        {
            if (call == FakeBackend::BindLeft || call == FakeBackend::PopulateLeft)
                return fail("alias detection must occur before either eye is bound or populated");
        }
    }

    {
        FakeBackend backend;
        backend.failLeftBind = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::LeftBind)
            return fail("partial left bind failure must abort the schedule");
        const std::vector<int> tail {
            FakeBackend::BindLeft,
            FakeBackend::EndLeft,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (backend.calls.size() < tail.size()
            || !std::equal(tail.begin(), tail.end(), backend.calls.end() - tail.size()))
        {
            return fail("bind failure must unwind eye isolation before state restoration");
        }
    }

    {
        FakeBackend backend;
        backend.failRightRender = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::RightRender)
            return fail("right render failure must abort the schedule");
        if (backend.calls.back() != FakeBackend::Discard)
            return fail("failed eye work must restore and discard isolated resources");
        for (const int call : backend.calls)
        {
            if (call == FakeBackend::FinalizeRight)
                return fail("a failed render must not be represented as finalized output");
        }
    }

    {
        FakeBackend backend;
        backend.failRestore = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::StateRestore)
            return fail("state restoration failure must reject completed eye work");
        if (backend.calls.back() != FakeBackend::Discard)
            return fail("state restoration failure must discard both eyes");
    }

    std::cout << "fnvxr engine stereo schedule PASS\n";
    return EXIT_SUCCESS;
}
