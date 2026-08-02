#pragma once

#include "../protocol/fnvxr_shared_state.h"

#include <cstdint>
#include <string_view>

namespace fnvxr::engine::stereo_visual_trial_automation
{
// This gate grants no general command or input authority. An explicitly
// requested visual trial can either load the one verified Goodsprings retail
// save, once, or create one fixed disposable character through the xNVSE
// no-save COC path. The fresh-character sequence never drives a menu: it
// enters one fixed cell, assigns one fixed name, and writes one new fixed save
// name. The caller never constructs a console command from supplied text.
struct ApprovedRetailSave
{
    std::string_view name {};
    std::string_view loadCommand {};
};

inline constexpr std::string_view FreshCharacterName =
    "FNVXR_StereoTest";
// This save was created through the fixed no-save COC route in Goodsprings.
// It is the only load target for supervised visual trials. Historical saves
// remain untouched on disk, but cannot be selected by this authority.
inline constexpr std::string_view FreshCharacterLoadCommand =
    "load FNVXR_StereoTest";
inline constexpr std::string_view FreshCharacterStartCommand =
    "coc Goodsprings";
inline constexpr std::string_view FreshCharacterSetNameCommand =
    "player.SetName FNVXR_StereoTest";
inline constexpr std::string_view FreshCharacterSaveCommand =
    "save FNVXR_StereoTest";
// This is intentionally not a general MessageMenu capability. It names only
// the exact stock DLC notifications observed from the verified Goodsprings
// fixture's temporary official-master profile, and is used only with a
// separate, explicit launcher opt-in.
struct OfficialPackNotification
{
    std::string_view title {};
    std::string_view body {};
};

inline constexpr OfficialPackNotification OfficialPackNotifications[] = {
    { "Tribal Pack", "Tribal Pack items added to inventory." },
    { "Classic Pack", "Classic Pack items added to inventory." },
    { "Mercenary Pack", "Mercenary Pack items added to inventory." },
    { "Caravan Pack", "Caravan Pack items added to inventory." },
    { "Gun Runners' Arsenal",
      "The ongoing conflict in the Mojave Wasteland has kicked weapon "
      "manufacturers into high gear! All major and minor weapon dealers in "
      "the region are rolling in new weapons, ammunition types, and "
      "modifications. Head to your nearest participating vendor to peruse "
      "the merchandise!" },
};
inline constexpr std::string_view TribalPackNotificationText =
    OfficialPackNotifications[0].body;
inline constexpr std::string_view MessageMenuOkText = "OK";
inline constexpr ApprovedRetailSave ApprovedRetailSaves[] = {
    { FreshCharacterName, FreshCharacterLoadCommand },
};
constexpr const ApprovedRetailSave* findApprovedRetailSave(
    std::string_view name) noexcept
{
    for (const ApprovedRetailSave& candidate : ApprovedRetailSaves)
    {
        if (candidate.name == name)
            return &candidate;
    }
    return nullptr;
}

// The caller must independently prove the current visible menu contains the
// exact official-pack notification and exactly one visible native first-button
// (index zero) OK tile.  This tiny gate grants no keyboard, mouse,
// controller, simulator, or general menu authority.
constexpr bool exactOfficialPackAcknowledgementAuthorized(
    bool explicitlyOptedIn,
    bool visibleMessageMenu,
    bool exactNotificationObserved,
    bool exactlyOneFirstButtonOk,
    bool alreadyAttempted) noexcept
{
    return explicitlyOptedIn
        && visibleMessageMenu
        && exactNotificationObserved
        && exactlyOneFirstButtonOk
        && !alreadyAttempted;
}

enum class Action : std::uint8_t
{
    None = 0u,
    LoadFixedRecoverySave,
    StartFreshCharacter,
    NameFreshCharacter,
    SaveFreshCharacter,
};

enum class Stage : std::uint8_t
{
    AwaitingStartMenuCommand = 0u,
    AwaitingFreshGameplayName,
    AwaitingFreshGameplaySave,
    Complete,
};

enum class Failure : std::uint8_t
{
    None = 0u,
    ExplicitOptInRequired,
    ActionNotPermitted,
    RecoverySaveNameMismatch,
    FreshCharacterCommandMismatch,
    UnexpectedArgument,
    SequenceComplete,
    InvalidState,
};

struct State
{
    Stage stage = Stage::AwaitingStartMenuCommand;
};

struct Request
{
    bool explicitlyOptedIn = false;
    Action action = Action::None;
    // Used only to select an exact static command. It can never become a
    // caller-controlled prefix.
    std::string_view argument {};
};

struct Decision
{
    bool authorized = false;
    Failure failure = Failure::ExplicitOptInRequired;
    State nextState {};
    // The caller must execute this exact gate-owned command, never rebuild a
    // command from Request::argument.
    std::string_view command {};
};

namespace detail
{
constexpr Decision reject(
    const State& state,
    Failure failure) noexcept
{
    return { false, failure, state, {} };
}

constexpr Decision authorize(
    Stage nextStage,
    std::string_view command) noexcept
{
    return { true, Failure::None, { nextStage }, command };
}
}

constexpr Decision decide(
    const State& state,
    const Request& request) noexcept
{
    if (!request.explicitlyOptedIn)
        return detail::reject(state, Failure::ExplicitOptInRequired);

    switch (state.stage)
    {
    case Stage::AwaitingStartMenuCommand:
    {
        if (request.action == Action::LoadFixedRecoverySave)
        {
            const ApprovedRetailSave* approvedSave =
                findApprovedRetailSave(request.argument);
            if (approvedSave == nullptr)
                return detail::reject(state, Failure::RecoverySaveNameMismatch);
            return detail::authorize(
                Stage::Complete,
                approvedSave->loadCommand);
        }
        if (request.action == Action::StartFreshCharacter)
        {
            if (request.argument != FreshCharacterStartCommand)
            {
                return detail::reject(
                    state,
                    Failure::FreshCharacterCommandMismatch);
            }
            return detail::authorize(
                Stage::AwaitingFreshGameplayName,
                FreshCharacterStartCommand);
        }
        return detail::reject(state, Failure::ActionNotPermitted);
    }

    case Stage::AwaitingFreshGameplayName:
        if (request.action != Action::NameFreshCharacter)
            return detail::reject(state, Failure::ActionNotPermitted);
        if (!request.argument.empty())
            return detail::reject(state, Failure::UnexpectedArgument);
        return detail::authorize(
            Stage::AwaitingFreshGameplaySave,
            FreshCharacterSetNameCommand);

    case Stage::AwaitingFreshGameplaySave:
        if (request.action != Action::SaveFreshCharacter)
            return detail::reject(state, Failure::ActionNotPermitted);
        if (!request.argument.empty())
            return detail::reject(state, Failure::UnexpectedArgument);
        return detail::authorize(
            Stage::Complete,
            FreshCharacterSaveCommand);

    case Stage::Complete:
        return detail::reject(state, Failure::SequenceComplete);
    }

    return detail::reject(state, Failure::InvalidState);
}
}
