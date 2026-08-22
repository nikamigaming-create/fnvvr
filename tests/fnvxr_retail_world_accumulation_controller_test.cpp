#include "fnvxr_retail_world_accumulation_controller.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
enum class Event
{
    Read,
    Claim,
    Prepare,
    Render,
    Publish,
};

struct State
{
    std::vector<Event> events;
    fnvxr::engine::RetailTrackedFrame tracked {};
    fnvxr::engine::abi::RetailSceneGraphLayout scene {};
    fnvxr::engine::abi::RetailNiCameraLayout camera {};
    fnvxr::engine::abi::RetailBSCullingProcessLayout culler {};
    bool mismatch = false;
    bool read = true;
    bool claim = true;
    bool prepare = true;
    bool render = true;
    bool publish = true;
    std::uint64_t nextTransaction = 1u;
    std::uint64_t renderedTransaction = 0u;
    void* renderedScene = nullptr;
    fnvxr::engine::abi::RetailNiCameraLayout* renderedCamera = nullptr;
    fnvxr::engine::abi::RetailBSCullingProcessLayout* renderedCuller = nullptr;
};

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

fnvxr::engine::RetailTrackedFrame gameplayFrame()
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

bool read(void* raw, fnvxr::engine::RetailTrackedFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Read);
    frame = state.tracked;
    return state.read;
}

bool claim(void* raw, std::uint64_t& transaction) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Claim);
    transaction = state.claim ? state.nextTransaction++ : 0u;
    return state.claim;
}

bool prepare(
    void* raw,
    const fnvxr::engine::RetailWorldAccumulationCallFrame& call,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t,
    fnvxr::engine::RetailCenterRuntimeFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Prepare);
    frame.sceneGraph = state.mismatch
        ? nullptr
        : reinterpret_cast<const fnvxr::engine::abi::RetailSceneGraphLayout*>(
            call.sceneObject);
    frame.stockCenterCamera = call.stockCenterCamera;
    frame.gameUnitsPerMeter = 70.0f;
    return state.prepare;
}

fnvxr::engine::RetailCenterRuntimeFrameResult render(
    void* raw,
    const fnvxr::engine::RetailCenterRuntimeFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Render);
    state.renderedTransaction = frame.generation;
    state.renderedScene = frame.sceneObject;
    state.renderedCamera = frame.stockCenterCamera;
    state.renderedCuller = frame.stockCullingProcess;
    if (!state.render)
        return {};
    fnvxr::engine::RetailCenterRuntimeFrameResult result {};
    result.disposition = fnvxr::engine::RetailWorldHookDisposition::
        StereoWorldComplete;
    result.failure = fnvxr::engine::RetailCenterRuntimeFailure::None;
    result.renderer.complete = true;
    result.renderer.failure = fnvxr::engine::CenterRendererFailure::None;
    return result;
}

bool publish(
    void* raw,
    const fnvxr::engine::RetailTrackedFrame&,
    std::uint64_t) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Publish);
    return state.publish;
}

fnvxr::engine::RetailWorldAccumulationControllerOperations operations(
    State& state)
{
    return {
        &state,
        &read,
        &claim,
        &prepare,
        &render,
        &publish,
    };
}
}

int main()
{
    try
    {
        using namespace fnvxr::engine;
        State gameplay;
        gameplay.tracked = gameplayFrame();
        gameplay.nextTransaction = 9u;
        RetailWorldAccumulationController controller;
        require(controller.initialize(operations(gameplay)), "init failed");
        const RetailWorldAccumulationCallFrame call {
            &gameplay.camera,
            &gameplay.scene,
            &gameplay.culler,
        };
        const RetailWorldAccumulationControllerResult result =
            controller.dispatch(call);
        require(
            result.complete()
                && result.disposition
                    == RetailWorldHookDisposition::StereoWorldComplete
                && result.transactionId == 9u
                && gameplay.events == std::vector<Event> {
                    Event::Read,
                    Event::Claim,
                    Event::Prepare,
                    Event::Render,
                    Event::Publish,
                },
            "world transaction did not run strictly inside the callsite scope");
        require(
            gameplay.renderedTransaction == result.transactionId
                && gameplay.renderedScene == &gameplay.scene
                && gameplay.renderedCamera == &gameplay.camera
                && gameplay.renderedCuller == &gameplay.culler,
            "controller did not preserve the exact stock call arguments");

        State ui;
        ui.tracked = gameplayFrame();
        ui.tracked.runtime.phase = fnvxr::shared::RuntimePhaseMenu;
        RetailWorldAccumulationController uiController;
        require(uiController.initialize(operations(ui)), "UI init failed");
        const RetailWorldAccumulationControllerResult uiResult =
            uiController.dispatch({ &ui.camera, &ui.scene, &ui.culler });
        require(
            uiResult.complete()
                && uiResult.disposition
                    == RetailWorldHookDisposition::CallOriginalForUi
                && ui.events == std::vector<Event> { Event::Read },
            "confirmed UI entered the private world transaction");

        State pipBoy;
        pipBoy.tracked = gameplayFrame();
        pipBoy.tracked.runtime.phase = fnvxr::shared::RuntimePhaseMenu;
        pipBoy.tracked.runtime.menuBits =
            fnvxr::shared::RuntimeMenuModeBit
            | fnvxr::shared::RuntimePipBoyMenuBit;
        pipBoy.nextTransaction = 21u;
        RetailWorldAccumulationController pipBoyController;
        require(
            pipBoyController.initialize(operations(pipBoy)),
            "live Pip-Boy init failed");
        const RetailWorldAccumulationControllerResult pipBoyResult =
            pipBoyController.dispatch(
                { &pipBoy.camera, &pipBoy.scene, &pipBoy.culler });
        require(
            pipBoyResult.complete()
                && pipBoyResult.disposition
                    == RetailWorldHookDisposition::StereoWorldComplete
                && pipBoyResult.transactionId == 21u
                && pipBoy.events == std::vector<Event> {
                    Event::Read,
                    Event::Claim,
                    Event::Prepare,
                    Event::Render,
                    Event::Publish,
                },
            "live Pip-Boy did not execute the private binocular transaction");

        State mismatch;
        mismatch.tracked = gameplayFrame();
        mismatch.mismatch = true;
        RetailWorldAccumulationController mismatchController;
        require(
            mismatchController.initialize(operations(mismatch)),
            "mismatch init failed");
        const RetailWorldAccumulationControllerResult mismatchResult =
            mismatchController.dispatch(
                { &mismatch.camera, &mismatch.scene, &mismatch.culler });
        require(
            mismatchResult.failure
                    == RetailWorldAccumulationControllerFailure::
                        CallArgumentsMismatch
                && mismatch.events == std::vector<Event> {
                    Event::Read,
                    Event::Claim,
                    Event::Prepare,
                },
            "mismatched global scene reached the private eye render");

        State unknown;
        unknown.tracked = gameplayFrame();
        unknown.tracked.runtime.phase = fnvxr::shared::RuntimePhaseUnknown;
        RetailWorldAccumulationController unknownController;
        require(
            unknownController.initialize(operations(unknown)),
            "unknown init failed");
        const RetailWorldAccumulationControllerResult unknownResult =
            unknownController.dispatch(
                { &unknown.camera, &unknown.scene, &unknown.culler });
        require(
            unknownResult.failure
                    == RetailWorldAccumulationControllerFailure::
                        TrackedFrameRejected
                && unknown.events == std::vector<Event> { Event::Read },
            "unknown runtime state reached either render route");

        std::cout << "retail AccumulateScene controller tests passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
