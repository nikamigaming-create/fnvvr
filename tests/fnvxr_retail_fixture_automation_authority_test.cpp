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
    static_assert(fixture::CloseExactOfficialPackMessageCommand
        == "CloseAllMenus");
    static_assert(fixture::MaxExactOfficialPackCloseAttemptsPerRun == 8u);
    static_assert(fixture::exactOfficialPackCloseAuthorized(
        true, true, true, true, false));
    static_assert(!fixture::exactOfficialPackCloseAuthorized(
        false, true, true, true, false));
    static_assert(!fixture::exactOfficialPackCloseAuthorized(
        true, true, true, true, true));

    fixture::Plan create {
        Action::Create,
        Trait::BuiltToDestroy,
        Trait::GoodNatured,
        "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured",
    };
    require(fixture::authorized(create),
        "valid owned two-trait fixture was rejected");

    fixture::Plan load {
        Action::Load,
        Trait::None,
        Trait::None,
        "FNVXR_AutoRetail_L1_Base",
    };
    require(fixture::authorized(load),
        "valid owned base fixture load was rejected");

    fixture::Plan ttwLoad {
        Action::Load,
        Trait::FastShot,
        Trait::WildWasteland,
        "FNVXR_AutoTTW_L1_FastShot_WildWasteland",
    };
    require(fixture::authorized(ttwLoad),
        "valid owned TTW fixture load was rejected");

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

    std::cout << "retail fixture automation authority gate passed\n";
    return EXIT_SUCCESS;
}
