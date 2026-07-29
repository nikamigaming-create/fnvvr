#include "../runtime/fnvxr_headset_demo_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using namespace fnvxr::engine::headset_demo;

    State state {};
    Input input {};
    input.selected = true;
    input.frame = 10u;
    input.fixtureReady = true;
    state = advance(state, input).next;
    require(state.stage == Stage::WaitingForGameplay, "fixture did not enter gameplay wait");

    input.fixtureReady = false;
    input.gameplay = true;
    input.frame = 20u;
    input.gameplayWarmupFrames = 3u;
    input.pipBoyHoldFrames = 5u;
    state = advance(state, input).next;
    require(state.stage == Stage::WaitingForGameplay, "gameplay warmup opened Pip-Boy immediately");

    // A DLC/message interruption restarts the warmup.  The demo may not
    // issue its fixed Tab while startup modals are still cycling.
    input.frame = 21u;
    input.gameplay = false;
    state = advance(state, input).next;
    require(
        state.stage == Stage::WaitingForGameplay && state.stageFrame == 0u,
        "gameplay interruption did not reset the Pip-Boy warmup");

    input.frame = 22u;
    input.gameplay = true;
    state = advance(state, input).next;
    require(
        state.stage == Stage::WaitingForGameplay && state.stageFrame == 22u,
        "post-interruption gameplay did not start a fresh warmup");

    input.frame = 25u;
    input.inGameTapAvailable = false;
    require(
        advance(state, input).action == Action::None,
        "unavailable in-game tap opened Pip-Boy");
    input.inGameTapAvailable = true;
    Decision opened = advance(state, input);
    require(
        opened.action == Action::OpenPipBoy
            && opened.next.stage == Stage::WaitingForPipBoy,
        "bounded gameplay sequence did not issue its one Pip-Boy open tap");
    state = opened.next;

    input.frame = 26u;
    input.pipBoyVisible = false;
    require(
        advance(state, input).action == Action::None,
        "Pip-Boy close tap occurred before visibility was observed");
    input.pipBoyVisible = true;
    state = advance(state, input).next;
    require(state.stage == Stage::HoldingPipBoy, "visible Pip-Boy did not enter bounded hold");

    input.frame = 30u;
    require(
        advance(state, input).action == Action::None,
        "Pip-Boy close tap occurred before the bounded hold elapsed");
    input.frame = 31u;
    Decision closed = advance(state, input);
    require(
        closed.action == Action::ClosePipBoy
            && closed.next.stage == Stage::WaitingForGameplayAfterClose,
        "bounded UI sequence did not issue its one Pip-Boy close tap");
    state = closed.next;

    input.frame = 32u;
    input.pipBoyVisible = false;
    state = advance(state, input).next;
    require(state.stage == Stage::Complete, "Pip-Boy close did not return to gameplay completion");
    require(
        advance(state, input).action == Action::None,
        "completed demo issued a third input action");

    std::cout << "headset demo authority passed\n";
    return 0;
}
