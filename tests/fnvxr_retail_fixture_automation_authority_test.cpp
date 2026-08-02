#include "fnvxr_retail_fixture_automation_authority.h"

#include <cstdlib>
#include <iostream>

namespace
{
namespace fixture = fnvxr::engine::retail_fixture_automation;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using fixture::Action;
    using fixture::Failure;
    using fixture::Trait;
    using fixture::Weapon;

    static_assert(fixture::isOwnedFixtureSaveName(
        "FNVXR_AutoRetail_L1_Base"));
    static_assert(fixture::isOwnedFixtureSaveName(
        "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured"));
    static_assert(fixture::isOwnedFixtureSaveName(
        "FNVXR_AutoTTW_L1_FastShot_WildWasteland"));
    static_assert(!fixture::isOwnedFixtureSaveName("FNVXR_StereoTest"));
    static_assert(!fixture::isOwnedFixtureSaveName("FNVXR_AutoRetail_"));
    static_assert(!fixture::isOwnedFixtureSaveName(
        "FNVXR_AutoRetail_.._outside"));
    static_assert(fixture::findTrait("BuiltToDestroy") != nullptr);
    static_assert(fixture::findTrait("GoodNatured") != nullptr);
    static_assert(fixture::findTrait("NotATrait") == nullptr);
    static_assert(fixture::addPerkCommand(Trait::BuiltToDestroy)
        == "player.AddPerk BuiltToDestroy");
    static_assert(fixture::addPerkCommand(Trait::GoodNatured)
        == "player.AddPerk GoodNatured");
    static_assert(fixture::findWeapon("Pistol") != nullptr);
    static_assert(fixture::findWeapon("RifleSingleHand") != nullptr);
    static_assert(fixture::findWeapon("RifleTwoHand") != nullptr);
    static_assert(fixture::findWeapon("Minigun") != nullptr);
    static_assert(fixture::findWeapon("FragGrenade") != nullptr);
    static_assert(fixture::findWeapon("Knife") != nullptr);
    static_assert(fixture::findWeapon("ThrowingKnife") != nullptr);
    static_assert(fixture::findWeapon("NotAWeapon") == nullptr);
    static_assert(fixture::addWeaponCommand(Weapon::Pistol)
        == "player.additem 000E3778 1");
    static_assert(fixture::addWeaponAmmoCommand(Weapon::Pistol)
        == "player.additem 0008ED03 120");
    static_assert(fixture::equipWeaponCommand(Weapon::Pistol)
        == "player.equipitem 000E3778");
    static_assert(fixture::addWeaponCommand(Weapon::Minigun)
        == "player.additem 0000433F 1");
    static_assert(fixture::equipWeaponCommand(Weapon::ThrowingKnife)
        == "player.equipitem 00161246");
    static_assert(fixture::CloseExactOfficialPackMessageCommand
        == "CloseAllMenus");
    static_assert(fixture::MaxExactOfficialPackCloseAttemptsPerRun == 8u);
    static_assert(fixture::TtwStewieDependencyWarningTitle
        == "Missing/outdated dependency!");
    static_assert(fixture::TtwStewieDependencyWarningBody
        == "TTW has detected that lStewieAl's Tweaks and Engine Fixes is "
           "missing or you are using a version older than 9.41.    Please "
           "follow the Best of Times guide for guidance on how to install it.");
    static_assert(
        fixture::MaxExactTtwStewieDependencyCloseAttemptsPerRun == 1u);
    static_assert(fixture::exactOfficialPackCloseAuthorized(
        true, true, true, true, false));
    static_assert(!fixture::exactOfficialPackCloseAuthorized(
        false, true, true, true, false));
    static_assert(!fixture::exactOfficialPackCloseAuthorized(
        true, true, true, true, true));
    static_assert(
        fixture::exactTtwStewieDependencyAcknowledgementAuthorized(
            true, true, true, true, true, false));
    static_assert(
        !fixture::exactTtwStewieDependencyAcknowledgementAuthorized(
            false, true, true, true, true, false));
    static_assert(
        !fixture::exactTtwStewieDependencyAcknowledgementAuthorized(
            true, true, false, true, true, false));
    static_assert(
        !fixture::exactTtwStewieDependencyAcknowledgementAuthorized(
            true, true, true, true, true, true));

