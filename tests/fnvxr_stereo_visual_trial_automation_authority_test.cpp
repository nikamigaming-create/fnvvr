#include "fnvxr_stereo_visual_trial_automation_authority.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
namespace automation = fnvxr::engine::stereo_visual_trial_automation;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

automation::Request recoveryLoadRequest(
    std::string_view saveName = automation::FreshCharacterName)
{
    automation::Request request {};
    request.explicitlyOptedIn = true;
    request.action = automation::Action::LoadFixedRecoverySave;
    request.argument = saveName;
    return request;
}

automation::Request freshCharacterStartRequest(
    std::string_view command = automation::FreshCharacterStartCommand)
{
    automation::Request request {};
    request.explicitlyOptedIn = true;
    request.action = automation::Action::StartFreshCharacter;
    request.argument = command;
    return request;
}

automation::Request fixedInternalRequest(automation::Action action)
{
    automation::Request request {};
    request.explicitlyOptedIn = true;
    request.action = action;
    return request;
}

void requireRejected(
    const automation::Decision& decision,
    automation::Failure expected,
    automation::Stage unchanged,
    const char* message)
{
    require(!decision.authorized, message);
    require(decision.failure == expected,
        "rejection did not preserve the exact failure");
    require(decision.command.empty(),
        "rejection leaked an executable command");
    require(decision.nextState.stage == unchanged,
        "rejection advanced the one-shot state");
}
}

