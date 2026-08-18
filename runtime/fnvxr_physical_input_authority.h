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
    ControllerConsumerUnacknowledged,
    MenuOwnsInput,
    RuntimeNotGameplay,
    GameplayClassificationUnavailable,
};

struct GameplayAuthorityInput
{
    bool physicalHeadsetPlayRequested = false;
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
    const std::int64_t magnitudeX = leftThumbX < 0
        ? -static_cast<std::int64_t>(leftThumbX)
        : static_cast<std::int64_t>(leftThumbX);
    const std::int64_t magnitudeY = leftThumbY < 0
        ? -static_cast<std::int64_t>(leftThumbY)
        : static_cast<std::int64_t>(leftThumbY);
    // Consumer thumbsticks are not perfectly cardinal. Suppress the smaller
    // axis when the larger one is at least 1.5x stronger, while retaining a
    // deliberate diagonal when both axes have comparable magnitude.
    const bool suppressX = magnitudeY * 2 >= magnitudeX * 3;
    const bool suppressY = magnitudeX * 2 >= magnitudeY * 3;
    return {
        !suppressY && leftThumbY > effectiveDeadzone,
        !suppressY && leftThumbY < -effectiveDeadzone,
        !suppressX && leftThumbX < -effectiveDeadzone,
        !suppressX && leftThumbX > effectiveDeadzone,
    };
}

constexpr bool radialAnalogRunHeld(
    std::int32_t leftThumbX,
    std::int32_t leftThumbY,
    std::int32_t threshold) noexcept
{
    const std::int64_t magnitudeX = leftThumbX < 0
        ? -static_cast<std::int64_t>(leftThumbX)
        : static_cast<std::int64_t>(leftThumbX);
    const std::int64_t magnitudeY = leftThumbY < 0
        ? -static_cast<std::int64_t>(leftThumbY)
        : static_cast<std::int64_t>(leftThumbY);
    const std::int64_t effectiveThreshold = threshold < 0 ? 0 : threshold;
    return magnitudeX >= effectiveThreshold || magnitudeY >= effectiveThreshold;
}

struct SnapTurnLatch
{
    bool armed = true;

    constexpr int update(
        std::int32_t rightThumbX,
        std::int32_t pressThreshold,
        std::int32_t releaseThreshold) noexcept
    {
        const std::int32_t press = pressThreshold < 0 ? 0 : pressThreshold;
        const std::int32_t release = releaseThreshold < 0 ? 0 : releaseThreshold;
        if (!armed)
        {
            if (rightThumbX >= -release && rightThumbX <= release)
                armed = true;
            return 0;
        }
        if (rightThumbX <= -press)
        {
            armed = false;
            return -1;
        }
        if (rightThumbX >= press)
        {
            armed = false;
            return 1;
        }
        return 0;
    }

    constexpr void reset() noexcept
    {
        armed = true;
    }
};
}