    fixture::Plan create {
        Action::Create,
        Trait::BuiltToDestroy,
        Trait::GoodNatured,
        Weapon::None,
        "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured",
    };
    require(fixture::authorized(create),
        "valid owned two-trait fixture was rejected");

    fixture::Plan load {
        Action::Load,
        Trait::None,
        Trait::None,
        Weapon::None,
        "FNVXR_AutoRetail_L1_Base",
    };
    require(fixture::authorized(load),
        "valid owned base fixture load was rejected");

    fixture::Plan ttwLoad {
        Action::Load,
        Trait::FastShot,
        Trait::WildWasteland,
        Weapon::None,
        "FNVXR_AutoTTW_L1_FastShot_WildWasteland",
    };
    require(fixture::authorized(ttwLoad),
        "valid owned TTW fixture load was rejected");

    fixture::Plan pistolCreate {
        Action::Create,
        Trait::None,
        Trait::None,
        Weapon::Pistol,
        "FNVXR_AutoRetail_L1_Pistol",
    };
    require(fixture::authorized(pistolCreate),
        "valid owned pistol fixture was rejected");

    fixture::Plan pistolLoad {
        Action::Load,
        Trait::None,
        Trait::None,
        Weapon::Pistol,
        "FNVXR_AutoRetail_L1_Pistol",
    };
    require(fixture::SetFixtureWeaponOutCommand == "player.SetWeaponOut 1",
        "fixture weapon command was not the fixed JIP draw command");
    require(fixture::headsetWorldOnlyFixturePreparationSaveAuthorized(
        pistolLoad, true, true, true, true, false),
        "owned pistol load was not authorized for its one final fixture save");
    require(!fixture::headsetWorldOnlyFixturePreparationSaveAuthorized(
        pistolLoad, true, true, true, true, true),
        "fixture final save was admitted more than once");
    require(!fixture::headsetWorldOnlyFixturePreparationSaveAuthorized(
        pistolCreate, true, true, true, true, false),
        "fixture final save was admitted while creating a fixture");
    require(fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        pistolLoad, true, true, true, true, false),
        "owned holstered pistol load was not authorized for its one draw");
    require(!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        pistolLoad, false, true, true, true, false),
        "weapon draw was admitted without the explicit request");
    require(!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        pistolLoad, true, false, true, true, false),
        "weapon draw was admitted before the fixture was ready");
    require(!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        pistolLoad, true, true, false, true, false),
        "weapon draw was admitted outside gameplay");
    require(!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        pistolLoad, true, true, true, false, false),
        "weapon draw was admitted without the player process");
    require(!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        pistolLoad, true, true, true, true, true),
        "weapon draw was admitted for an already-ready weapon");
    require(!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        pistolCreate, true, true, true, true, false),
        "weapon draw was admitted while creating a fixture");

    fixture::Plan unarmedLoad = pistolLoad;
    unarmedLoad.weapon = Weapon::None;
    require(!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
        unarmedLoad, true, true, true, true, false),
        "weapon draw was admitted for an unarmed fixture");

    fixture::Plan personalSave = create;
    personalSave.saveName = "FNVXR_StereoTest";
    require(fixture::validate(personalSave) == Failure::SaveNameNotOwned,
        "fixture authority admitted a historical save name");

    fixture::Plan duplicateTrait = create;
    duplicateTrait.secondTrait = Trait::BuiltToDestroy;
    require(fixture::validate(duplicateTrait) == Failure::DuplicateTrait,
        "fixture authority admitted a duplicate trait");

    fixture::Plan noAction = create;
    noAction.action = Action::None;
    require(fixture::validate(noAction) == Failure::ActionRequired,
        "fixture authority admitted a missing action");

    fixture::Plan invalidTrait = create;
    invalidTrait.firstTrait = static_cast<Trait>(0xffu);
    require(fixture::validate(invalidTrait) == Failure::TraitUnknown,
        "fixture authority admitted an unknown trait enum");

    fixture::Plan invalidWeapon = pistolCreate;
    invalidWeapon.weapon = static_cast<Weapon>(0xffu);
    require(fixture::validate(invalidWeapon) == Failure::WeaponUnknown,
        "fixture authority admitted an unknown weapon enum");

    std::cout << "retail fixture automation authority gate passed\n";
    return EXIT_SUCCESS;
}
