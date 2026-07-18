#include "fnvxr_retail_world_controller.h"

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
    Prepare,
    Render,
    Publish,
};

struct State
{
    std::vector<Event> events;
    bool read = true;
    bool prepare = true;
    bool render = true;
    bool publish = true;
};

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool original(void* raw, const fnvxr::engine::RetailWorldHookDispatchFrame&) noexcept
{
    static_cast<State*>(raw)->events.push_back(Event::Original);
    return true;
}

bool read(void* raw, fnvxr::engine::RetailTrackedFrame& frame) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Read);
    frame.pose.frame = 4u;
    return state.read;
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
    frame.generation = transaction;
    frame.gameUnitsPerMeter = 70.0f;
    return state.prepare;
}

fnvxr::engine::RetailCenterRuntimeFrameResult render(
    void* raw,
    const fnvxr::engine::RetailCenterRuntimeFrame&) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Render);
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
    std::uint64_t) noexcept
{
    State& state = *static_cast<State*>(raw);
    state.events.push_back(Event::Publish);
    return state.publish;
}

fnvxr::engine::RetailWorldControllerOperations operations(State& state)
{
    return { &state, original, read, prepare, render, publish };
}
}

int main()
{
    using namespace fnvxr::engine;
    abi::RetailSceneGraphLayout sceneGraph {};
    sceneGraph.camera = 3u;
    RetailWorldHookDispatchFrame hook {};
    hook.retailThis = &sceneGraph;
    hook.arguments.sharedRenderObjectAddress = 1u;
    hook.originalTrampolineAddress = 2u;

    State gameplay;
    RetailWorldController controller;
    require(controller.initialize(operations(gameplay), 9u), "init failed");
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
            Event::Prepare,
            Event::Render,
            Event::Publish,
        },
        "gameplay operations were reordered");

    State ui;
    sceneGraph.isMenuSceneGraph = 1u;
    RetailWorldController uiController;
    require(uiController.initialize(operations(ui)), "UI init failed");
    const RetailWorldControllerResult uiResult = uiController.dispatch(hook);
    require(
        uiResult.complete()
            && uiResult.disposition
                == RetailWorldHookDisposition::CallOriginalForUi
            && ui.events == std::vector<Event> { Event::Original },
        "UI did not call only the original renderer");

    State failed;
    failed.publish = false;
    sceneGraph.isMenuSceneGraph = 0u;
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

    std::cout << "retail world controller ordering passed\n";
    return EXIT_SUCCESS;
}
