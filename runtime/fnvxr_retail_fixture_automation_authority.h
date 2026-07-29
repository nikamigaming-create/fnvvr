#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fnvxr::engine::retail_fixture_automation
{
// Owned fixture automation is deliberately separate from historical visual-
// trial saves. It owns only the two explicit product prefixes, so an
// automation configuration can never name a player's existing save. The TTW
// prefix has a separate save lineage but uses the same base-game trait IDs.
inline constexpr std::string_view SaveNamePrefix = "FNVXR_AutoRetail_";
inline constexpr std::string_view TtwSaveNamePrefix = "FNVXR_AutoTTW_";
inline constexpr std::string_view CreateStartCommand = "coc Goodsprings";
inline constexpr std::string_view FixturePlayerName = "FNVXR_AutoRetail";
inline constexpr std::string_view SetFixturePlayerNameCommand =
    "player.SetName FNVXR_AutoRetail";
inline constexpr std::string_view LoadCommandPrefix = "load ";
inline constexpr std::string_view SaveCommandPrefix = "save ";
// This stock command is deliberately available only to the exact official-pack
// modal fallback below. It is never derived from launcher input or a save name.
inline constexpr std::string_view CloseExactOfficialPackMessageCommand =
    "CloseAllMenus";
inline constexpr std::uint32_t MaxExactOfficialPackCloseAttemptsPerRun = 8u;
// TTW 3.3.3 emits this exact native MessageMenu when its optional Stewie
// dependency is absent.  The isolated VR fixture intentionally does not load
// that additional engine-hook DLL because it is outside the sealed retail
// compatibility proof.  The fixture may acknowledge only this complete,
// versioned warning and its unique native first-button OK tile.
inline constexpr std::string_view TtwStewieDependencyWarningTitle =
    "Missing/outdated dependency!";
inline constexpr std::string_view TtwStewieDependencyWarningBody =
    "TTW has detected that lStewieAl's Tweaks and Engine Fixes is missing or "
    "you are using a version older than 9.41.    Please follow the Best of "
    "Times guide for guidance on how to install it.";
inline constexpr std::uint32_t
    MaxExactTtwStewieDependencyCloseAttemptsPerRun = 1u;

enum class Action : std::uint8_t
{
    None = 0u,
    Create,
    Load,
};

enum class Trait : std::uint8_t
{
    None = 0u,
    BuiltToDestroy,
    FastShot,
    FourEyes,
    GoodNatured,
    HeavyHanded,
    Kamikaze,
    SmallFrame,
    TriggerDiscipline,
    WildWasteland,
};

struct TraitCommand
{
    Trait trait = Trait::None;
    std::string_view token {};
    // Each command is a retail-owned FalloutNV.esm editor ID. The game uses
    // the same editor IDs in its own trait-removal script.
    std::string_view addPerkCommand {};
};

inline constexpr TraitCommand TraitCommands[] = {
    { Trait::None, "None", {} },
    { Trait::BuiltToDestroy, "BuiltToDestroy", "player.AddPerk BuiltToDestroy" },
    { Trait::FastShot, "FastShot", "player.AddPerk FastShot" },
    { Trait::FourEyes, "FourEyes", "player.AddPerk FourEyes" },
    { Trait::GoodNatured, "GoodNatured", "player.AddPerk GoodNatured" },
    { Trait::HeavyHanded, "HeavyHanded", "player.AddPerk HeavyHanded" },
    { Trait::Kamikaze, "Kamikaze", "player.AddPerk Kamikaze" },
    { Trait::SmallFrame, "SmallFrame", "player.AddPerk SmallFrame" },
    { Trait::TriggerDiscipline, "TriggerDiscipline", "player.AddPerk TriggerDiscipline" },
    { Trait::WildWasteland, "WildWasteland", "player.AddPerk WildWasteland" },
};

constexpr const TraitCommand* findTrait(std::string_view token) noexcept
{
    for (const TraitCommand& candidate : TraitCommands)
    {
        if (candidate.token == token)
            return &candidate;
    }
    return nullptr;
}

constexpr const TraitCommand* findTrait(Trait trait) noexcept
{
    for (const TraitCommand& candidate : TraitCommands)
    {
        if (candidate.trait == trait)
            return &candidate;
    }
    return nullptr;
}

constexpr bool isFixtureSaveNameCharacter(char value) noexcept
{
    return (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z')
        || (value >= '0' && value <= '9')
        || value == '_';
}

constexpr bool hasPrefix(
    std::string_view value,
    std::string_view prefix) noexcept
{
    if (value.size() < prefix.size())
        return false;
    for (std::size_t index = 0u; index < prefix.size(); ++index)
    {
        if (value[index] != prefix[index])
            return false;
    }
    return true;
}

// The save-name field is 64 bytes including its terminal NUL in the shared
// command mapping. Keep room for that terminator and reject paths, spaces, and
// arbitrary existing save names before the plugin ever constructs a command.
constexpr bool isOwnedFixtureSaveName(std::string_view value) noexcept
{
    const std::string_view prefix = hasPrefix(value, SaveNamePrefix)
        ? SaveNamePrefix
        : (hasPrefix(value, TtwSaveNamePrefix) ? TtwSaveNamePrefix
                                                : std::string_view {});
    if (prefix.empty() || value.size() <= prefix.size() || value.size() >= 64u)
    {
        return false;
    }

    for (std::size_t index = prefix.size(); index < value.size(); ++index)
    {
        if (!isFixtureSaveNameCharacter(value[index]))
            return false;
    }
    return true;
}

struct Plan
{
    Action action = Action::None;
    Trait firstTrait = Trait::None;
    Trait secondTrait = Trait::None;
    std::string_view saveName {};
};

enum class Failure : std::uint8_t
{
    None = 0u,
    ActionRequired,
    SaveNameNotOwned,
    TraitUnknown,
    DuplicateTrait,
};

constexpr Failure validate(const Plan& plan) noexcept
{
    if (plan.action != Action::Create && plan.action != Action::Load)
        return Failure::ActionRequired;
    if (!isOwnedFixtureSaveName(plan.saveName))
        return Failure::SaveNameNotOwned;

    const TraitCommand* const first = findTrait(plan.firstTrait);
    const TraitCommand* const second = findTrait(plan.secondTrait);
    if (first == nullptr || first->trait != plan.firstTrait
        || second == nullptr || second->trait != plan.secondTrait)
    {
        return Failure::TraitUnknown;
    }
    if (plan.firstTrait != Trait::None
        && plan.firstTrait == plan.secondTrait)
    {
        return Failure::DuplicateTrait;
    }
    return Failure::None;
}

constexpr bool authorized(const Plan& plan) noexcept
{
    return validate(plan) == Failure::None;
}

// This is not general menu authority. The caller must independently prove one
// known official-pack title/body pair and exactly one visible first-button OK
// tile before it can submit the one fixed stock close command.
constexpr bool exactOfficialPackCloseAuthorized(
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

// This gate is deliberately distinct from the official-pack matcher.  It
// grants one native acknowledgement/close only when the complete TTW title
// and body above and exactly one first-button OK tile are independently
// observed in the owned fixture process.
constexpr bool exactTtwStewieDependencyAcknowledgementAuthorized(
    bool explicitlyOptedIn,
    bool visibleMessageMenu,
    bool exactTitleObserved,
    bool exactBodyObserved,
    bool exactlyOneFirstButtonOk,
    bool alreadyAttempted) noexcept
{
    return explicitlyOptedIn
        && visibleMessageMenu
        && exactTitleObserved
        && exactBodyObserved
        && exactlyOneFirstButtonOk
        && !alreadyAttempted;
}

constexpr std::string_view addPerkCommand(Trait trait) noexcept
{
    for (const TraitCommand& candidate : TraitCommands)
    {
        if (candidate.trait == trait)
            return candidate.addPerkCommand;
    }
    return {};
}
}
