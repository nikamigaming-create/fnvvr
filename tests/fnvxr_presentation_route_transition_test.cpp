#include "fnvxr_retail_ui_quad_capture.h"
#include "fnvxr_retail_world_controller.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
enum class Event
{
    WorldRead,
    OriginalRetailPass,
    WorldClaim,
    WorldPrepare,
    WorldRender,
    WorldPublish,
    UiRead,
    UiCopy,
    UiPublish,
    UiWithhold,
};

struct State
{
    std::vector<Event> events;
    fnvxr::engine::RetailTrackedFrame frame {};
    fnvxr::engine::abi::RetailSceneGraphLayout preparedSceneGraph {};
    fnvxr::engine::abi::RetailNiCameraLayout preparedCamera {};
    fnvxr::d3d9::RetailUiQuadCaptureFailure lastUiWithhold =
        fnvxr::d3d9::RetailUiQuadCaptureFailure::None;
    std::uint32_t worldPublications = 0u;
    std::uint32_t uiPublications = 0u;
    std::uint32_t uiWithholds = 0u;
    std::uint64_t nextTransactionId = 1u;
    std::uint64_t lastWorldTransactionId = 0u;
    std::uint64_t lastUiTransactionId = 0u;
};

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

fnvxr::engine::RetailTrackedFrame validGameplayFrame(
    std::uint64_t frameNumber,
    LONG sequence)
{
    using namespace fnvxr;
    engine::RetailTrackedFrame frame {};
    frame.pose.magic = shared::VrPoseSharedMagic;
    frame.pose.version = shared::VrPoseSharedVersion;
    frame.pose.frame = frameNumber;
    frame.pose.predictedDisplayTime = 100 + static_cast<std::int64_t>(frameNumber);
    frame.pose.hmdRot[3] = 1.0f;
    frame.pose.leftEyeRot[3] = 1.0f;
    frame.pose.rightEyeRot[3] = 1.0f;
    frame.pose.leftEyePos[0] = -0.032f;
    frame.pose.rightEyePos[0] = 0.032f;
    constexpr float fov[4] { -0.8f, 0.8f, 0.75f, -0.75f };
    for (std::size_t index = 0u; index < 4u; ++index)
    {
        frame.pose.leftFov[index] = fov[index];
        frame.pose.rightFov[index] = fov[index];
    }
    frame.pose.trackingFlags = shared::VrPoseTrackingHmd;
    frame.pose.referenceSpaceGeneration = 2u;
    frame.pose.producerEpoch = 3u;
    frame.runtime.magic = shared::RuntimeSharedMagic;
    frame.runtime.version = shared::RuntimeSharedVersion;
    frame.runtime.frame = frameNumber;
    frame.runtime.phase = shared::RuntimePhaseGameplay;
    frame.runtime.cameraActive = 1u;
    frame.poseSequence = sequence;
    frame.runtimeSequence = sequence;
    return frame;
}

bool worldRead(void* raw, fnvxr::engine::RetailTrackedFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::WorldRead);
    frame = state.frame;
    return true;
}

bool originalRetailPass(
    void* raw,
    const fnvxr::engine::RetailWorldHookDispatchFrame&) noexcept
{
    static_cast<State*>(raw)->events.push_back(Event::OriginalRetailPass);
    return true;
}

bool claimWorldTransaction(void* raw, std::uint64_t& transactionId) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::WorldClaim);
    transactionId = 0u;
    if (state.nextTransactionId == 0u)
        return false;
    transactionId = state.nextTransactionId++;
    return true;
}

bool prepareWorld(
    void* raw,
    const fnvxr::engine::RetailWorldHookDispatchFrame&,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t transactionId,
    fnvxr::engine::RetailCenterRuntimeFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::WorldPrepare);
    frame.sceneGraph = &state.preparedSceneGraph;
    frame.stockCenterCamera = &state.preparedCamera;
    frame.generation = transactionId;
    frame.gameUnitsPerMeter = 70.0f;
    return true;
}

fnvxr::engine::RetailCenterRuntimeFrameResult renderWorld(
    void* raw,
    const fnvxr::engine::RetailCenterRuntimeFrame&) noexcept
{
    static_cast<State*>(raw)->events.push_back(Event::WorldRender);
    fnvxr::engine::RetailCenterRuntimeFrameResult result {};
    result.disposition = fnvxr::engine::RetailWorldHookDisposition::StereoWorldComplete;
    result.failure = fnvxr::engine::RetailCenterRuntimeFailure::None;
    result.renderer.complete = true;
    result.renderer.failure = fnvxr::engine::CenterRendererFailure::None;
    return result;
}

bool publishWorld(
    void* raw,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t transactionId) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::WorldPublish);
    state.lastWorldTransactionId = transactionId;
    ++state.worldPublications;
    return true;
}

bool uiRead(void* raw, fnvxr::engine::RetailTrackedFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::UiRead);
    frame = state.frame;
    return true;
}

bool uiCopy(void* raw, void*) noexcept
{
    static_cast<State*>(raw)->events.push_back(Event::UiCopy);
    return true;
}

bool publishUi(
    void* raw,
    const fnvxr::engine::RetailTrackedFrame&) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::UiPublish);
    if (state.nextTransactionId == 0u)
        return false;
    state.lastUiTransactionId = state.nextTransactionId++;
    ++state.uiPublications;
    return true;
}

void withholdUi(
    void* raw,
    fnvxr::d3d9::RetailUiQuadCaptureFailure failure) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::UiWithhold);
    state.lastUiWithhold = failure;
    ++state.uiWithholds;
}
}

