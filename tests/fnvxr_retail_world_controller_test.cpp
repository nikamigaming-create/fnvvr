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
    Original,
    Read,
    Claim,
    Prepare,
    Render,
    Publish,
};

struct State
{
    std::vector<Event> events;
    fnvxr::engine::RetailTrackedFrame frame {};
    fnvxr::engine::abi::RetailSceneGraphLayout preparedSceneGraph {};
    fnvxr::engine::abi::RetailNiCameraLayout preparedCamera {};
    const fnvxr::engine::abi::RetailSceneGraphLayout* renderedSceneGraph =
        nullptr;
    void* renderedSceneObject = nullptr;
    fnvxr::engine::abi::RetailNiCameraLayout* renderedCamera = nullptr;
    bool read = true;
    bool original = true;
    bool claim = true;
    bool prepare = true;
    bool render = true;
    bool publish = true;
    std::uint64_t nextTransactionId = 1u;
    std::uint64_t forcedTransactionId = 0u;
    std::uint64_t preparedTransactionId = 0u;
    std::uint64_t publishedTransactionId = 0u;
};

fnvxr::engine::RetailTrackedFrame validGameplayFrame()
{
    using namespace fnvxr;
    engine::RetailTrackedFrame frame {};
    frame.pose.magic = shared::VrPoseSharedMagic;
    frame.pose.version = shared::VrPoseSharedVersion;
    frame.pose.frame = 4u;
    frame.pose.predictedDisplayTime = 77;
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
    frame.runtime.frame = 4u;
    frame.runtime.phase = shared::RuntimePhaseGameplay;
    frame.runtime.cameraActive = 1u;
    frame.poseSequence = 2;
    frame.runtimeSequence = 2;
    return frame;
}

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool original(void* raw, const fnvxr::engine::RetailWorldHookDispatchFrame&) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Original);
    return state.original;
}

bool read(void* raw, fnvxr::engine::RetailTrackedFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Read);
    frame = state.frame;
    return state.read;
}

bool claimWorldTransaction(void* raw, std::uint64_t& transaction) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Claim);
    transaction = 0u;
    if (!state.claim || state.nextTransactionId == 0u)
        return false;
    transaction = state.forcedTransactionId != 0u
        ? state.forcedTransactionId
        : state.nextTransactionId++;
    return true;
}

bool prepare(
    void* raw,
    const fnvxr::engine::RetailWorldHookDispatchFrame&,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t transaction,
    fnvxr::engine::RetailCenterRuntimeFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Prepare);
    state.preparedTransactionId = transaction;
    frame.sceneGraph = &state.preparedSceneGraph;
    frame.stockCenterCamera = &state.preparedCamera;
    frame.generation = transaction;
    frame.gameUnitsPerMeter = 70.0f;
    return state.prepare;
}

fnvxr::engine::RetailCenterRuntimeFrameResult render(
    void* raw,
    const fnvxr::engine::RetailCenterRuntimeFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Render);
    state.renderedSceneGraph = frame.sceneGraph;
    state.renderedSceneObject = frame.sceneObject;
    state.renderedCamera = frame.stockCenterCamera;
    if (!state.render)
        return {};
    fnvxr::engine::RetailCenterRuntimeFrameResult result {};
    result.disposition =
        fnvxr::engine::RetailWorldHookDisposition::StereoWorldComplete;
    result.failure = fnvxr::engine::RetailCenterRuntimeFailure::None;
    result.renderer.complete = true;
    result.renderer.failure = fnvxr::engine::CenterRendererFailure::None;
    return result;
}

bool publish(
    void* raw,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t transaction) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Publish);
    state.publishedTransactionId = transaction;
    return state.publish;
}

fnvxr::engine::RetailWorldControllerOperations operations(State& state)
{
    return {
        &state,
        original,
        read,
        claimWorldTransaction,
        prepare,
        render,
        publish,
    };
}
}

