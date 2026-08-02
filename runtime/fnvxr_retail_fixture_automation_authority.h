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
// JIP LN NVSE's exact actor command used only to make the named, already
// equipped fixture weapon visible after the fixture has reached clean
// gameplay.  It is fixed here rather than derived from launcher input.
inline constexpr std::string_view SetFixtureWeaponOutCommand =
    "player.SetWeaponOut 1";
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

// The weapon fixture is intentionally a finite stock table, not a generic
// console-command input.  Each row is a base FalloutNV.esm form that can be
// created in an owned level-one save and later loaded without touching a
// player's inventory or save lineage.
enum class Weapon : std::uint8_t
{
    None = 0u,
    Pistol,
    RifleSingleHand,
    RifleTwoHand,
    Minigun,
    FragGrenade,
    Knife,
    ThrowingKnife,
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

struct WeaponCommand
{
    Weapon weapon = Weapon::None;
    std::string_view token {};
    std::string_view addWeaponCommand {};
    std::string_view addAmmoCommand {};
    std::string_view equipCommand {};
};

inline constexpr WeaponCommand WeaponCommands[] = {
    { Weapon::None, "None", {}, {}, {} },
    // 000E3778 WeapNV9mmPistol; 0008ED03 Ammo9mm
    { Weapon::Pistol, "Pistol", "player.additem 000E3778 1",
        "player.additem 0008ED03 120", "player.equipitem 000E3778" },
    // 000E9C3B WeapNVServiceRifle; 00004240 Ammo556mm
    { Weapon::RifleSingleHand, "RifleSingleHand",
        "player.additem 000E9C3B 1", "player.additem 00004240 120",
        "player.equipitem 000E9C3B" },
    // 000CD53A WeapCaravanShotgun.  The visibility fixture does not fire,
    // so no ammunition is needed for the two-hand long-gun render check.
    { Weapon::RifleTwoHand, "RifleTwoHand", "player.additem 000CD53A 1",
        {}, "player.equipitem 000CD53A" },
    // 0000433F WeapMinigun; 0006B53D Ammo5mm
    { Weapon::Minigun, "Minigun", "player.additem 0000433F 1",
        "player.additem 0006B53D 240", "player.equipitem 0000433F" },
    // 00004330 WeapGrenadeFrag
    { Weapon::FragGrenade, "FragGrenade", "player.additem 00004330 12",
        {}, "player.equipitem 00004330" },
    // 00004334 WeapKnife
    { Weapon::Knife, "Knife", "player.additem 00004334 1", {},
        "player.equipitem 00004334" },
    // 00161246 WeapNVThrowingKnife
    { Weapon::ThrowingKnife, "ThrowingKnife",
        "player.additem 00161246 12", {}, "player.equipitem 00161246" },
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

constexpr const WeaponCommand* findWeapon(std::string_view token) noexcept
{
    for (const WeaponCommand& candidate : WeaponCommands)
    {
        if (candidate.token == token)
            return &candidate;
    }
    return nullptr;
}

constexpr const WeaponCommand* findWeapon(Weapon weapon) noexcept
{
    for (const WeaponCommand& candidate : WeaponCommands)
    {
        if (candidate.weapon == weapon)
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
    Weapon weapon = Weapon::None;
    std::string_view saveName {};
};

enum class Failure : std::uint8_t
{
    None = 0u,
    ActionRequired,
    SaveNameNotOwned,
    TraitUnknown,
    DuplicateTrait,
    WeaponUnknown,
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
    const WeaponCommand* const weapon = findWeapon(plan.weapon);
    if (weapon == nullptr || weapon->weapon != plan.weapon)
        return Failure::WeaponUnknown;
    return Failure::None;
}

constexpr bool authorized(const Plan& plan) noexcept
{
    return validate(plan) == Failure::None;
}

// A world-only capture may finalize one owned loaded fixture after its stock
// notices have cleared.  This permits one exact save to that same owned name;
// it is not an authority to save arbitrary games or mutate a personal save.
constexpr bool headsetWorldOnlyFixturePreparationSaveAuthorized(
    const Plan& plan,
    bool explicitlyRequested,
    bool fixtureReady,
    bool gameplay,
    bool playerProcessAvailable,
    bool alreadySaved) noexcept
{
    return explicitlyRequested
        && fixtureReady
        && authorized(plan)
        && plan.action == Action::Load
        && plan.weapon != Weapon::None
        && gameplay
        && playerProcessAvailable
        && !alreadySaved;
}

// After that exact owned save has settled, a world-only capture may issue the
// fixed JIP SetWeaponOut command for the named stock weapon already equipped
// in the fixture.  This is not a general input or command grant: it excludes
// creates, unarmed fixtures, menus, missing player state, and weapons already
// out.
constexpr bool headsetWorldOnlyFixtureWeaponDrawAuthorized(
    const Plan& plan,
    bool explicitlyRequested,
    bool fixtureReady,
    bool gameplay,
    bool playerProcessAvailable,
    bool weaponOut) noexcept
{
    return explicitlyRequested
        && fixtureReady
        && authorized(plan)
        && plan.action == Action::Load
        && plan.weapon != Weapon::None
        && gameplay
        && playerProcessAvailable
        && !weaponOut;
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

constexpr std::string_view addWeaponCommand(Weapon weapon) noexcept
{
    const WeaponCommand* const command = findWeapon(weapon);
    return command ? command->addWeaponCommand : std::string_view {};
}

constexpr std::string_view addWeaponAmmoCommand(Weapon weapon) noexcept
{
    const WeaponCommand* const command = findWeapon(weapon);
    return command ? command->addAmmoCommand : std::string_view {};
}

constexpr std::string_view equipWeaponCommand(Weapon weapon) noexcept
{
    const WeaponCommand* const command = findWeapon(weapon);
    return command ? command->equipCommand : std::string_view {};
}
}