int main()
{
    using automation::Action;
    using automation::Failure;
    using automation::Stage;

    constexpr automation::State initial {};
    constexpr automation::Request defaultRequest {};
    constexpr automation::Decision defaultDecision =
        automation::decide(initial, defaultRequest);
    static_assert(!defaultDecision.authorized);
    static_assert(
        defaultDecision.failure == Failure::ExplicitOptInRequired);
    static_assert(defaultDecision.command.empty());
    static_assert(
        defaultDecision.nextState.stage == Stage::AwaitingStartMenuCommand);
    static_assert(
        automation::findApprovedRetailSave(
            automation::FreshCharacterName) != nullptr);
    static_assert(
        automation::findApprovedRetailSave("FNVXR_HostExitRecovery") == nullptr);
    static_assert(
        automation::findApprovedRetailSave("Nikami10mmGoodsprings222") == nullptr);
    static_assert(
        automation::findApprovedRetailSave("OtherSave") == nullptr);
    static_assert(
        automation::FreshCharacterName == "FNVXR_StereoTest");
    static_assert(
        automation::TribalPackNotificationText
            == "Tribal Pack items added to inventory.");
    static_assert(
        sizeof(automation::OfficialPackNotifications)
            / sizeof(automation::OfficialPackNotifications[0]) == 5u);
    static_assert(
        automation::OfficialPackNotifications[1].title == "Classic Pack");
    static_assert(
        automation::OfficialPackNotifications[1].body
            == "Classic Pack items added to inventory.");
    static_assert(
        automation::OfficialPackNotifications[4].title
            == "Gun Runners' Arsenal");
    static_assert(
        automation::OfficialPackNotifications[4].body
            == "The ongoing conflict in the Mojave Wasteland has kicked weapon "
               "manufacturers into high gear! All major and minor weapon dealers in "
               "the region are rolling in new weapons, ammunition types, and "
               "modifications. Head to your nearest participating vendor to peruse "
               "the merchandise!");
    static_assert(automation::MessageMenuOkText == "OK");
    static_assert(automation::exactOfficialPackAcknowledgementAuthorized(
        true, true, true, true, false));
    static_assert(!automation::exactOfficialPackAcknowledgementAuthorized(
        false, true, true, true, false));
    static_assert(!automation::exactOfficialPackAcknowledgementAuthorized(
        true, false, true, true, false));
    static_assert(!automation::exactOfficialPackAcknowledgementAuthorized(
        true, true, false, true, false));
    static_assert(!automation::exactOfficialPackAcknowledgementAuthorized(
        true, true, true, false, false));
    static_assert(!automation::exactOfficialPackAcknowledgementAuthorized(
        true, true, true, true, true));

    automation::Request request = recoveryLoadRequest();
    automation::Decision decision = automation::decide(initial, request);
    require(decision.authorized && decision.failure == Failure::None,
        "explicit exact verified Goodsprings-save load was rejected");
    require(decision.command == automation::FreshCharacterLoadCommand,
        "verified Goodsprings load authorization did not return the fixed command");
    require(
        decision.nextState.stage == Stage::Complete,
        "verified Goodsprings load authorization did not advance exactly one stage");
    const automation::State afterLoad = decision.nextState;

    request = recoveryLoadRequest();
    request.explicitlyOptedIn = false;
    requireRejected(
        automation::decide(initial, request),
        Failure::ExplicitOptInRequired,
        Stage::AwaitingStartMenuCommand,
        "recovery load was authorized without explicit opt-in");

    constexpr std::string_view wrongSaveNames[] = {
        "",
        "FNVXR_HostExitRecovery",
        "FNVXR_HostExitRecover",
        "FNVXR_HostExitRecovery2",
        "Nikami10mmGoodsprings222",
        "fnvxr_HostExitRecovery",
        "load FNVXR_HostExitRecovery",
        "OtherSave",
    };
    for (const std::string_view wrongSave : wrongSaveNames)
    {
        request = recoveryLoadRequest();
        request.argument = wrongSave;
        requireRejected(
            automation::decide(initial, request),
            Failure::RecoverySaveNameMismatch,
            Stage::AwaitingStartMenuCommand,
            "a non-exact recovery save name was authorized");
    }

    request = recoveryLoadRequest();
    request.action = Action::None;
    requireRejected(
        automation::decide(initial, request),
        Failure::ActionNotPermitted,
        Stage::AwaitingStartMenuCommand,
        "empty action was authorized");
    request = recoveryLoadRequest();
    request.action = static_cast<Action>(0xffu);
    requireRejected(
        automation::decide(initial, request),
        Failure::ActionNotPermitted,
        Stage::AwaitingStartMenuCommand,
        "unknown action was authorized");

    request = freshCharacterStartRequest();
    decision = automation::decide(initial, request);
    require(decision.authorized && decision.failure == Failure::None,
        "fixed fresh-character start was rejected");
    require(decision.command == automation::FreshCharacterStartCommand,
        "fresh-character start leaked or changed the fixed COC command");
    require(
        decision.nextState.stage == Stage::AwaitingFreshGameplayName,
        "fresh-character start did not wait for observed gameplay before naming");
    const automation::State afterFreshStart = decision.nextState;

    request = freshCharacterStartRequest("coc Novac");
    requireRejected(
        automation::decide(initial, request),
        Failure::FreshCharacterCommandMismatch,
        Stage::AwaitingStartMenuCommand,
        "a caller-controlled fresh-character COC command was authorized");

    request = fixedInternalRequest(Action::NameFreshCharacter);
    decision = automation::decide(afterFreshStart, request);
    require(decision.authorized && decision.failure == Failure::None,
        "fixed fresh-character naming was rejected after fresh gameplay");
    require(decision.command == automation::FreshCharacterSetNameCommand,
        "fresh-character name command changed from the fixed identity");
    require(
        decision.nextState.stage == Stage::AwaitingFreshGameplaySave,
        "fresh-character naming did not wait before saving");
    const automation::State afterFreshName = decision.nextState;

    request.argument = "FNVXR_OtherName";
    requireRejected(
        automation::decide(afterFreshStart, request),
        Failure::UnexpectedArgument,
        Stage::AwaitingFreshGameplayName,
        "caller-controlled fresh-character name was authorized");

    request = fixedInternalRequest(Action::SaveFreshCharacter);
    decision = automation::decide(afterFreshName, request);
    require(decision.authorized && decision.failure == Failure::None,
        "fixed fresh-character save was rejected after naming");
    require(decision.command == automation::FreshCharacterSaveCommand,
        "fresh-character save command changed from the fixed save name");
    require(decision.nextState.stage == Stage::Complete,
        "fresh-character save did not complete the one-shot authority");

    request = fixedInternalRequest(Action::NameFreshCharacter);
    requireRejected(
        automation::decide(initial, request),
        Failure::ActionNotPermitted,
        Stage::AwaitingStartMenuCommand,
        "fresh-character naming was authorized before a fresh game existed");
    request = fixedInternalRequest(Action::SaveFreshCharacter);
    requireRejected(
        automation::decide(afterFreshStart, request),
        Failure::ActionNotPermitted,
        Stage::AwaitingFreshGameplayName,
        "fresh-character save was authorized before fixed naming");

    request = recoveryLoadRequest();
    requireRejected(
        automation::decide(afterLoad, request),
        Failure::SequenceComplete,
        Stage::Complete,
        "a second retail-save load was authorized after completion");
    request = freshCharacterStartRequest();
    requireRejected(
        automation::decide(decision.nextState, request),
        Failure::SequenceComplete,
        Stage::Complete,
        "a second fresh character was authorized after completion");

    const automation::State invalid {
        static_cast<Stage>(0xffu),
    };
    requireRejected(
        automation::decide(invalid, recoveryLoadRequest()),
        Failure::InvalidState,
        static_cast<Stage>(0xffu),
        "invalid authority state admitted an action");

    std::cout
        << "stereo visual-trial automation authority gate passed\n";
    return EXIT_SUCCESS;
}
