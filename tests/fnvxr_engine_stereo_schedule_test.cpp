#include "fnvxr_engine_stereo_schedule.h"

#include <algorithm>
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

bool exposesNoPublicResources(
    const fnvxr::engine::StereoScheduleResult& result) noexcept
{
    return !result.resources.valid()
        && result.resources.visibleSet == 0
        && result.resources.leftAccumulator == 0
        && result.resources.rightAccumulator == 0;
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
        RollbackLeft = 17,
        RollbackRight = 18,
    };

    bool failSnapshot = false;
    bool aliasAccumulators = false;
    bool failLeftBindBeforeMutation = false;
    bool failLeftBindAfterMutation = false;
    bool rewriteEyeDuringLeftBind = false;
    bool rewriteEyeDuringLeftEnd = false;
    bool omitLeftIsolationToken = false;
    bool rewriteRetainedResourcesDuringLeftBind = false;
    bool failRightRender = false;
    bool failRightEnd = false;
    bool failRestore = false;
    std::vector<int> calls;
    std::uint64_t populatedVisible[2] {};
    std::uint64_t populatedAccumulator[2] {};
    std::uint64_t* retainedVisibleSet = nullptr;
    std::uint64_t* retainedLeftAccumulator = nullptr;
    std::uint64_t* retainedRightAccumulator = nullptr;
    fnvxr::engine::StereoScheduleResources discardedResources {};

    bool snapshotAuthoritativeState() noexcept override
    {
        calls.push_back(Snapshot);
        return !failSnapshot;
    }

    bool buildConservativeVisibleSet(std::uint64_t& visibleSet) noexcept override
    {
        calls.push_back(CullVisible);
        visibleSet = 101;
        retainedVisibleSet = &visibleSet;
        return true;
    }

    bool createFreshAccumulator(
        fnvxr::engine::EngineEye eye,
        std::uint64_t& accumulator) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? CreateLeft : CreateRight);
        accumulator = eye == fnvxr::engine::EngineEye::Left || aliasAccumulators ? 201 : 202;
        if (eye == fnvxr::engine::EngineEye::Left)
            retainedLeftAccumulator = &accumulator;
        else
            retainedRightAccumulator = &accumulator;
        return true;
    }

    bool bindEyeCameraAndTargets(
        fnvxr::engine::EngineEye eye,
        std::uint64_t accumulator,
        fnvxr::engine::EyeIsolationToken& isolation) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? BindLeft : BindRight);
        if (accumulator == 0
            || (eye == fnvxr::engine::EngineEye::Left && failLeftBindBeforeMutation))
        {
            return false;
        }
        if (eye == fnvxr::engine::EngineEye::Left
            && rewriteRetainedResourcesDuringLeftBind
            && retainedVisibleSet
            && retainedLeftAccumulator
            && retainedRightAccumulator)
        {
            *retainedVisibleSet = 901;
            *retainedLeftAccumulator = 902;
            *retainedRightAccumulator = 902;
        }
        if (eye == fnvxr::engine::EngineEye::Left && rewriteEyeDuringLeftBind)
        {
            // The hostile backend returns opaque state claiming the other eye.
            // Cleanup identity must still come from the caller's explicit eye.
            isolation.backendToken = 302;
            return false;
        }
        if (eye != fnvxr::engine::EngineEye::Left || !omitLeftIsolationToken)
            isolation.backendToken = eye == fnvxr::engine::EngineEye::Left ? 301 : 302;
        return eye != fnvxr::engine::EngineEye::Left || !failLeftBindAfterMutation;
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

    bool endEyeIsolation(
        fnvxr::engine::EngineEye eye,
        fnvxr::engine::EyeIsolationToken& isolation) noexcept override
    {
        calls.push_back(eye == fnvxr::engine::EngineEye::Left ? EndLeft : EndRight);
        if (eye == fnvxr::engine::EngineEye::Left && rewriteEyeDuringLeftEnd)
        {
            // Simulate an end callback replacing its opaque state with a token
            // associated with the other eye before reporting failure.
            isolation.backendToken = 302;
            return false;
        }
        if (eye == fnvxr::engine::EngineEye::Right && failRightEnd)
            return false;
        isolation.backendToken = 0;
        return true;
    }

    void rollbackEyeIsolation(
        fnvxr::engine::EngineEye eye,
        fnvxr::engine::EyeIsolationToken& isolation) noexcept override
    {
        calls.push_back(
            eye == fnvxr::engine::EngineEye::Left
                ? RollbackLeft
                : RollbackRight);
        isolation.backendToken = 0;
    }

    bool restoreAuthoritativeState() noexcept override
    {
        calls.push_back(Restore);
        return !failRestore;
    }

    void discardResources(
        std::uint64_t visibleSet,
        std::uint64_t leftAccumulator,
        std::uint64_t rightAccumulator) noexcept override
    {
        calls.push_back(Discard);
        discardedResources = { visibleSet, leftAccumulator, rightAccumulator };
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
        if (!result.resources.valid())
            return fail("completed engine stereo schedule must expose valid resources");
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
        backend.failSnapshot = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::Snapshot)
            return fail("failed authoritative-state snapshot must abort the schedule");
        if (!exposesNoPublicResources(result))
            return fail("snapshot failure must expose no public resources");
        const std::vector<int> expected {
            FakeBackend::Snapshot,
            FakeBackend::Discard,
        };
        if (backend.calls != expected)
        {
            return fail(
                "pure snapshot failure must not restore state or perform engine work");
        }
    }

    {
        FakeBackend backend;
        backend.rewriteRetainedResourcesDuringLeftBind = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (!result.complete || result.failure != StereoScheduleFailure::None)
            return fail("retained-resource success fixture must complete");
        if (backend.populatedVisible[0] != 101
            || backend.populatedVisible[1] != 101
            || backend.populatedAccumulator[0] != 201
            || backend.populatedAccumulator[1] != 202)
        {
            return fail("both eyes must use the original frozen nonaliased resources");
        }
        if (result.resources.visibleSet != 101
            || result.resources.leftAccumulator != 201
            || result.resources.rightAccumulator != 202)
        {
            return fail("successful public result must expose the original frozen resources");
        }
    }

    {
        FakeBackend backend;
        backend.rewriteRetainedResourcesDuringLeftBind = true;
        backend.failRightRender = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::RightRender)
            return fail("retained-resource cleanup fixture must fail during right render");
        if (backend.discardedResources.visibleSet != 101
            || backend.discardedResources.leftAccumulator != 201
            || backend.discardedResources.rightAccumulator != 202)
        {
            return fail("retained out-parameter rewrite must not change cleanup handles");
        }
        if (!exposesNoPublicResources(result))
            return fail("retained-resource failure must still expose no public resources");
    }

    {
        FakeBackend backend;
        backend.aliasAccumulators = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::AliasedAccumulators)
            return fail("aliased eye accumulators must fail closed");
        if (!exposesNoPublicResources(result))
            return fail("post-allocation alias failure must expose no public resources");
        for (const int call : backend.calls)
        {
            if (call == FakeBackend::BindLeft || call == FakeBackend::PopulateLeft)
                return fail("alias detection must occur before either eye is bound or populated");
        }
    }

    {
        FakeBackend backend;
        backend.failLeftBindBeforeMutation = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::LeftBind)
            return fail("pre-mutation left bind failure must abort the schedule");
        if (!exposesNoPublicResources(result))
            return fail("pre-mutation bind failure must expose no public resources");
        const std::vector<int> tail {
            FakeBackend::BindLeft,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (backend.calls.size() < tail.size()
            || !std::equal(tail.begin(), tail.end(), backend.calls.end() - tail.size()))
        {
            return fail("pre-mutation bind failure must restore without invented cleanup state");
        }
        for (const int call : backend.calls)
        {
            if (call == FakeBackend::RollbackLeft)
                return fail("rollback must not run when bind never activated isolation");
        }
    }

    {
        FakeBackend backend;
        backend.omitLeftIsolationToken = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::LeftIsolationToken)
            return fail("a successful bind without an isolation token must be rejected");
        if (!exposesNoPublicResources(result))
            return fail("missing isolation token failure must expose no public resources");
        for (const int call : backend.calls)
        {
            if (call == FakeBackend::PopulateLeft || call == FakeBackend::RollbackLeft)
                return fail("missing isolation token must stop before populate without fake rollback");
        }
    }

    {
        FakeBackend backend;
        backend.failLeftBindAfterMutation = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::LeftBind)
            return fail("partial left bind failure must abort the schedule");
        if (!exposesNoPublicResources(result))
            return fail("partial bind failure must expose no public resources");
        const std::vector<int> tail {
            FakeBackend::BindLeft,
            FakeBackend::RollbackLeft,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (backend.calls.size() < tail.size()
            || !std::equal(tail.begin(), tail.end(), backend.calls.end() - tail.size()))
        {
            return fail("partial bind must roll back its explicit isolation token");
        }
    }

    {
        FakeBackend backend;
        backend.rewriteEyeDuringLeftBind = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::LeftBind)
            return fail("left bind eye rewrite must reject the schedule");
        const std::vector<int> tail {
            FakeBackend::BindLeft,
            FakeBackend::RollbackLeft,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (backend.calls.size() < tail.size()
            || !std::equal(tail.begin(), tail.end(), backend.calls.end() - tail.size()))
        {
            return fail("bind callback must not redirect cleanup away from the caller-owned eye");
        }
    }

    {
        FakeBackend backend;
        backend.failRightRender = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::RightRender)
            return fail("right render failure must abort the schedule");
        if (!exposesNoPublicResources(result))
            return fail("post-allocation render failure must expose no public resources");
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
        backend.rewriteEyeDuringLeftEnd = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::LeftEnd)
            return fail("left isolation end eye rewrite must reject the schedule");
        const std::vector<int> tail {
            FakeBackend::EndLeft,
            FakeBackend::RollbackLeft,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (backend.calls.size() < tail.size()
            || !std::equal(tail.begin(), tail.end(), backend.calls.end() - tail.size()))
        {
            return fail("end callback must not redirect cleanup away from the caller-owned eye");
        }
    }

    {
        FakeBackend backend;
        backend.failRightEnd = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::RightEnd)
            return fail("failed right isolation close must reject the schedule");
        if (!exposesNoPublicResources(result))
            return fail("isolation close failure must expose no public resources");
        const std::vector<int> tail {
            FakeBackend::EndRight,
            FakeBackend::RollbackRight,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (backend.calls.size() < tail.size()
            || !std::equal(tail.begin(), tail.end(), backend.calls.end() - tail.size()))
        {
            return fail("failed isolation close must roll back the still-active token");
        }
    }

    {
        FakeBackend backend;
        backend.failRestore = true;
        const StereoScheduleResult result = executeStereoSchedule(backend);
        if (result.complete || result.failure != StereoScheduleFailure::StateRestore)
            return fail("state restoration failure must reject completed eye work");
        if (!exposesNoPublicResources(result))
            return fail("state restoration failure must expose no public resources");
        if (backend.calls.back() != FakeBackend::Discard)
            return fail("state restoration failure must discard both eyes");
    }

    std::cout << "fnvxr engine stereo schedule PASS\n";
    return EXIT_SUCCESS;
}
