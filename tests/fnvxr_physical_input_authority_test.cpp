#include "fnvxr_physical_input_authority.h"

#include <iostream>

namespace
{
int g_failures = 0;

void expect(bool condition, const char* description)
{
    if (condition)
        return;

    std::cerr << "FAILED: " << description << "\n";
    ++g_failures;
}

fnvxr::physical_input::GameplayAuthorityInput physicalGameplayInput()
{
    return {
        true,
        true,
        true,
        false,
        true,
        true,
    };
}
}

int main()
{
    using fnvxr::physical_input::GameplayAuthorityBlocker;
    using fnvxr::physical_input::LocomotionDelivery;

    const auto granted = fnvxr::physical_input::assessGameplayAuthority(
        physicalGameplayInput());
    expect(granted.granted(),
        "physical locomotion is authorized by focused, acknowledged runtime gameplay");

    auto input = physicalGameplayInput();
    input.physicalHeadsetPlayRequested = false;
    expect(
        fnvxr::physical_input::assessGameplayAuthority(input).blocker
            == GameplayAuthorityBlocker::PhysicalPlayNotRequested,
        "physical profile opt-in is required");

    input = physicalGameplayInput();
    input.inputFocused = false;
    expect(
        fnvxr::physical_input::assessGameplayAuthority(input).blocker
            == GameplayAuthorityBlocker::InputFocusLost,
        "focus loss revokes physical locomotion");

    input = physicalGameplayInput();
    input.controllerConsumerAcknowledged = false;
    expect(
        fnvxr::physical_input::assessGameplayAuthority(input).blocker
            == GameplayAuthorityBlocker::ControllerConsumerUnacknowledged,
        "consumer acknowledgement is required");

    input = physicalGameplayInput();
    input.menuOwnsInput = true;
    expect(
        fnvxr::physical_input::assessGameplayAuthority(input).blocker
            == GameplayAuthorityBlocker::MenuOwnsInput,
        "a menu revokes physical locomotion");

    input = physicalGameplayInput();
    input.runtimeGameplay = false;
    expect(
        fnvxr::physical_input::assessGameplayAuthority(input).blocker
            == GameplayAuthorityBlocker::RuntimeNotGameplay,
        "runtime gameplay is required independently of rendering");

    input = physicalGameplayInput();
    input.gameplayClassified = false;
    expect(
        fnvxr::physical_input::assessGameplayAuthority(input).blocker
            == GameplayAuthorityBlocker::GameplayClassificationUnavailable,
        "an unclassified runtime cannot move the player");

    expect(
        fnvxr::physical_input::selectLocomotionDelivery(true)
            == LocomotionDelivery::InProcessNvseDirectInput,
        "physical locomotion uses the xNVSE DirectInput consumer");
    expect(
        fnvxr::physical_input::selectLocomotionDelivery(false)
            == LocomotionDelivery::SharedInputQueue,
        "nonphysical locomotion retains the shared queue");

    const auto forward = fnvxr::physical_input::classifyLocomotion(0, 9001, 9000);
    expect(forward.forward && !forward.backward && !forward.left && !forward.right,
        "forward intent crosses the deadzone");
    const auto backwardLeft =
        fnvxr::physical_input::classifyLocomotion(-9001, -9001, 9000);
    expect(backwardLeft.backward && backwardLeft.left
            && !backwardLeft.forward && !backwardLeft.right,
        "diagonal backward-left intent preserves both axes");
    const auto neutral = fnvxr::physical_input::classifyLocomotion(9000, -9000, 9000);
    expect(!neutral.any(), "deadzone boundary is neutral");

    if (g_failures == 0)
    {
        std::cout << "physical input authority tests passed\n";
        return 0;
    }
    return 1;
}