int main()
{
    using namespace fnvxr::engine;
    // RenderWorldSceneGraph's hook context is renderer-owned. It must not be
    // recast as the global SceneGraph that the camera provider has resolved.
    abi::RetailBSShaderAccumulatorLayout hookAccumulator {};
    RetailWorldHookDispatchFrame hook {};
    hook.retailThis = &hookAccumulator;
    hook.arguments.sharedRenderObjectAddress = 1u;
    hook.originalTrampolineAddress = 2u;

    State gameplay;
    gameplay.frame = validGameplayFrame();
    gameplay.nextTransactionId = 9u;
    RetailWorldController controller;
    require(controller.initialize(operations(gameplay)), "init failed");
    const RetailWorldControllerResult result = controller.dispatch(hook);
    require(
        result.complete()
            && result.disposition
                == RetailWorldHookDisposition::StereoWorldComplete
            && result.transactionId == 9u,
        "gameplay transaction did not complete");
    require(
        gameplay.events == std::vector<Event> {
            Event::Read,
            Event::Original,
            Event::Claim,
            Event::Prepare,
            Event::Render,
            Event::Publish,
        },
        "gameplay operations were reordered");
    require(
        gameplay.preparedTransactionId == result.transactionId
            && gameplay.publishedTransactionId == result.transactionId,
        "camera preparation and publication lost their claimed world identity");
    require(
        gameplay.renderedSceneGraph == &gameplay.preparedSceneGraph
            && gameplay.renderedSceneObject
                == static_cast<void*>(&gameplay.preparedSceneGraph)
            && gameplay.renderedCamera == &gameplay.preparedCamera
            && gameplay.renderedSceneGraph
                != reinterpret_cast<const abi::RetailSceneGraphLayout*>(
                    hook.retailThis)
            && gameplay.renderedSceneObject
                != reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(
                        hook.arguments.sharedRenderObjectAddress)),
        "controller did not preserve the provider's global SceneGraph as the cull root");

    State ui;
    ui.frame = validGameplayFrame();
    ui.frame.runtime.phase = fnvxr::shared::RuntimePhaseMenu;
    RetailWorldController uiController;
    require(uiController.initialize(operations(ui)), "UI init failed");
    const RetailWorldControllerResult uiResult = uiController.dispatch(hook);
    require(
        uiResult.complete()
            && uiResult.disposition
                == RetailWorldHookDisposition::CallOriginalForUi
            && ui.events == std::vector<Event> { Event::Read, Event::Original },
        "confirmed UI did not call the original renderer after reading runtime state");

    // A confirmed UI runtime state owns the UI route even when the opaque
    // SceneGraph word contains an arbitrary nonzero value.
    State runtimeUi;
    runtimeUi.frame = validGameplayFrame();
    runtimeUi.nextTransactionId = 17u;
    runtimeUi.frame.runtime.phase = fnvxr::shared::RuntimePhaseMenu;
    RetailWorldController runtimeUiController;
    require(
        runtimeUiController.initialize(operations(runtimeUi)),
        "runtime-UI controller init failed");
    const RetailWorldControllerResult runtimeUiResult =
        runtimeUiController.dispatch(hook);
    require(
        runtimeUiResult.complete()
            && runtimeUiResult.disposition
                == RetailWorldHookDisposition::CallOriginalForUi
            && runtimeUiResult.transactionId == 0u
            && runtimeUi.events == std::vector<Event> {
                Event::Read,
                Event::Original,
            },
        "confirmed runtime UI entered the binocular world route");
    runtimeUi.events.clear();
    runtimeUi.frame = validGameplayFrame();
    const RetailWorldControllerResult resumedGameplay =
        runtimeUiController.dispatch(hook);
    require(
        resumedGameplay.complete()
            && resumedGameplay.disposition
                == RetailWorldHookDisposition::StereoWorldComplete
            && resumedGameplay.transactionId == 17u
            && runtimeUi.events == std::vector<Event> {
                Event::Read,
                Event::Original,
                Event::Claim,
                Event::Prepare,
                Event::Render,
                Event::Publish,
            },
        "world route did not resume cleanly after UI transition");

    State unknown;
    unknown.frame = validGameplayFrame();
    unknown.frame.runtime.phase = fnvxr::shared::RuntimePhaseUnknown;
    RetailWorldController unknownController;
    require(unknownController.initialize(operations(unknown)), "unknown controller init failed");
    const RetailWorldControllerResult unknownResult =
        unknownController.dispatch(hook);
    require(
        unknownResult.failure
                == RetailWorldControllerFailure::TrackedFrameRejected
            && unknown.events == std::vector<Event> { Event::Read },
        "unknown runtime reached either presentation route");

    State failed;
    failed.publish = false;
    failed.frame = validGameplayFrame();
    RetailWorldController failedController;
    require(failedController.initialize(operations(failed)), "failure init failed");
    const RetailWorldControllerResult failedResult =
        failedController.dispatch(hook);
    require(
        failedResult.failure
                == RetailWorldControllerFailure::GpuPublicationRejected
            && failedResult.disposition
                == RetailWorldHookDisposition::RejectGameplayFrame
            && failed.events.back() == Event::Publish,
        "GPU publication failure did not fail gameplay closed");

    State claimFailure;
    claimFailure.frame = validGameplayFrame();
    claimFailure.claim = false;
    RetailWorldController claimFailureController;
    require(
        claimFailureController.initialize(operations(claimFailure)),
        "claim-failure init failed");
    const RetailWorldControllerResult claimFailureResult =
        claimFailureController.dispatch(hook);
    require(
        claimFailureResult.failure
                == RetailWorldControllerFailure::TransactionClaimRejected
            && claimFailureResult.disposition
                == RetailWorldHookDisposition::RejectGameplayFrame
            && claimFailure.events == std::vector<Event> {
                Event::Read,
                Event::Original,
                Event::Claim,
            },
        "world rendering continued after a shared transaction claim failure");

    // A claimed identity remains consumed even when the later publication
    // fails. Reusing it could let a subsequent pair masquerade as the failed
    // render transaction at the compositor boundary.
    State reusedIdentity;
    reusedIdentity.frame = validGameplayFrame();
    reusedIdentity.forcedTransactionId = 20u;
    reusedIdentity.publish = false;
    RetailWorldController reusedIdentityController;
    require(
        reusedIdentityController.initialize(operations(reusedIdentity)),
        "reused-identity init failed");
    const RetailWorldControllerResult firstReuseAttempt =
        reusedIdentityController.dispatch(hook);
    require(
        firstReuseAttempt.failure
                == RetailWorldControllerFailure::GpuPublicationRejected
            && firstReuseAttempt.transactionId == 20u,
        "failed world transaction did not retain its claimed identity");
    reusedIdentity.events.clear();
    reusedIdentity.publish = true;
    const RetailWorldControllerResult secondReuseAttempt =
        reusedIdentityController.dispatch(hook);
    require(
        secondReuseAttempt.failure
                == RetailWorldControllerFailure::TransactionClaimRejected
            && secondReuseAttempt.transactionId == 0u
            && reusedIdentity.events == std::vector<Event> {
                Event::Read,
                Event::Original,
                Event::Claim,
            },
        "a failed world transaction identity was reused by a later render");

    State preludeFailure;
    preludeFailure.frame = validGameplayFrame();
    preludeFailure.original = false;
    RetailWorldController preludeFailureController;
    require(
        preludeFailureController.initialize(operations(preludeFailure)),
        "prelude-failure init failed");
    const RetailWorldControllerResult preludeFailureResult =
        preludeFailureController.dispatch(hook);
    require(
        preludeFailureResult.failure
                == RetailWorldControllerFailure::OriginalRetailPassRejected
            && preludeFailureResult.transactionId == 0u
            && preludeFailure.events == std::vector<Event> {
                Event::Read,
                Event::Original,
            },
        "a rejected stock gameplay prelude reached the eye transaction");

    std::cout << "retail world controller ordering passed\n";
    return EXIT_SUCCESS;
}
