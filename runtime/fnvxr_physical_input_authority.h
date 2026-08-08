#pragma once

#include <cstdint>

namespace fnvxr::physical_input
{
// Physical gameplay input must be driven by the current runtime classifier,
// not by whether the renderer has already accepted a presentation frame.  The
// latter can legitimately lag while the first engine-center transaction is
// becoming available, whereas it is never authority for moving the player.
enum class GameplayAuthorityBlocker : std::uint8_t
{
    None = 0u,
    PhysicalPlayNotRequested,
    InputFocusLost,
    ControllerConsumerUnacknowledged,
    MenuOwnsInput,
    RuntimeNotGameplay,
    GameplayClassificationUnavailable,
};

struct GameplayAuthorityInput
{
    bool physicalHeadsetPlayRequested = false;
    bool inputFocused = false;
    bool controllerConsumerAcknowledged = false;
    bool menuOwnsInput = false;
    bool runtimeGameplay = false;
    bool gameplayClassified = false;
};

struct GameplayAuthorityDecision
{
    GameplayAuthorityBlocker blocker =
        GameplayAuthorityBlocker::PhysicalPlayNotRequested;

    constexpr bool granted() const noexcept
    {
        return blocker == GameplayAuthorityBlocker::None;
    }
};

constexpr GameplayAuthorityDecision assessGameplayAuthority(
    const GameplayAuthorityInput& input) noexcept
{
    if (!input.physicalHeadsetPlayRequested)
        return { GameplayAuthorityBlocker::PhysicalPlayNotRequested };
    if (!input.inputFocused)
        return { GameplayAuthorityBlocker::InputFocusLost };
    if (!input.controllerConsumerAcknowledged)
        return { GameplayAuthorityBlocker::ControllerConsumerUnacknowledged };
    if (input.menuOwnsInput)
        return { GameplayAuthorityBlocker::MenuOwnsInput };
    if (!input.runtimeGameplay)
        return { GameplayAuthorityBlocker::RuntimeNotGameplay };
    if (!input.gameplayClassified)
        return { GameplayAuthorityBlocker::GameplayClassificationUnavailable };
    return { GameplayAuthorityBlocker::None };
}

constexpr const char* gameplayAuthorityBlockerName(
    GameplayAuthorityBlocker blocker) noexcept
{
    switch (blocker)
    {
        case GameplayAuthorityBlocker::None: return "granted";
        case GameplayAuthorityBlocker::PhysicalPlayNotRequested:
            return "physical-play-not-requested";
        case GameplayAuthorityBlocker::InputFocusLost:
            return "input-focus-lost";
        case GameplayAuthorityBlocker::ControllerConsumerUnacknowledged:
            return "controller-consumer-unacknowledged";
        case GameplayAuthorityBlocker::MenuOwnsInput:
            return "menu-owns-input";
        case GameplayAuthorityBlocker::RuntimeNotGameplay:
            return "runtime-not-gameplay";
        case GameplayAuthorityBlocker::GameplayClassificationUnavailable:
            return "gameplay-classification-unavailable";
    }
    return "invalid";
}

enum class LocomotionDelivery : std::uint8_t
{
    SharedInputQueue = 0u,
    InProcessNvseDirectInput,
};

constexpr LocomotionDelivery selectLocomotionDelivery(
    bool physicalHeadsetPlayRequested) noexcept
{
    return physicalHeadsetPlayRequested
        ? LocomotionDelivery::InProcessNvseDirectInput
        : LocomotionDelivery::SharedInputQueue;
}

constexpr const char* locomotionDeliveryName(
    LocomotionDelivery delivery) noexcept
{
    switch (delivery)
    {
        case LocomotionDelivery::SharedInputQueue: return "shared-input-queue";
        case LocomotionDelivery::InProcessNvseDirectInput:
            return "nvse-directinput-hold";
    }
    return "invalid";
}

struct LocomotionIntent
{
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;

    constexpr bool any() const noexcept
    {
        return forward || backward || left || right;
    }
};

constexpr LocomotionIntent classifyLocomotion(
    std::int32_t leftThumbX,
    std::int32_t leftThumbY,
    std::int32_t deadzone) noexcept
{
    const std::int32_t effectiveDeadzone = deadzone < 0 ? 0 : deadzone;
    return {
        leftThumbY > effectiveDeadzone,
        leftThumbY < -effectiveDeadzone,
        leftThumbX < -effectiveDeadzone,
        leftThumbX > effectiveDeadzone,
    };
}
}
