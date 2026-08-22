#pragma once

#include <cstdint>

namespace fnvxr::engine::headset_demo
{
// The demo is deliberately a tiny, deterministic in-game sequence: after an
// owned fixture reaches real gameplay, one native engine action opens Pip-Boy,
// it stays visible for a bounded interval, and one engine action closes it.
// This policy
// cannot express movement, aiming, firing, arbitrary menu navigation, desktop
// input, controller input, or simulator control.
enum class Stage : std::uint8_t
{
    WaitingForFixture,
    WaitingForGameplay,
    WaitingForPipBoy,
    HoldingPipBoy,
    WaitingForGameplayAfterClose,
    Complete,
};

enum class Action : std::uint8_t
{
    None,
    OpenPipBoy,
    ClosePipBoy,
};

struct State
{
    Stage stage = Stage::WaitingForFixture;
    std::uint64_t stageFrame = 0u;
};

struct Input
{
    bool selected = false;
    bool fixtureReady = false;
    bool gameplay = false;
    bool pipBoyVisible = false;
    bool engineActionAvailable = false;
    std::uint64_t frame = 0u;
    std::uint64_t gameplayWarmupFrames = 0u;
    std::uint64_t pipBoyHoldFrames = 0u;
};

struct Decision
{
    State next {};
    Action action = Action::None;
};

constexpr Decision advance(const State& state, const Input& input) noexcept
{
    if (!input.selected || input.frame == 0u)
        return { state, Action::None };

    State next = state;
    switch (state.stage)
    {
    case Stage::WaitingForFixture:
        if (input.fixtureReady)
        {
            next.stage = Stage::WaitingForGameplay;
            next.stageFrame = 0u;
        }
        return { next, Action::None };

    case Stage::WaitingForGameplay:
        // A retail DLC/message modal can briefly return to gameplay between
        // notifications while its startup scripts are still busy.  Do not
        // count that earlier gameplay toward the fixed Pip-Boy proof: the
        // demo must see one uninterrupted gameplay interval before it issues
        // its first (and only) in-game Tab tap.
        if (!input.gameplay || input.pipBoyVisible)
        {
            next.stageFrame = 0u;
            return { next, Action::None };
        }
        if (next.stageFrame == 0u)
        {
            next.stageFrame = input.frame;
            return { next, Action::None };
        }
        if (input.frame < next.stageFrame + input.gameplayWarmupFrames
            || !input.engineActionAvailable)
        {
            return { next, Action::None };
        }
        next.stage = Stage::WaitingForPipBoy;
        next.stageFrame = input.frame;
        return { next, Action::OpenPipBoy };

    case Stage::WaitingForPipBoy:
        if (input.pipBoyVisible)
        {
            next.stage = Stage::HoldingPipBoy;
            next.stageFrame = input.frame;
        }
        return { next, Action::None };

    case Stage::HoldingPipBoy:
        if (!input.pipBoyVisible)
        {
            next.stage = Stage::WaitingForGameplay;
            next.stageFrame = 0u;
            return { next, Action::None };
        }
        if (input.frame < next.stageFrame + input.pipBoyHoldFrames
            || !input.engineActionAvailable)
        {
            return { next, Action::None };
        }
        next.stage = Stage::WaitingForGameplayAfterClose;
        next.stageFrame = input.frame;
        return { next, Action::ClosePipBoy };

    case Stage::WaitingForGameplayAfterClose:
        if (input.gameplay && !input.pipBoyVisible)
        {
            next.stage = Stage::Complete;
            next.stageFrame = input.frame;
        }
        return { next, Action::None };

    case Stage::Complete:
        return { next, Action::None };
    }
    return { next, Action::None };
}
}