int main()
{
    using namespace fnvxr;
    State state;
    state.frame = validGameplayFrame(10u, 2);
    state.nextTransactionId = 31u;

    engine::RetailWorldController world;
    const engine::RetailWorldControllerOperations worldOperations {
        &state,
        &originalRetailPass,
        &worldRead,
        &claimWorldTransaction,
        &prepareWorld,
        &renderWorld,
        &publishWorld,
    };
    require(world.initialize(worldOperations), "world controller init failed");

    d3d9::RetailUiQuadCaptureController ui;
    const d3d9::RetailUiQuadCaptureOperations uiOperations {
        &state,
        &uiRead,
        &uiCopy,
        &publishUi,
        &withholdUi,
    };
    require(ui.initialize(uiOperations), "UI quad controller init failed");

    engine::abi::RetailBSShaderAccumulatorLayout hookAccumulator {};
    engine::RetailWorldHookDispatchFrame hook {};
    hook.retailThis = &hookAccumulator;
    hook.arguments.sharedRenderObjectAddress = 1u;
    hook.originalTrampolineAddress = 2u;
    void* authorizedDevice = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(1u));

    const engine::RetailWorldControllerResult firstWorld = world.dispatch(hook);
    require(!ui.beforePresent(authorizedDevice), "gameplay frame reached the UI quad capture path");
    require(
        firstWorld.complete()
            && firstWorld.disposition
                == engine::RetailWorldHookDisposition::StereoWorldComplete
            && firstWorld.transactionId == 31u
            && state.events == std::vector<Event> {
                Event::WorldRead,
                Event::OriginalRetailPass,
                Event::WorldClaim,
                Event::WorldPrepare,
                Event::WorldRender,
                Event::WorldPublish,
                Event::UiRead,
                Event::UiWithhold,
            }
            && state.lastUiWithhold
                == d3d9::RetailUiQuadCaptureFailure::RuntimeNotConfirmedUi,
        "gameplay did not select world-only presentation");

    state.events.clear();
    state.frame = validGameplayFrame(11u, 4);
    state.frame.runtime.phase = shared::RuntimePhaseMenu;
    const engine::RetailWorldControllerResult menu = world.dispatch(hook);
    require(ui.beforePresent(authorizedDevice), "confirmed menu did not publish a UI quad");
    require(
        menu.complete()
            && menu.disposition
                == engine::RetailWorldHookDisposition::CallOriginalForUi
            && menu.transactionId == 0u
            && state.events == std::vector<Event> {
                Event::WorldRead,
                Event::OriginalRetailPass,
                Event::UiRead,
                Event::UiCopy,
                Event::UiPublish,
            }
            && state.worldPublications == 1u
            && state.uiPublications == 1u,
        "confirmed menu overlapped the world renderer instead of using the UI quad");

    state.events.clear();
    state.frame = validGameplayFrame(12u, 6);
    const engine::RetailWorldControllerResult resumedWorld = world.dispatch(hook);
    require(!ui.beforePresent(authorizedDevice), "resumed gameplay retained the UI quad");
    require(
        resumedWorld.complete()
            && resumedWorld.disposition
                == engine::RetailWorldHookDisposition::StereoWorldComplete
            && resumedWorld.transactionId == 33u
            && state.events == std::vector<Event> {
                Event::WorldRead,
                Event::OriginalRetailPass,
                Event::WorldClaim,
                Event::WorldPrepare,
                Event::WorldRender,
                Event::WorldPublish,
                Event::UiRead,
                Event::UiWithhold,
            }
            && state.worldPublications == 2u
            && state.uiPublications == 1u
            && state.uiWithholds == 2u
            && state.lastUiTransactionId == 32u
            && state.lastWorldTransactionId == 33u
            && state.lastUiWithhold
                == d3d9::RetailUiQuadCaptureFailure::RuntimeNotConfirmedUi,
        "world presentation did not resume cleanly after menu exit");

    // The SceneGraph word at 0xE8 is opaque, not a menu flag. A nonzero value
    // must not override the authenticated gameplay publication or flatten a
    // confirmed world frame into a UI path.
    state.events.clear();
    state.frame = validGameplayFrame(13u, 8);
    state.preparedSceneGraph.opaqueUnknownB8 = 0xDEADBEEFu;
    const engine::RetailWorldControllerResult opaqueFieldWorld =
        world.dispatch(hook);
    require(
        !ui.beforePresent(authorizedDevice),
        "opaque SceneGraph data published a stale UI quad");
    require(
        opaqueFieldWorld.complete()
            && opaqueFieldWorld.disposition
                == engine::RetailWorldHookDisposition::StereoWorldComplete
            && opaqueFieldWorld.transactionId == 34u
            && state.events == std::vector<Event> {
                Event::WorldRead,
                Event::OriginalRetailPass,
                Event::WorldClaim,
                Event::WorldPrepare,
                Event::WorldRender,
                Event::WorldPublish,
                Event::UiRead,
                Event::UiWithhold,
            }
            && state.worldPublications == 3u
            && state.uiPublications == 1u
            && state.uiWithholds == 3u
            && state.lastUiTransactionId == 32u
            && state.lastWorldTransactionId == 34u
            && state.lastUiWithhold
                == d3d9::RetailUiQuadCaptureFailure::RuntimeNotConfirmedUi,
        "opaque SceneGraph data altered the confirmed world presentation route");

    std::cout << "presentation route transition passed\n";
    return EXIT_SUCCESS;
}
